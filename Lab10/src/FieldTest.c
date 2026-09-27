/**
 * @file    FieldTest.c
 *
 * Test harness for Field.c. Verifies FieldInit(), FieldAddBoat(),
 * FieldGetSquareStatus()/FieldSetSquareStatus(),
 * FieldRegisterEnemyAttack(), FieldUpdateKnowledge(),
 * FieldGetBoatStates(), FieldAIPlaceAllBoats(), and
 * FieldAIDecideGuess() against what Field.h documents -- including
 * the exact worked placement example given in the Lab 10 manual --
 * so this file behaves the same whether linked against
 * Field_correct.o or the partner's real Field.c.
 *
 * @author  Akhash Arjundas
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <BOARD.h>
#include "Field.h"

/** _ZeroField(f)
 * Field.h's FieldInit() only guarantees the grid contents (EMPTY /
 * UNKNOWN) -- it does NOT guarantee the own-field boat-life counters
 * start at zero (per the header: "our field's boat lives will be
 * filled when boats are added"). Plain stack-local Field structs are
 * NOT zero-initialized in C, so we explicitly zero them ourselves
 * before every FieldInit() call to guarantee a genuinely clean slate,
 * matching what "a properly initialized field" requires.
 */
static void _ZeroField(Field *f)
{
    memset(f, 0, sizeof(Field));
}

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

/** _CountBoatSquares(f)
 * Counts how many grid squares contain any boat type. Used to detect
 * silent overlaps: if two boats were allowed to overlap, the total
 * occupied-square count would come in under the sum of boat sizes
 * (18), since an overlapping write clobbers the earlier boat's mark.
 */
static int _CountBoatSquares(const Field *f)
{
    int count = 0;
    for (int row = 0; row < FIELD_ROWS; row++)
    {
        for (int col = 0; col < FIELD_COLS; col++)
        {
            SquareStatus s = FieldGetSquareStatus(f, row, col);
            if (s == FIELD_SQUARE_SMALL_BOAT || s == FIELD_SQUARE_MEDIUM_BOAT ||
                s == FIELD_SQUARE_LARGE_BOAT || s == FIELD_SQUARE_HUGE_BOAT)
            {
                count++;
            }
        }
    }
    return count;
}

