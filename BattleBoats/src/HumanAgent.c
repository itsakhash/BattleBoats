/**
 * @file    HumanAgent.c
 *
 * Extra credit: a human-controlled BattleBoats agent. Uses the same
 * negotiation logic as Agent.c, but replaces the automatic AI boat
 * placement and guessing with button-driven human interaction.
 *
 * Two deviations from the AI Agent's state diagram, both called out
 * in the lab manual:
 *   1. A new SETUP_BOATS state is inserted before negotiation begins,
 *      so the human can place their own boats using the buttons.
 *   2. ACCEPTING transitions to WAITING_TO_SEND (not directly to
 *      ATTACKING) even on a winning coin flip, so the human gets a
 *      chance to pick their first target.
 * A third change specific to this file: WAITING_TO_SEND is repurposed
 * as a "choose your target" state -- the human moves a cursor over
 * the opponent's grid and presses BTN4 to fire, rather than firing
 * automatically on MESSAGE_SENT.
 *
 * Known limitation: the boat orientation preview during SETUP_BOATS
 * only updates when a button is pressed (moving the cursor), since
 * this is an event-driven system and flipping SW1 alone does not
 * generate an event. In practice this means the switch must be set
 * BEFORE moving the cursor to the desired square, rather than being
 * toggled freely at any moment with an instantly-live preview.
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
static uint8_t gameInProgress;

static Field ownField;
static Field oppField;

// Negotiation data -- same roles/meanings as in Agent.c.
static NegotiationData myA;
static NegotiationData myHashA;
static NegotiationData theirB;
static NegotiationData myB;
static NegotiationData theirHashA;
static NegotiationData theirA;

// Which role we're playing this game, decided at the START state and
// used once boat setup finishes to decide whether to send CHA or ACC.
typedef enum
{
    ROLE_NONE,
    ROLE_CHALLENGER,
    ROLE_ACCEPTOR
} AgentRole;
static AgentRole pendingRole;

// Boat-setup bookkeeping.
static uint8_t setupRow, setupCol;
static BoatDirection setupDir;
static BoatType boatToPlace;

// Target-selection cursor, reused for the WAITING_TO_SEND "choose your
// target" state (never active at the same time as boat setup).
static uint8_t targetRow, targetCol;

// Tracks whether the one-time on-screen instructions for each phase
// have already been shown, so they don't reappear every redraw.
static uint8_t setupInstructionsShown;
static uint8_t targetInstructionsShown;

// Boat sizes indexed by BoatType, matching FIELD_BOAT_SIZE_* from Field.h.
// Used to preview the full boat footprint during placement.
static const uint8_t boatSizes[FIELD_NUM_BOATS] = {
    FIELD_BOAT_SIZE_SMALL,
    FIELD_BOAT_SIZE_MEDIUM,
    FIELD_BOAT_SIZE_LARGE,
    FIELD_BOAT_SIZE_HUGE};

// Button debounce: a real mechanical bounce can generate several
// events for a single physical press within a few milliseconds.
// We reject a button event if it's the SAME button type as the last
// one we actually accepted and arrives within this window -- but we
// only update the "last accepted" timestamp on an ACCEPTED event, so
// a later, deliberate press of the same button still registers.
#define BUTTON_DEBOUNCE_MS 100
static BB_EventType lastButtonEventType;
static uint32_t lastButtonEventTime;

/*  DISPLAY HELPERS  */

/** _ReadOrientationSwitch()
 * Reads SW1 (from BOARD.h) to decide boat orientation: one position
 * places horizontally (EAST), the other vertically (SOUTH). If the
 * physical positions feel backwards, just swap the two branches below.
 */
static BoatDirection _ReadOrientationSwitch(void)
{
    return SW1_STATE() ? FIELD_DIR_SOUTH : FIELD_DIR_EAST;
}

/** _IsButtonEvent(type)
 * True for the four physical-button event types, which are subject
 * to mechanical bounce. Protocol events (CHA_RECEIVED, SHO_RECEIVED,
 * MESSAGE_SENT, etc.) are never bounced and always pass through.
 */
