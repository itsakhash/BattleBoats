/**
 * @file    Agent.c
 *
 * Implements the BattleBoats Agent state machine: turn-order
 * negotiation (via the Negotiation module) and gameplay (via the
 * Field module). Mirrors the state machine diagram in the Lab 10
 * manual exactly, including the documented shortcut where an
 * ACCEPTING agent that wins the coin flip transitions directly into
 * ATTACKING instead of passing through WAITING_TO_SEND.
 *
 * @author  Akhash Arjundayal (aarjunda)
 */
#include <stdlib.h>
#include "Agent.h"
#include "Negotiation.h"
#include "Field.h"
#include "FieldOled.h" // Also pulls in Oled.h for text messages.

/*  MODULE-LEVEL STATE  */

static AgentState state;
static uint8_t turnNumber;
static uint8_t gameInProgress; // TRUE once FieldInit()+boats have been placed for this game.

// Both players' boards.
static Field ownField;
static Field oppField;

// Negotiation data. Only the variables relevant to our current role
// (challenger vs. acceptor) will actually be populated in a given game.
static NegotiationData myA;        // Challenger: our secret number.
static NegotiationData myHashA;    // Challenger: hash of myA, sent in CHA.
static NegotiationData theirB;     // Challenger: B received via ACC.
static NegotiationData myB;        // Acceptor: our random number, sent in ACC.
static NegotiationData theirHashA; // Acceptor: hash_a received via CHA (to verify later).
static NegotiationData theirA;     // Acceptor: A received via REV.

// Tracks the coordinates of the shot we most recently sent, so that
// when the RES arrives we know which square of oppField to update.
static GuessData lastGuessSent;

/*  DISPLAY HELPERS  */

/** _AgentDisplayMessage(msg)
 * Show a one-off text message (start screen, victory/defeat, errors).
 */
static void _AgentDisplayMessage(const char *msg)
{
    OLED_Clear(OLED_COLOR_BLACK);
    OLED_DrawString(msg);
    OLED_Update();
}

/** _AgentGetTurnIndicator()
 * Maps the current state to a FieldOledTurn value for the board display.
 */
static FieldOledTurn _AgentGetTurnIndicator(void)
{
    switch (state)
    {
    case AGENT_STATE_ATTACKING:
    case AGENT_STATE_WAITING_TO_SEND:
        return FIELD_OLED_TURN_MINE;
    case AGENT_STATE_DEFENDING:
        return FIELD_OLED_TURN_THEIRS;
    default:
        return FIELD_OLED_TURN_NONE;
    }
}

/** _AgentUpdateBoardDisplay()
 * Redraws both fields on the OLED. Only meaningful once a game has
 * actually started (FieldInit() + boat placement have occurred).
 */
static void _AgentUpdateBoardDisplay(void)
{
    if (gameInProgress)
    {
        FieldOledDrawScreen(&ownField, &oppField, _AgentGetTurnIndicator(), turnNumber);
    }
}

/*  REQUIRED INTERFACE FUNCTIONS  */

/** AgentInit()
 * Reset the state machine to its initial condition. Does NOT place
 * boats or touch the fields yet -- that only happens once a game is
 * actually started (START_BUTTON or CHA_RECEIVED), per the state
 * diagram.
 */
void AgentInit(void)
{
    state = AGENT_STATE_START;
    turnNumber = 0;
    gameInProgress = 0;

    myA = 0;
    myHashA = 0;
    theirB = 0;
    myB = 0;
    theirHashA = 0;
    theirA = 0;

    _AgentDisplayMessage("BattleBoats! Press BTN4 to challenge.");
}

/** AgentGetState() / AgentSetState()
 * Trivial accessors, primarily useful for AgentTest.c.
 */
AgentState AgentGetState(void)
{
    return state;
}

void AgentSetState(AgentState newState)
{
    state = newState;
}

/** AgentRun(event)
 * Evolves the state machine by exactly one event, per the diagram in
 * the lab manual. Returns a Message to be sent (or MESSAGE_NONE).
 */
