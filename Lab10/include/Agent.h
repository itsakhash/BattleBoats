#ifndef AGENT_H
#define AGENT_H
/**
 * Agent.h
 *
 * Header file for Agent module.
 *
 * @author  HARE Lab
 * @author  jLab
 * @version 1.0.1
 *
 * @date    5 May 2025
 */
#include <stdint.h>

#include "Message.h"
#include "BattleBoats.h"


/*  MODULE-LEVEL DEFINITIONS, MACROS    */

/** AgentState
 *
 * Defines the various states used within the agent state machines. All states
 * should be used within a valid agent implementation. Additionally there is no
 * need for states outside of those defined here.
 */
typedef enum {
    AGENT_STATE_START,              //0
    AGENT_STATE_CHALLENGING,        //1
    AGENT_STATE_ACCEPTING,          //2
    AGENT_STATE_ATTACKING,          //3
    AGENT_STATE_DEFENDING,          //4
    AGENT_STATE_WAITING_TO_SEND,    //5
    AGENT_STATE_END_SCREEN,         //6

    // If implementing a human agent, you will need a state to setup boats:
    AGENT_STATE_SETUP_BOATS,        //7
} AgentState;


/*  PROTOTYPES  */

/** AgentInit()
 *
 * The Init() function for an Agent sets up everything necessary for an agent
 * before the game starts.  At a minimum, this requires:
 *   -setting the start state of the Agent SM.
 *   -setting turn counter to 0
 *
 * If you are using any other persistent data in Agent SM, that should be reset
 * as well.
 * 
 * It is not advised to call srand() inside of AgentInit().
 */
void AgentInit(void);

/** AgentRun(event)
 *
 * AgentRun() evolves the Agent state machine in response to an event.
 * 
 * If the returned Message struct is a valid message (that is, not of type
 * MESSAGE_NONE), then it will be passed to the transmission module and sent via
 * UART. This is handled at the top level! AgentRun is ONLY responsible for
 * generating the Message struct, not for encoding or sending it.
 * 
 * @param   event   The most recently detected event
 * @return  Message, a Message struct to send to the opponent. 
 */
Message AgentRun(BB_Event event);

/** AgentGetState() 
 *
 * This function is very useful for testing AgentRun().
 *
 * @return  Returns the current state that AgentGetState() is in.
 */
AgentState AgentGetState(void);

/** AgentSetState(newState)
 * 
 * This function is very useful for testing AgentRun().
 *
 * @param   newState    Force the agent into the state given by AgentState.
 */
void AgentSetState(AgentState newState);


#endif  /*  AGENT_H */
