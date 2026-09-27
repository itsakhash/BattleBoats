/**
 * @file    HumanAgentTest.c  (PERSONAL SCRATCH TEST -- not a required
 *          submission file; HumanAgent.c is extra credit, and the lab
 *          manual does not require a dedicated test harness for it.)
 *
 * Automated test coverage for HumanAgent.c, focused specifically on
 * the two behaviors added on top of the base state machine: button
 * debounce and repeat-fire prevention during target selection.
 *
 * IMPORTANT LIMITATIONS OF TESTING A HUMAN-INTERFACE AGENT:
 *
 *   1. No internal state is exposed besides AgentGetState()/
 *      AgentSetState() and the Message returned by AgentRun(). There
 *      is no getter for the cursor position, the boat-setup progress,
 *      or the contents of ownField/oppField -- everything here is
 *      inferred indirectly from message contents (e.g. reading a
 *      fired SHO's row/col to confirm where the cursor actually was).
 *
 *   2. Boat orientation is read live from a physical switch
 *      (SW1_STATE()) at the moment BTN4 is pressed. A test has no way
 *      to control or read that switch's position, so this file
 *      deliberately AVOIDS exercising real boat placement -- it
 *      jumps past SETUP_BOATS entirely via AgentSetState() rather
 *      than risk a test whose outcome silently depends on whatever
 *      position the physical switch happens to be in.
 *
 *   3. Timing-dependent behavior (button debounce) is approximated
 *      with HAL_Delay() to stand in for "a human pressed this again
 *      later, on purpose." This is a reasonable proxy, not a
 *      guarantee that a real human's timing will always exceed the
 *      debounce window -- extremely fast deliberate double-presses
 *      remain indistinguishable from bounce by design (see the
 *      discussion of this tradeoff in HumanAgent.c's own comments).
 *
 *   4. Nothing about the OLED's actual visual output -- text
 *      legibility, instruction-screen wording, whether the boat
 *      preview or cursor renders in the right square -- can be
 *      verified by an automated test. That can only be confirmed by
 *      a human looking at the physical screen.
 *
 *   5. This file bypasses negotiation and boat setup using
 *      AgentSetState() specifically to isolate the two behaviors
 *      under test. It does NOT re-verify negotiation logic (already
 *      covered by AgentTest.c, since HumanAgent.c reuses the exact
 *      same Negotiation.c functions) or defend/attack win-loss
 *      realism, which would require a fully and realistically
 *      populated ownField this file never sets up.
 *
 * @author  Akhash Arjundayal (aarjunda)
 */
#include <stdio.h>
#include <BOARD.h>
#include "Agent.h"
#include "Field.h"
#include "FieldOled.h" // For OLED_Init()'s declaration.

static int testsRun = 0;
static int testsPassed = 0;

#define CHECK(description, condition)             \
    do                                            \
    {                                             \
        testsRun++;                               \
        if (condition)                            \
        {                                         \
            testsPassed++;                        \
            printf("[PASS] %s\r\n", description); \
        }                                         \
        else                                      \
        {                                         \
            printf("[FAIL] %s\r\n", description); \
        }                                         \
    } while (0)

// Longer than HumanAgent.c's BUTTON_DEBOUNCE_MS (100ms), so a delay of
// this length reliably counts as "a later, deliberate press" rather
// than a bounce.
#define CLEAR_DEBOUNCE_MS 150

