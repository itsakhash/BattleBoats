/**
 * @file    AgentTest.c
 *
 *  Test harness for the Agent module.
 *
 * @author  Pranav Kamat (pkamat)
 *
 * @date    26 Aug 2026
 */

// Include libaries.
#include <stdlib.h>
#include <stdio.h>
#include "Agent.h"
#include "Negotiation.h"
#include "Field.h"
#include "Message.h"
#include "BattleBoats.h"
#include "FieldOled.h"

#define ZERO_PARAM 0
#define NO_TESTS 0
#define NO_TESTS_DEC 0.0
#define DEC_FACTOR 100.0

#define TEST_RESULT_PASS "PASS | %s\r\n"
#define TEST_RESULT_FAIL "FAIL | %s\r\n"
#define OVERALL_STATS "Tests passed: %d/%d (%.1f)\r\n"

// Helper functions, standardized.
static int testsRun = 0;
static int testsPassed = 0;

#define CHECK(description, condition)                \
     do                                              \
     {                                               \
          testsRun++;                                \
          if (condition)                             \
          {                                          \
               testsPassed++;                        \
               printf("[PASS] %s\r\n", description); \
          }                                          \
          else                                       \
          {                                          \
               printf("[FAIL] %s\r\n", description); \
          }                                          \
     } while (0)

void OverallStats(void)
{
     double pass_rate = (testsRun > NO_TESTS) ? (DEC_FACTOR * testsPassed / testsRun) : NO_TESTS_DEC;

     printf(OVERALL_STATS, testsPassed, testsRun, pass_rate);
}

static BB_Event MakeEvent(BB_EventType type, uint16_t p0, uint16_t p1, uint16_t p2)
{
     BB_Event e;
     e.type = type;
     e.param0 = p0;
     e.param1 = p1;
     e.param2 = p2;
     return e;
}

// Hard coding each test. We test each of the usual state transitions, as well as what happens
// when no/empty input is passed, as well as the outcome of passing a clearly wrong input.

static void TestAgentInit(void)
{
     AgentSetState(AGENT_STATE_ATTACKING);
     AgentInit();
     CHECK("AgentInit() sets state to AGENT_STATE_START", AgentGetState() == AGENT_STATE_START);
}

static void TestState_Start(void)
{
     // Start to Challenging via START_BUTTON.
     AgentInit();
     AgentSetState(AGENT_STATE_START);
     Message msg = AgentRun(MakeEvent(BB_EVENT_START_BUTTON, 0, 0, 0));
     CHECK("Start to Challenging transition", AgentGetState() == AGENT_STATE_CHALLENGING);
     CHECK("Start to Challenging sends MESSAGE_CHA", msg.type == MESSAGE_CHA);

     // Start to Accepting via CHA_RECEIVED.
     AgentInit();
     AgentSetState(AGENT_STATE_START);
     msg = AgentRun(MakeEvent(BB_EVENT_CHA_RECEIVED, 12345, 0, 0));
     CHECK("Start to Accepting transition", AgentGetState() == AGENT_STATE_ACCEPTING);
     CHECK("Start to Accepting sends MESSAGE_ACC", msg.type == MESSAGE_ACC);

     // Testing what happens in the case of NO_EVENT.
     AgentInit();
     AgentSetState(AGENT_STATE_START);
     msg = AgentRun(MakeEvent(BB_EVENT_NO_EVENT, 0, 0, 0));
     CHECK("When passed NO_EVENT, Agent state machine stays in starting mode",
           AgentGetState() == AGENT_STATE_START);
     CHECK("When passed NO_EVENT, Agent sends MESSAGE_NONE in starting mode",
           msg.type == MESSAGE_NONE);

     // Testing what happens when we pass nonsensical input.
     AgentInit();
     AgentSetState(AGENT_STATE_START);
     msg = AgentRun(MakeEvent(BB_EVENT_RES_RECEIVED, 1, 2, 3));
     CHECK("Agent state machine stays in start when passed unexpected input",
           AgentGetState() == AGENT_STATE_START);
     CHECK("Agent state machine sends MESSAGE_NONE when passed unexpected input",
           msg.type == MESSAGE_NONE);
}