static uint8_t _IsButtonEvent(BB_EventType type)
{
    return (type == BB_EVENT_START_BUTTON || type == BB_EVENT_EAST_BUTTON ||
            type == BB_EVENT_SOUTH_BUTTON || type == BB_EVENT_RESET_BUTTON);
}

static void _AgentDisplayMessage(const char *msg)
{
    OLED_Clear(OLED_COLOR_BLACK);
    OLED_DrawString(msg);
    OLED_Update();
}

/** _DrawFieldWithCursor(realField, cursorRow, cursorCol)
 * Draws a copy of realField with FIELD_SQUARE_CURSOR overlaid at the
 * given position, WITHOUT modifying the real field's actual data.
 */
static Field _DrawFieldWithCursor(const Field *realField, uint8_t cursorRow, uint8_t cursorCol)
{
    Field displayCopy = *realField;
    FieldSetSquareStatus(&displayCopy, cursorRow, cursorCol, FIELD_SQUARE_CURSOR);
    return displayCopy;
}

/** _DrawFieldWithBoatPreview(realField, row, col, dir, boatType)
 * Like _DrawFieldWithCursor(), but overlays the CURSOR marker across
 * every square the boat would occupy at its current position and
 * orientation -- a live preview of where it will land. Squares that
 * would fall off the edge of the grid are silently skipped (bounds
 * checking is handled by FieldSetSquareStatus), which doubles as a
 * visual cue that the boat won't fit there.
 */