int main(void)
{
    BOARD_Init();
    srand(1); // FieldAIPlaceAllBoats()/FieldAIDecideGuess() are randomized -- seed before use.
    printf("\r\n--- FieldTest starting ---\r\n");

    /* ------------------------------------------------------------
     * SECTION A: FieldInit()
     * ------------------------------------------------------------ */
    printf("\r\n-- Section A: FieldInit() --\r\n");
    Field ownField, oppField;
    _ZeroField(&ownField);
    _ZeroField(&oppField);
    FieldInit(&ownField, &oppField);

    int ownAllEmpty = 1, oppAllUnknown = 1;
    for (int r = 0; r < FIELD_ROWS; r++)
    {
        for (int c = 0; c < FIELD_COLS; c++)
        {
            if (FieldGetSquareStatus(&ownField, r, c) != FIELD_SQUARE_EMPTY)
                ownAllEmpty = 0;
            if (FieldGetSquareStatus(&oppField, r, c) != FIELD_SQUARE_UNKNOWN)
                oppAllUnknown = 0;
        }
    }
    CHECK("FieldInit() fills own field with FIELD_SQUARE_EMPTY", ownAllEmpty);
    CHECK("FieldInit() fills opponent field with FIELD_SQUARE_UNKNOWN", oppAllUnknown);
    CHECK("FieldInit() gives the opponent field full lives on all 4 boats",
          FieldGetBoatStates(&oppField) ==
              (FIELD_BOAT_STATUS_SMALL | FIELD_BOAT_STATUS_MEDIUM |
               FIELD_BOAT_STATUS_LARGE | FIELD_BOAT_STATUS_HUGE));

    /* ------------------------------------------------------------
     * SECTION B: FieldGetSquareStatus() / FieldSetSquareStatus()
     * ------------------------------------------------------------ */
    printf("\r\n-- Section B: Get/SetSquareStatus() --\r\n");
    CHECK("GetSquareStatus rejects an out-of-bounds row",
          FieldGetSquareStatus(&ownField, FIELD_ROWS, 0) == FIELD_SQUARE_INVALID);
    CHECK("GetSquareStatus rejects an out-of-bounds column",
          FieldGetSquareStatus(&ownField, 0, FIELD_COLS) == FIELD_SQUARE_INVALID);

    SquareStatus oldVal = FieldSetSquareStatus(&ownField, 2, 2, FIELD_SQUARE_CURSOR);
    CHECK("SetSquareStatus returns the previous value",
          oldVal == FIELD_SQUARE_EMPTY);
    CHECK("SetSquareStatus actually updates the square",
          FieldGetSquareStatus(&ownField, 2, 2) == FIELD_SQUARE_CURSOR);

    /* ------------------------------------------------------------
     * SECTION C: FieldAddBoat() -- the manual's exact worked example.
     * ------------------------------------------------------------ */
    printf("\r\n-- Section C: FieldAddBoat() (manual's worked example) --\r\n");
    Field mf, dummyOpp;
    _ZeroField(&mf);
    _ZeroField(&dummyOpp);
    FieldInit(&mf, &dummyOpp);

    CHECK("Add small boat at (0,0) EAST succeeds",
          FieldAddBoat(&mf, 0, 0, FIELD_DIR_EAST, FIELD_BOAT_TYPE_SMALL) == SUCCESS);
    CHECK("Add medium boat at (1,0) EAST succeeds",
          FieldAddBoat(&mf, 1, 0, FIELD_DIR_EAST, FIELD_BOAT_TYPE_MEDIUM) == SUCCESS);
    CHECK("Add huge boat at (1,0) EAST fails (overlaps the medium boat)",
          FieldAddBoat(&mf, 1, 0, FIELD_DIR_EAST, FIELD_BOAT_TYPE_HUGE) == STANDARD_ERROR);
    CHECK("Add small boat at (0,6) SOUTH succeeds",
          FieldAddBoat(&mf, 0, 6, FIELD_DIR_SOUTH, FIELD_BOAT_TYPE_SMALL) == SUCCESS);

    // Verify the exact resulting grid matches the manual's ASCII diagram.
    int gridMatches = 1;
    SquareStatus expectedRow0[FIELD_COLS] = {
        FIELD_SQUARE_SMALL_BOAT, FIELD_SQUARE_SMALL_BOAT, FIELD_SQUARE_SMALL_BOAT,
        FIELD_SQUARE_EMPTY, FIELD_SQUARE_EMPTY, FIELD_SQUARE_EMPTY,
        FIELD_SQUARE_SMALL_BOAT, FIELD_SQUARE_EMPTY, FIELD_SQUARE_EMPTY, FIELD_SQUARE_EMPTY};
    SquareStatus expectedRow1[FIELD_COLS] = {
        FIELD_SQUARE_MEDIUM_BOAT, FIELD_SQUARE_MEDIUM_BOAT, FIELD_SQUARE_MEDIUM_BOAT,
        FIELD_SQUARE_MEDIUM_BOAT, FIELD_SQUARE_EMPTY, FIELD_SQUARE_EMPTY,
        FIELD_SQUARE_SMALL_BOAT, FIELD_SQUARE_EMPTY, FIELD_SQUARE_EMPTY, FIELD_SQUARE_EMPTY};
    for (int c = 0; c < FIELD_COLS; c++)
    {
        if (FieldGetSquareStatus(&mf, 0, c) != expectedRow0[c])
            gridMatches = 0;
        if (FieldGetSquareStatus(&mf, 1, c) != expectedRow1[c])
            gridMatches = 0;
    }
    CHECK("FieldGetSquareStatus(mf, 2, 6) shows the small boat's 3rd square",
          FieldGetSquareStatus(&mf, 2, 6) == FIELD_SQUARE_SMALL_BOAT);
    CHECK("Resulting grid matches the manual's worked example exactly", gridMatches);

    uint8_t cStates = FieldGetBoatStates(&mf);
    CHECK("Small and medium boats show as alive after successful placement",
          (cStates & FIELD_BOAT_STATUS_SMALL) && (cStates & FIELD_BOAT_STATUS_MEDIUM));
    CHECK("Huge boat does NOT show as alive (its placement failed)",
          (cStates & FIELD_BOAT_STATUS_HUGE) == 0);

    /* ------------------------------------------------------------
     * SECTION D: FieldAddBoat() -- off-grid boundary cases.
     * ------------------------------------------------------------ */
    printf("\r\n-- Section D: FieldAddBoat() boundary cases --\r\n");
    Field bf, dummyOpp2;
    _ZeroField(&bf);
    _ZeroField(&dummyOpp2);
    FieldInit(&bf, &dummyOpp2);

    // A HUGE boat (size 6) starting at the last row heading SOUTH
    // would need 6 rows starting from FIELD_ROWS - 1, running off grid.
    CHECK("Placing a boat that runs off the bottom edge fails",
          FieldAddBoat(&bf, FIELD_ROWS - 1, 0, FIELD_DIR_SOUTH, FIELD_BOAT_TYPE_HUGE) == STANDARD_ERROR);
    CHECK("A failed placement leaves the field unmodified",
          FieldGetSquareStatus(&bf, FIELD_ROWS - 1, 0) == FIELD_SQUARE_EMPTY);

    // A HUGE boat that exactly fits starting at row (FIELD_ROWS - 6).
    CHECK("Placing a boat that exactly fits the grid succeeds",
          FieldAddBoat(&bf, FIELD_ROWS - FIELD_BOAT_SIZE_HUGE, 5, FIELD_DIR_SOUTH, FIELD_BOAT_TYPE_HUGE) == SUCCESS);

    /* ------------------------------------------------------------
     * SECTION E: FieldRegisterEnemyAttack()
     * ------------------------------------------------------------ */
    printf("\r\n-- Section E: FieldRegisterEnemyAttack() --\r\n");
    Field ef, dummyOpp3;
    _ZeroField(&ef);
    _ZeroField(&dummyOpp3);
    FieldInit(&ef, &dummyOpp3);
    FieldAddBoat(&ef, 0, 0, FIELD_DIR_EAST, FIELD_BOAT_TYPE_SMALL); // occupies (0,0),(0,1),(0,2)

    GuessData atk = {0, 0, RESULT_MISS};
    SquareStatus prev = FieldRegisterEnemyAttack(&ef, &atk);
    CHECK("Attacking a boat square returns its previous (boat) status",
          prev == FIELD_SQUARE_SMALL_BOAT);
    CHECK("First hit on a 3-life boat reports RESULT_HIT (not sunk yet)",
          atk.result == RESULT_HIT);
    CHECK("Attacked square is now marked HIT",
          FieldGetSquareStatus(&ef, 0, 0) == FIELD_SQUARE_HIT);

    atk = (GuessData){0, 1, RESULT_MISS};
    FieldRegisterEnemyAttack(&ef, &atk);
    CHECK("Second hit on the small boat reports RESULT_HIT (still not sunk)",
          atk.result == RESULT_HIT);

    atk = (GuessData){0, 2, RESULT_MISS};
    FieldRegisterEnemyAttack(&ef, &atk);
    CHECK("Third hit sinks the small boat (RESULT_SMALL_BOAT_SUNK)",
          atk.result == RESULT_SMALL_BOAT_SUNK);
    CHECK("Small boat now shows as sunk in FieldGetBoatStates()",
          (FieldGetBoatStates(&ef) & FIELD_BOAT_STATUS_SMALL) == 0);

    atk = (GuessData){3, 3, RESULT_HIT};
    SquareStatus prevMiss = FieldRegisterEnemyAttack(&ef, &atk);
    CHECK("Attacking empty water returns previous EMPTY status",
          prevMiss == FIELD_SQUARE_EMPTY);
    CHECK("Attacking empty water reports RESULT_MISS",
          atk.result == RESULT_MISS);
    CHECK("Missed square is now marked MISS",
          FieldGetSquareStatus(&ef, 3, 3) == FIELD_SQUARE_MISS);

    /* ------------------------------------------------------------
     * SECTION F: FieldUpdateKnowledge()
     * ------------------------------------------------------------ */
    printf("\r\n-- Section F: FieldUpdateKnowledge() --\r\n");
    Field ownDummy, kf; // kf represents our knowledge of the opponent's board
    _ZeroField(&ownDummy);
    _ZeroField(&kf);
    FieldInit(&ownDummy, &kf); // kf initialized with full lives via the opponent-field path

    GuessData ourMiss = {2, 2, RESULT_MISS};
    SquareStatus prevKnowledge = FieldUpdateKnowledge(&kf, &ourMiss);
    CHECK("A previously-unknown square starts as UNKNOWN",
          prevKnowledge == FIELD_SQUARE_UNKNOWN);
    CHECK("A miss is recorded as EMPTY (known no boat there)",
          FieldGetSquareStatus(&kf, 2, 2) == FIELD_SQUARE_EMPTY);

    GuessData ourHit = {3, 3, RESULT_HIT};
    FieldUpdateKnowledge(&kf, &ourHit);
    CHECK("A hit is recorded as HIT",
          FieldGetSquareStatus(&kf, 3, 3) == FIELD_SQUARE_HIT);

    GuessData ourSink = {4, 4, RESULT_MEDIUM_BOAT_SUNK};
    FieldUpdateKnowledge(&kf, &ourSink);
    CHECK("A sinking hit is still recorded as HIT on the grid",
          FieldGetSquareStatus(&kf, 4, 4) == FIELD_SQUARE_HIT);
    CHECK("Sinking the opponent's medium boat clears only that boat's life bit",
          FieldGetBoatStates(&kf) ==
              (FIELD_BOAT_STATUS_SMALL | FIELD_BOAT_STATUS_LARGE | FIELD_BOAT_STATUS_HUGE));

    /* ------------------------------------------------------------
     * SECTION G: FieldAIPlaceAllBoats()
     * ------------------------------------------------------------ */
    printf("\r\n-- Section G: FieldAIPlaceAllBoats() --\r\n");
    int allPlacementsOk = 1;
    int allNoOverlap = 1;
    int allAlive = 1;
    for (int trial = 0; trial < 20; trial++)
    {
        Field pf, dummyOppP;
        _ZeroField(&pf);
        _ZeroField(&dummyOppP);
        FieldInit(&pf, &dummyOppP);
        if (FieldAIPlaceAllBoats(&pf) != SUCCESS)
            allPlacementsOk = 0;
        if (_CountBoatSquares(&pf) != (FIELD_BOAT_SIZE_SMALL + FIELD_BOAT_SIZE_MEDIUM +
                                       FIELD_BOAT_SIZE_LARGE + FIELD_BOAT_SIZE_HUGE))
        {
            allNoOverlap = 0;
        }
        if (FieldGetBoatStates(&pf) != (FIELD_BOAT_STATUS_SMALL | FIELD_BOAT_STATUS_MEDIUM |
                                        FIELD_BOAT_STATUS_LARGE | FIELD_BOAT_STATUS_HUGE))
        {
            allAlive = 0;
        }
    }
    CHECK("FieldAIPlaceAllBoats() succeeds across 20 trials", allPlacementsOk);
    CHECK("Total occupied squares always equals the sum of boat sizes (no overlaps)", allNoOverlap);
    CHECK("All 4 boats are alive after placement, every trial", allAlive);

    /* ------------------------------------------------------------
     * SECTION H: FieldAIDecideGuess()
     * ------------------------------------------------------------ */
    printf("\r\n-- Section H: FieldAIDecideGuess() --\r\n");
    Field dummyOwnH, gf; // gf represents our knowledge of the opponent's board (needs UNKNOWN squares)
    _ZeroField(&dummyOwnH);
    _ZeroField(&gf);
    FieldInit(&dummyOwnH, &gf);

    static uint8_t visited[FIELD_ROWS][FIELD_COLS];
    for (int r = 0; r < FIELD_ROWS; r++)
        for (int c = 0; c < FIELD_COLS; c++)
            visited[r][c] = 0;

    int allInBounds = 1, noRepeats = 1;
    for (int i = 0; i < FIELD_ROWS * FIELD_COLS; i++)
    {
        GuessData g = FieldAIDecideGuess(&gf);
        if (g.row >= FIELD_ROWS || g.col >= FIELD_COLS)
        {
            allInBounds = 0;
            continue;
        }
        if (visited[g.row][g.col])
        {
            noRepeats = 0;
        }
        visited[g.row][g.col] = 1;

        // Mark this square as no longer UNKNOWN, mirroring how Agent.c
        // calls FieldUpdateKnowledge() after every real shot -- without
        // this, a field-contents-based "no repeat" check has no way to
        // know a square was already guessed.
        GuessData resolved = {g.row, g.col, RESULT_MISS};
        FieldUpdateKnowledge(&gf, &resolved);
    }
    CHECK("Every guess stays within the grid bounds", allInBounds);
    CHECK("No guess is repeated across a full board's worth of calls (60 guesses)", noRepeats);

    printf("\r\n--- FieldTest complete: %d/%d passed ---\r\n",
           testsPassed, testsRun);

    while (1)
    {
        // Idle so serial output stays visible.
    }

    return SUCCESS;
}