Message AgentRun(BB_Event event)
{
    Message outMsg = {MESSAGE_NONE, 0, 0, 0};

    /* ------------------------------------------------------------
     * Global transitions: RESET_BUTTON and ERROR apply from any
     * state, per the "(from any state)" annotations in the diagram.
     * ---------------------------------------------------------- */
    if (event.type == BB_EVENT_RESET_BUTTON)
    {
        AgentInit();
        return outMsg;
    }

    if (event.type == BB_EVENT_ERROR)
    {
        // We are not required to recover -- just report and stop.
        // This also covers "opponent fails to uphold the protocol."
        _AgentDisplayMessage("ERROR: bad transmission or protocol violation.");
        state = AGENT_STATE_END_SCREEN;
        return outMsg;
    }

    /* ------------------------------------------------------------
     * Per-state transitions.
     * ---------------------------------------------------------- */
    switch (state)
    {

    case AGENT_STATE_START:
        if (event.type == BB_EVENT_START_BUTTON)
        {
            // We are the CHALLENGER.
            myA = (NegotiationData)rand();
            myHashA = NegotiationHash(myA);

            FieldInit(&ownField, &oppField);
            FieldAIPlaceAllBoats(&ownField);
            gameInProgress = 1;
            turnNumber = 0;

            outMsg.type = MESSAGE_CHA;
            outMsg.param0 = myHashA;

            state = AGENT_STATE_CHALLENGING;
        }
        else if (event.type == BB_EVENT_CHA_RECEIVED)
        {
            // We are the ACCEPTOR.
            theirHashA = event.param0;
            myB = (NegotiationData)rand();

            FieldInit(&ownField, &oppField);
            FieldAIPlaceAllBoats(&ownField);
            gameInProgress = 1;
            turnNumber = 0;

            outMsg.type = MESSAGE_ACC;
            outMsg.param0 = myB;

            state = AGENT_STATE_ACCEPTING;
        }
        break;

    case AGENT_STATE_CHALLENGING:
        if (event.type == BB_EVENT_ACC_RECEIVED)
        {
            theirB = event.param0;

            outMsg.type = MESSAGE_REV;
            outMsg.param0 = myA;

            if (NegotiateCoinFlip(myA, theirB) == HEADS)
            {
                // Challenger wins the flip on HEADS -> goes first,
                // but must wait for REV to finish sending first.
                state = AGENT_STATE_WAITING_TO_SEND;
            }
            else
            {
                state = AGENT_STATE_DEFENDING;
            }
        }
        break;

    case AGENT_STATE_ACCEPTING:
        if (event.type == BB_EVENT_REV_RECEIVED)
        {
            theirA = event.param0;

            if (!NegotiationVerify(theirA, theirHashA))
            {
                // Challenger cheated: revealed A doesn't match
                // the commitment they originally sent.
                _AgentDisplayMessage("Cheating detected by opponent!");
                state = AGENT_STATE_END_SCREEN;
                break;
            }

            if (NegotiateCoinFlip(theirA, myB) == TAILS)
            {
                // Acceptor wins the flip on TAILS -> goes first.
                // NOTE: per the manual, this transitions directly
                // into ATTACKING (skipping WAITING_TO_SEND).
                GuessData guess = FieldAIDecideGuess(&oppField);
                lastGuessSent = guess;

                outMsg.type = MESSAGE_SHO;
                outMsg.param0 = guess.row;
                outMsg.param1 = guess.col;

                state = AGENT_STATE_ATTACKING;
            }
            else
            {
                state = AGENT_STATE_DEFENDING;
            }
        }
        break;

    case AGENT_STATE_WAITING_TO_SEND:
        if (event.type == BB_EVENT_MESSAGE_SENT)
        {
            turnNumber++;

            GuessData guess = FieldAIDecideGuess(&oppField);
            lastGuessSent = guess;

            outMsg.type = MESSAGE_SHO;
            outMsg.param0 = guess.row;
            outMsg.param1 = guess.col;

            state = AGENT_STATE_ATTACKING;
        }
        break;

    case AGENT_STATE_ATTACKING:
        if (event.type == BB_EVENT_RES_RECEIVED)
        {
            GuessData resultData;
            resultData.row = event.param0;
            resultData.col = event.param1;
            resultData.result = (ShotResult)event.param2;

            FieldUpdateKnowledge(&oppField, &resultData);

            if (FieldGetBoatStates(&oppField) == 0)
            {
                _AgentDisplayMessage("VICTORY! You sank all enemy boats.");
                state = AGENT_STATE_END_SCREEN;
            }
            else
            {
                state = AGENT_STATE_DEFENDING;
            }
        }
        break;

    case AGENT_STATE_DEFENDING:
        if (event.type == BB_EVENT_SHO_RECEIVED)
        {
            GuessData incoming;
            incoming.row = event.param0;
            incoming.col = event.param1;

            FieldRegisterEnemyAttack(&ownField, &incoming);

            outMsg.type = MESSAGE_RES;
            outMsg.param0 = incoming.row;
            outMsg.param1 = incoming.col;
            outMsg.param2 = incoming.result;

            if (FieldGetBoatStates(&ownField) == 0)
            {
                _AgentDisplayMessage("DEFEAT! All your boats were sunk.");
                state = AGENT_STATE_END_SCREEN;
            }
            else
            {
                state = AGENT_STATE_WAITING_TO_SEND;
            }
        }
        break;

    case AGENT_STATE_END_SCREEN:
        // Only RESET_BUTTON (handled globally above) leaves this state.
        break;

    case AGENT_STATE_SETUP_BOATS:
        // Not used by the AI agent -- reserved for HumanAgent.c.
        break;

    default:
        break;
    }

    // Refresh the OLED board view for any gameplay state. START (pre-
    // negotiation) and END_SCREEN/ERROR already show their own one-off
    // text messages above and are intentionally not overwritten here.
    if (state == AGENT_STATE_CHALLENGING ||
        state == AGENT_STATE_ACCEPTING ||
        state == AGENT_STATE_ATTACKING ||
        state == AGENT_STATE_DEFENDING ||
        state == AGENT_STATE_WAITING_TO_SEND)
    {
        _AgentUpdateBoardDisplay();
    }

    return outMsg;
}