int main(void)
{
    BOARD_Init();
    OLED_Init(); // Required: HumanAgent.c writes to the OLED on every call.
    printf("\r\n--- HumanAgentTest (personal scratch test) starting ---\r\n");

    /* ------------------------------------------------------------
     * Shared setup: get oppField into a real, validly-initialized
     * (all-UNKNOWN) state via the actual START_BUTTON transition,
     * then jump straight to WAITING_TO_SEND -- skipping real boat
     * placement and negotiation entirely, per the limitations above.
     * ------------------------------------------------------------ */
    printf("\r\n-- Setup --\r\n");
    AgentInit();
    CHECK("AgentInit() -> START state", AgentGetState() == AGENT_STATE_START);

    BB_Event startBtn = {BB_EVENT_START_BUTTON, 0, 0, 0};
    AgentRun(startBtn); // Real transition: calls FieldInit() internally.
    CHECK("START_BUTTON -> SETUP_BOATS (initializes oppField for real)",
          AgentGetState() == AGENT_STATE_SETUP_BOATS);

    AgentSetState(AGENT_STATE_WAITING_TO_SEND);

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event dismissBtn = {BB_EVENT_START_BUTTON, 0, 0, 0};
    Message dismissMsg = AgentRun(dismissBtn); // First press: dismiss instructions only.
    CHECK("First BTN4 in WAITING_TO_SEND only dismisses instructions",
          dismissMsg.type == MESSAGE_NONE &&
              AgentGetState() == AGENT_STATE_WAITING_TO_SEND);

    /* ------------------------------------------------------------
     * SECTION A: Repeat-fire prevention.
     * ------------------------------------------------------------ */
    printf("\r\n-- Section A: Repeat-fire prevention --\r\n");

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event fireBtn = {BB_EVENT_START_BUTTON, 0, 0, 0};
    Message firstShot = AgentRun(fireBtn);
    CHECK("First fire at (0,0) sends a real SHO",
          firstShot.type == MESSAGE_SHO &&
              firstShot.param0 == 0 && firstShot.param1 == 0);
    CHECK("First fire -> ATTACKING state",
          AgentGetState() == AGENT_STATE_ATTACKING);

    BB_Event resMiss = {BB_EVENT_RES_RECEIVED, 0, 0, RESULT_MISS};
    AgentRun(resMiss); // Marks (0,0) as no-longer-UNKNOWN in oppField.
    CHECK("Non-winning RES -> DEFENDING",
          AgentGetState() == AGENT_STATE_DEFENDING);

    // Skip DEFENDING's real SHO_RECEIVED requirement -- ownField was
    // never given real boats in this test (see limitation #2 above),
    // so driving a real incoming attack here would trigger a false
    // "all boats sunk" defeat unrelated to what we're testing.
    AgentSetState(AGENT_STATE_WAITING_TO_SEND);

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event repeatFireBtn = {BB_EVENT_START_BUTTON, 0, 0, 0};
    Message repeatAttempt = AgentRun(repeatFireBtn);
    CHECK("Firing again at the already-hit (0,0) sends no message",
          repeatAttempt.type == MESSAGE_NONE);
    CHECK("Repeat-fire attempt does NOT end the turn (stays WAITING_TO_SEND)",
          AgentGetState() == AGENT_STATE_WAITING_TO_SEND);

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event moveEast = {BB_EVENT_EAST_BUTTON, 0, 0, 0};
    AgentRun(moveEast); // Cursor moves to (0,1), a genuinely UNKNOWN square.

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event secondFireBtn = {BB_EVENT_START_BUTTON, 0, 0, 0};
    Message secondShot = AgentRun(secondFireBtn);
    CHECK("Firing at a fresh square (0,1) succeeds after moving",
          secondShot.type == MESSAGE_SHO &&
              secondShot.param0 == 0 && secondShot.param1 == 1);
    CHECK("Successful fire at a new square -> ATTACKING",
          AgentGetState() == AGENT_STATE_ATTACKING);

    /* ------------------------------------------------------------
     * SECTION B: Button debounce.
     * A rapid same-button repeat (no delay) should be dropped; a
     * later press of the same button (after CLEAR_DEBOUNCE_MS)
     * should still register normally. We can't read the cursor
     * directly, so we infer its position from where the eventual
     * fire actually lands.
     *
     * Each sub-test moves to a FRESH, never-before-touched row via
     * SOUTH_BUTTON first (with its own debounce-clearing delay),
     * then does its EAST_BUTTON checks purely on untouched columns
     * within that row -- this avoids colliding with the repeat-fire
     * guard from Section A, which (correctly) blocks re-firing at
     * any square already resolved by a previous RES_RECEIVED.
     * ------------------------------------------------------------ */
    printf("\r\n-- Section B: Button debounce --\r\n");

    BB_Event resMiss2 = {BB_EVENT_RES_RECEIVED, 0, 1, RESULT_MISS};
    AgentRun(resMiss2); // Resolves Section A's pending shot at (0,1); resets cursor to (0,0).
    AgentSetState(AGENT_STATE_WAITING_TO_SEND);

    // --- Debounce test 1: immediate repeat should be dropped. ---
    // Move to row 1 (fresh), then test EAST_BUTTON debounce at (1,0)->(1,1).
    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event moveSouth1 = {BB_EVENT_SOUTH_BUTTON, 0, 0, 0};
    AgentRun(moveSouth1); // (0,0) -> (1,0), untouched row.

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event moveEast2 = {BB_EVENT_EAST_BUTTON, 0, 0, 0};
    AgentRun(moveEast2); // (1,0) -> (1,1). Debounce clock now set for EAST_BUTTON.

    // Immediate same-button repeat, no delay: should be dropped.
    BB_Event bounceEast = {BB_EVENT_EAST_BUTTON, 0, 0, 0};
    AgentRun(bounceEast);

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event fireAfterBounce = {BB_EVENT_START_BUTTON, 0, 0, 0};
    Message shotAfterBounce = AgentRun(fireAfterBounce);
    CHECK("An immediate same-button repeat is dropped (cursor only moved once)",
          shotAfterBounce.type == MESSAGE_SHO &&
              shotAfterBounce.param0 == 1 && shotAfterBounce.param1 == 1);

    BB_Event resMiss3 = {BB_EVENT_RES_RECEIVED, 1, 1, RESULT_MISS};
    AgentRun(resMiss3); // Resolves this shot; resets cursor to (0,0).
    AgentSetState(AGENT_STATE_WAITING_TO_SEND);

    // --- Debounce test 2: a later, deliberate repeat should register. ---
    // Move to row 2 (fresh: two SOUTH presses, each cleared by its own delay).
    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event moveSouth2 = {BB_EVENT_SOUTH_BUTTON, 0, 0, 0};
    AgentRun(moveSouth2); // (0,0) -> (1,0).

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event moveSouth3 = {BB_EVENT_SOUTH_BUTTON, 0, 0, 0};
    AgentRun(moveSouth3); // (1,0) -> (2,0), untouched row.

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event moveEast3 = {BB_EVENT_EAST_BUTTON, 0, 0, 0};
    AgentRun(moveEast3); // (2,0) -> (2,1).

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event moveEast4 = {BB_EVENT_EAST_BUTTON, 0, 0, 0};
    AgentRun(moveEast4); // A genuinely later press: (2,1) -> (2,2).

    HAL_Delay(CLEAR_DEBOUNCE_MS);
    BB_Event fireAfterTwoMoves = {BB_EVENT_START_BUTTON, 0, 0, 0};
    Message shotAfterTwoMoves = AgentRun(fireAfterTwoMoves);
    CHECK("A later, deliberate same-button press still registers (cursor moved twice)",
          shotAfterTwoMoves.type == MESSAGE_SHO &&
              shotAfterTwoMoves.param0 == 2 && shotAfterTwoMoves.param1 == 2);

    printf("\r\n--- HumanAgentTest complete: %d/%d passed ---\r\n",
           testsPassed, testsRun);

    while (1)
    {
        // Idle so serial output stays visible.
    }

    return SUCCESS;
}