static void TestState_Challenging(void)
{
     // Normal transition.
     AgentInit();
     AgentSetState(AGENT_STATE_START);
     AgentRun(MakeEvent(BB_EVENT_START_BUTTON, 0, 0, 0));
     Message msg = AgentRun(MakeEvent(BB_EVENT_ACC_RECEIVED, 999, 0, 0));
     CHECK("Challenging sends MESSAGE_REV after ACC_RECEIVED", msg.type == MESSAGE_REV);
     CHECK("Challenging transitions to WAITING_TO_SEND or DEFENDING after ACC_RECEIVED",
           AgentGetState() == AGENT_STATE_WAITING_TO_SEND ||
               AgentGetState() == AGENT_STATE_DEFENDING);

     // No event.
     AgentInit();
     AgentSetState(AGENT_STATE_CHALLENGING);
     msg = AgentRun(MakeEvent(BB_EVENT_NO_EVENT, 0, 0, 0));
     CHECK("When passed NO_EVENT, Agent stays in Challenging",
           AgentGetState() == AGENT_STATE_CHALLENGING);
     CHECK("When passed NO_EVENT, Agent sends MESSAGE_NONE",
           msg.type == MESSAGE_NONE);

     // Nonsensical input.
     AgentInit();
     AgentSetState(AGENT_STATE_CHALLENGING);
     msg = AgentRun(MakeEvent(BB_EVENT_SHO_RECEIVED, 1, 2, 0));
     CHECK("Agent state machine stays in Challenging when passed unexpected input",
           AgentGetState() == AGENT_STATE_CHALLENGING);
     CHECK("Agent sends MESSAGE_NONE when passed unexpected input in Challenging",
           msg.type == MESSAGE_NONE);
}

static void TestState_Accepting(void)
{
     // Standard input, hash cannot be reconciled.
     AgentInit();
     AgentSetState(AGENT_STATE_START);
     AgentRun(MakeEvent(BB_EVENT_CHA_RECEIVED, 111, 0, 0)); // Standard oppHash 111.
     Message msg = AgentRun(MakeEvent(BB_EVENT_REV_RECEIVED, 222, 0, 0));
     CHECK("Agent sends bad verification to END_SCREEN",
           AgentGetState() == AGENT_STATE_END_SCREEN);
     CHECK("Agent sends NO_MESSAGE after bad verification",
           msg.type == MESSAGE_NONE);

     // Empty input.
     AgentInit();
     AgentSetState(AGENT_STATE_ACCEPTING);
     msg = AgentRun(MakeEvent(BB_EVENT_NO_EVENT, 0, 0, 0));
     CHECK("Agent stays in Accepting when passed NO_EVENT",
           AgentGetState() == AGENT_STATE_ACCEPTING);
     CHECK("Agent sends MESSAGE_NONE in Accepting when passed NO_EVENT",
           msg.type == MESSAGE_NONE);

     // Nonsensical input.
     AgentInit();
     AgentSetState(AGENT_STATE_ACCEPTING);
     msg = AgentRun(MakeEvent(BB_EVENT_MESSAGE_SENT, 0, 0, 0));
     CHECK("Agent stays in Accepting when passed unexpected input",
           AgentGetState() == AGENT_STATE_ACCEPTING);
     CHECK("Agent sends MESSAGE_NONE when passed unexpected input in Accepting",
           msg.type == MESSAGE_NONE);
}

static void TestState_WaitingToSend(void)
{
     // Waiting to send to attacking works properly
     AgentInit();
     AgentSetState(AGENT_STATE_START);
     AgentRun(MakeEvent(BB_EVENT_START_BUTTON, 0, 0, 0)); // sets up fields
     AgentSetState(AGENT_STATE_WAITING_TO_SEND);
     Message msg = AgentRun(MakeEvent(BB_EVENT_MESSAGE_SENT, 0, 0, 0));
     CHECK("Agent goes to attacking when MESSAGE_SENT from Waiting to Send",
           AgentGetState() == AGENT_STATE_ATTACKING);
     CHECK("Agent sends MESSAGE_SHO when MESSAGE_SENT from Waiting to Send",
           msg.type == MESSAGE_SHO);

     // Empty input
     AgentSetState(AGENT_STATE_WAITING_TO_SEND);
     msg = AgentRun(MakeEvent(BB_EVENT_NO_EVENT, 0, 0, 0));
     CHECK("When passed an empty input, the agent stays in WAITING_TO_SEND",
           AgentGetState() == AGENT_STATE_WAITING_TO_SEND);
     CHECK("Agent 'sends' MESSAGE_NONE when passed empty input in WAITING_TO_SEND",
           msg.type == MESSAGE_NONE);

     // Nonsensical input.
     AgentSetState(AGENT_STATE_WAITING_TO_SEND);
     msg = AgentRun(MakeEvent(BB_EVENT_CHA_RECEIVED, 1, 0, 0));
     CHECK("WAITING_TO_SEND + unexpected CHA_RECEIVED stays in WAITING_TO_SEND",
           AgentGetState() == AGENT_STATE_WAITING_TO_SEND);
     CHECK("WAITING_TO_SEND + unexpected CHA_RECEIVED sends MESSAGE_NONE",
           msg.type == MESSAGE_NONE);
}