static Field _DrawFieldWithBoatPreview(const Field *realField, uint8_t row, uint8_t col,
                                       BoatDirection dir, BoatType boatType)
{
    Field displayCopy = *realField;
    uint8_t size = boatSizes[boatType];

    for (uint8_t i = 0; i < size; i++)
    {
        uint8_t r = (dir == FIELD_DIR_SOUTH) ? (row + i) : row;
        uint8_t c = (dir == FIELD_DIR_EAST) ? (col + i) : col;
        FieldSetSquareStatus(&displayCopy, r, c, FIELD_SQUARE_CURSOR);
    }
    return displayCopy;
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
 * Redraws the board for the current state. During SETUP_BOATS, shows
 * only our own field (with a placement cursor) -- theirField is NULL,
 * exactly as FieldOled.h documents for this situation. During
 * WAITING_TO_SEND, shows our own field normally plus a cursor overlay
 * on our knowledge of the opponent's field. All other gameplay states
 * show both fields plainly, same as Agent.c.
 */
static void _AgentUpdateBoardDisplay(void)
{
    if (!gameInProgress)
        return;

    if (state == AGENT_STATE_SETUP_BOATS)
    {
        if (!setupInstructionsShown)
        {
            _AgentDisplayMessage("B2:Down B3:Right\nB4:Place\nSW1:Orientation\nPress B4 to continue");
            return;
        }
        Field ownWithCursor = _DrawFieldWithBoatPreview(&ownField, setupRow, setupCol,
                                                        _ReadOrientationSwitch(), boatToPlace);
        FieldOledDrawScreen(&ownWithCursor, NULL, FIELD_OLED_TURN_NONE, turnNumber);
    }
    else if (state == AGENT_STATE_WAITING_TO_SEND)
    {
        if (!targetInstructionsShown)
        {
            _AgentDisplayMessage("B2:Down\nB3:Right\nB4:Fire\n\nPress BTN4\nto continue");
            return;
        }
        Field oppWithCursor = _DrawFieldWithCursor(&oppField, targetRow, targetCol);
        FieldOledDrawScreen(&ownField, &oppWithCursor, _AgentGetTurnIndicator(), turnNumber);
    }
    else if (state == AGENT_STATE_CHALLENGING || state == AGENT_STATE_ACCEPTING ||
             state == AGENT_STATE_ATTACKING || state == AGENT_STATE_DEFENDING)
    {
        FieldOledDrawScreen(&ownField, &oppField, _AgentGetTurnIndicator(), turnNumber);
    }
}

/*  REQUIRED INTERFACE FUNCTIONS  */

void AgentInit(void)
{
    state = AGENT_STATE_START;
    turnNumber = 0;
    gameInProgress = 0;
    pendingRole = ROLE_NONE;

    myA = 0;
    myHashA = 0;
    theirB = 0;
    myB = 0;
    theirHashA = 0;
    theirA = 0;

    setupRow = 0;
    setupCol = 0;
    setupDir = FIELD_DIR_SOUTH;
    boatToPlace = FIELD_BOAT_TYPE_SMALL;
    targetRow = 0;
    targetCol = 0;
    setupInstructionsShown = 0;
    targetInstructionsShown = 0;
    lastButtonEventType = BB_EVENT_NO_EVENT;
    lastButtonEventTime = 0;

    _AgentDisplayMessage("BattleBoats!\nPress BTN4\nto challenge.");
}

AgentState AgentGetState(void)
{
    return state;
}

void AgentSetState(AgentState newState)
{
    state = newState;
}

Message AgentRun(BB_Event event)
{
    Message outMsg = {MESSAGE_NONE, 0, 0, 0};

    /* ------------------------------------------------------------
     * Button debounce: reject a physical button event if it's the
     * same button as the last one we ACCEPTED, within a short
     * window. Only accepted events update the timestamp, so a later
     * deliberate press of the same button still registers normally.
     * ---------------------------------------------------------- */
    if (_IsButtonEvent(event.type))
    {
        uint32_t now = HAL_GetTick();
        if (event.type == lastButtonEventType &&
            (now - lastButtonEventTime) < BUTTON_DEBOUNCE_MS)
        {
            return outMsg;
        }
        lastButtonEventType = event.type;
        lastButtonEventTime = now;
    }

    /* Global transitions, same as Agent.c. */
    if (event.type == BB_EVENT_RESET_BUTTON)
    {
        AgentInit();
        return outMsg;
    }
    if (event.type == BB_EVENT_ERROR)
    {
        _AgentDisplayMessage("ERROR: bad transmission or protocol violation.");
        state = AGENT_STATE_END_SCREEN;
        return outMsg;
    }

    /* ------------------------------------------------------------
     * One-time instructions intercept: while the instructions for
     * the current phase haven't been dismissed yet, ignore every
     * event except START_BUTTON, which dismisses them and does
     * nothing else. The actual game action (place a boat / fire)
     * only happens on the NEXT button press after dismissal.
     * ---------------------------------------------------------- */
    if (state == AGENT_STATE_SETUP_BOATS && !setupInstructionsShown)
    {
        if (event.type == BB_EVENT_START_BUTTON)
        {
            setupInstructionsShown = 1;
        }
        _AgentUpdateBoardDisplay();
        return outMsg;
    }
    if (state == AGENT_STATE_WAITING_TO_SEND && !targetInstructionsShown)
    {
        if (event.type == BB_EVENT_START_BUTTON)
        {
            targetInstructionsShown = 1;
        }
        _AgentUpdateBoardDisplay();
        return outMsg;
    }

    switch (state)
    {

    case AGENT_STATE_START:
        if (event.type == BB_EVENT_START_BUTTON)
        {
            // We'll be the CHALLENGER once boats are placed.
            pendingRole = ROLE_CHALLENGER;
            FieldInit(&ownField, &oppField);
            gameInProgress = 1;
            turnNumber = 0;
            setupRow = 0;
            setupCol = 0;
            setupDir = FIELD_DIR_SOUTH;
            boatToPlace = FIELD_BOAT_TYPE_SMALL;
            state = AGENT_STATE_SETUP_BOATS;
        }
        else if (event.type == BB_EVENT_CHA_RECEIVED)
        {
            // We'll be the ACCEPTOR once boats are placed.
            theirHashA = event.param0;
            pendingRole = ROLE_ACCEPTOR;
            FieldInit(&ownField, &oppField);
            gameInProgress = 1;
            turnNumber = 0;
            setupRow = 0;
            setupCol = 0;
            setupDir = FIELD_DIR_SOUTH;
            boatToPlace = FIELD_BOAT_TYPE_SMALL;
            state = AGENT_STATE_SETUP_BOATS;
        }
        break;

    case AGENT_STATE_SETUP_BOATS:
        if (event.type == BB_EVENT_EAST_BUTTON)
        {
            setupCol = (setupCol + 1) % FIELD_COLS;
        }
        else if (event.type == BB_EVENT_SOUTH_BUTTON)
        {
            setupRow = (setupRow + 1) % FIELD_ROWS;
        }
        else if (event.type == BB_EVENT_START_BUTTON)
        {
            setupDir = _ReadOrientationSwitch();
            if (FieldAddBoat(&ownField, setupRow, setupCol, setupDir, boatToPlace) == SUCCESS)
            {
                if (boatToPlace == FIELD_BOAT_TYPE_HUGE)
                {
                    // All 4 boats placed -- proceed into negotiation.
                    if (pendingRole == ROLE_CHALLENGER)
                    {
                        myA = (NegotiationData)rand();
                        myHashA = NegotiationHash(myA);
                        outMsg.type = MESSAGE_CHA;
                        outMsg.param0 = myHashA;
                        state = AGENT_STATE_CHALLENGING;
                    }
                    else
                    {
                        myB = (NegotiationData)rand();
                        outMsg.type = MESSAGE_ACC;
                        outMsg.param0 = myB;
                        state = AGENT_STATE_ACCEPTING;
                    }
                }
                else
                {
                    boatToPlace++;
                }
            }
            // If placement failed (illegal position), just stay in
            // SETUP_BOATS and let the human try again.
        }
        break;

    case AGENT_STATE_CHALLENGING:
        if (event.type == BB_EVENT_ACC_RECEIVED)
        {
            theirB = event.param0;
            outMsg.type = MESSAGE_REV;
            outMsg.param0 = myA;
            // Either outcome goes to WAITING_TO_SEND or DEFENDING
            // just like Agent.c -- WAITING_TO_SEND here means
            // "wait, then let the human pick a target."
            if (NegotiateCoinFlip(myA, theirB) == HEADS)
            {
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
                _AgentDisplayMessage("Cheating detected by opponent!");
                state = AGENT_STATE_END_SCREEN;
                break;
            }
            // NOTE: unlike Agent.c, we ALWAYS go to WAITING_TO_SEND
            // here (never straight to ATTACKING), per the manual's
            // guidance for human agents -- this lets the human pick
            // their first target even when winning the coin flip.
            if (NegotiateCoinFlip(theirA, myB) == TAILS)
            {
                state = AGENT_STATE_WAITING_TO_SEND;
            }
            else
            {
                state = AGENT_STATE_DEFENDING;
            }
        }
        break;

    case AGENT_STATE_WAITING_TO_SEND:
        // Repurposed as "choose your target." A MESSAGE_SENT may
        // arrive here (confirming a previously queued REV/RES went
        // out) -- we simply ignore it and keep waiting for the
        // human to aim and fire.
        if (event.type == BB_EVENT_EAST_BUTTON)
        {
            targetCol = (targetCol + 1) % FIELD_COLS;
        }
        else if (event.type == BB_EVENT_SOUTH_BUTTON)
        {
            targetRow = (targetRow + 1) % FIELD_ROWS;
        }
        else if (event.type == BB_EVENT_START_BUTTON)
        {
            if (FieldGetSquareStatus(&oppField, targetRow, targetCol) == FIELD_SQUARE_UNKNOWN)
            {
                turnNumber++;
                outMsg.type = MESSAGE_SHO;
                outMsg.param0 = targetRow;
                outMsg.param1 = targetCol;
                state = AGENT_STATE_ATTACKING;
            }
            // Already fired at this square before -- ignore the press
            // and stay in WAITING_TO_SEND; the human must aim
            // elsewhere before their turn can end.
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
                // Reset the target cursor for the next shot.
                targetRow = 0;
                targetCol = 0;
                state = AGENT_STATE_DEFENDING;
            }
        }
        break;

    case AGENT_STATE_DEFENDING:
        // Fully automatic, same as Agent.c -- no human input needed
        // to respond to an incoming shot.
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

    default:
        break;
    }

    _AgentUpdateBoardDisplay();

    return outMsg;
}