static void TestState_Attacking(void)
{
     // Agent goes into defending properly
     AgentInit();
     AgentSetState(AGENT_STATE_START);
     AgentRun(MakeEvent(BB_EVENT_START_BUTTON, 0, 0, 0)); // sets up fields, boats placed
     AgentSetState(AGENT_STATE_ATTACKING);
     Message msg = AgentRun(MakeEvent(BB_EVENT_RES_RECEIVED, 0, 0, RESULT_MISS));
     CHECK("When result is not victory, Agent goes to Defending from Attacking",
           AgentGetState() == AGENT_STATE_DEFENDING);
     CHECK("Agent sends no message when receiving result", msg.type == MESSAGE_NONE);

     // Empty input
     AgentSetState(AGENT_STATE_ATTACKING);
     msg = AgentRun(MakeEvent(BB_EVENT_NO_EVENT, 0, 0, 0));
     CHECK("When passed NO_EVENT, agent stays in Attacking",
           AgentGetState() == AGENT_STATE_ATTACKING);
     CHECK("When passed NO_EVENT, agent sends MESSAGE_NONE", msg.type == MESSAGE_NONE);

     // Nonsensical input
     AgentSetState(AGENT_STATE_ATTACKING);
     msg = AgentRun(MakeEvent(BB_EVENT_ACC_RECEIVED, 1, 0, 0));
     CHECK("Agent handles weird input in Attacking properly",
           AgentGetState() == AGENT_STATE_ATTACKING);
     CHECK("Agent sends no message when passed weird input in Attacking",
           msg.type == MESSAGE_NONE);
}

static void TestState_Defending(void)
{
     // Proper transition from defending to waiting to send
     AgentInit();
     AgentSetState(AGENT_STATE_START);
     AgentRun(MakeEvent(BB_EVENT_START_BUTTON, 0, 0, 0)); // sets up fields, boats placed
     AgentSetState(AGENT_STATE_DEFENDING);
     Message msg = AgentRun(MakeEvent(BB_EVENT_SHO_RECEIVED, 0, 0, 0));
     CHECK("Agent sends result from Defending", msg.type == MESSAGE_RES);
     CHECK("Agent transitions to WAITING_TO_SEND from Defending properly",
           AgentGetState() == AGENT_STATE_WAITING_TO_SEND);

     // Empty input
     AgentSetState(AGENT_STATE_DEFENDING);
     msg = AgentRun(MakeEvent(BB_EVENT_NO_EVENT, 0, 0, 0));
     CHECK("Agent stays in Defending when passed empty input",
           AgentGetState() == AGENT_STATE_DEFENDING);
     CHECK("Agent does not send a message from Defending when passed empty input",
           msg.type == MESSAGE_NONE);

     // Nonsensical input.
     AgentSetState(AGENT_STATE_DEFENDING);
     msg = AgentRun(MakeEvent(BB_EVENT_REV_RECEIVED, 1, 0, 0));
     CHECK("Agent does not act on bad input in Defending",
           AgentGetState() == AGENT_STATE_DEFENDING);
     CHECK("Agent sends no message when passed bad input in Defending",
           msg.type == MESSAGE_NONE);
}

static void TestState_EndScreen(void)
{
     // We basically check that End_Screen cannot be exited with faulty (empty or nonsensical)
     // inputs.
     AgentSetState(AGENT_STATE_END_SCREEN);
     Message msg = AgentRun(MakeEvent(BB_EVENT_NO_EVENT, 0, 0, 0));
     CHECK("Agent stays in end screen", AgentGetState() == AGENT_STATE_END_SCREEN);
     CHECK("Agent does not send message in end screen", msg.type == MESSAGE_NONE);

     AgentSetState(AGENT_STATE_END_SCREEN);
     msg = AgentRun(MakeEvent(BB_EVENT_START_BUTTON, 0, 0, 0));
     CHECK("Agent stays in end screen even when passed bad input",
           AgentGetState() == AGENT_STATE_END_SCREEN);
     CHECK("Agent doesn't send a message from end screen when passed bad input",
           msg.type == MESSAGE_NONE);
}

static void TestGlobalTransitions(void)
{
     // Does reset button go to start?
     AgentSetState(AGENT_STATE_ATTACKING);
     Message msg = AgentRun(MakeEvent(BB_EVENT_RESET_BUTTON, 0, 0, 0));
     CHECK("Pressing reset from attack succesfully transitions Agent to Start",
           AgentGetState() == AGENT_STATE_START);
     CHECK("Agent does not send message when transitioning to start", msg.type == MESSAGE_NONE);

     AgentSetState(AGENT_STATE_DEFENDING);
     msg = AgentRun(MakeEvent(BB_EVENT_ERROR, 0, 0, 0));
     CHECK("ERROR from Defending transitions Agent to End Screen",
           AgentGetState() == AGENT_STATE_END_SCREEN);
     CHECK("Agent does not send a message when undergoing Error transition",
           msg.type == MESSAGE_NONE);
}

// Now, we run tests.
int main(void)
{
     BOARD_Init();
     OLED_Init();
     printf("\r\n Beginning pkamat's Agent test harness. \r\n");
     TestAgentInit();
     TestState_Start();
     TestState_Challenging();
     TestState_Accepting();
     TestState_WaitingToSend();
     TestState_Attacking();
     TestState_Defending();
     TestState_EndScreen();
     TestGlobalTransitions();

     printf("\r\n");
     OverallStats();

     return (testsPassed == testsRun) ? 0 : 1;
}