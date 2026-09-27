/**
 * @file    Field.c
 *
 *  Field module for BattleBoats lab.
 *
 * @author  Pranav Kamat (pkamat)
 * @date    24 Aug 2026
 */

// Including relevant files.

#include <stdint.h>
#include <stdlib.h>
#include "BattleBoats.h"
#include <BOARD.h>

// Define relevant constants/macros.
#ifndef FIELD_COLS
#define FIELD_COLS 10
#endif
#ifndef FIELD_ROWS
#define FIELD_ROWS 6
#endif

#define FIELD_NUM_BOATS 4
#define NO_LIVES 0
#define INIT_INDEX_VAL 0
#define INITIAL_STATE_VAL 0
#define NUM_DIRECTIONS 2

// Module level variables.

/** SquareStatus
 *
 * Set different constants used for conveying different information about the
 * different locations of the field. These values should be used for the actual
 * storage of the field state, which is almost every usage. For displaying the
 * field using FieldPrint(), the SquareStatus Display enum values should be used
 * instead.
 */
typedef enum
{
    // These denote field positions useful for representing the local board.
    FIELD_SQUARE_EMPTY = 0,   // An empty field position.
    FIELD_SQUARE_SMALL_BOAT,  // This position contains part of the small
                              //  boat.
    FIELD_SQUARE_MEDIUM_BOAT, // This position contains part of the medium
                              //  boat.
    FIELD_SQUARE_LARGE_BOAT,  // This position contains part of the large
                              //  boat.
    FIELD_SQUARE_HUGE_BOAT,   // This position contains part of the huge boat.

    // These denote field positions useful for representing the enemy's board.
    FIELD_SQUARE_UNKNOWN, // It is unknown what is here. Useful for
                          //  denoting a position on the enemy's board
                          //  that hasn't been checked.

    // These statuses may be used on either field:
    FIELD_SQUARE_HIT,  // A field position that was attacked and
                       //  contained part of a boat.
    FIELD_SQUARE_MISS, // This position was attacked by the enemy, but
                       //  was empty.

    // This may be useful for implementing extra-credit features:
    FIELD_SQUARE_CURSOR,  // This is used merely for use in FieldOled.c
                          //  for indicating the current cursor when
                          //  selecting a position to attack.
    FIELD_SQUARE_INVALID, // Occasionally, it may be necessary to
                          //  indicate an error using a square status.
} SquareStatus;

/** ShotResult
 *
 * These are the possible results of shots:
 */
typedef enum
{
    RESULT_MISS,             // 0
    RESULT_HIT,              // 1
    RESULT_SMALL_BOAT_SUNK,  // 2
    RESULT_MEDIUM_BOAT_SUNK, // 3
    RESULT_LARGE_BOAT_SUNK,  // 4
    RESULT_HUGE_BOAT_SUNK,   // 5
} ShotResult;

/** GuessData
 *
 * GuessData is used for exchanging coordinate data along with information about
 * of coordinate.
 */
typedef struct
{
    uint8_t row;       // Row of the coordinate that was guessed.
    uint8_t col;       // Column of the coordinate guessed.
    ShotResult result; // Result of a shot at this coordinate.
} GuessData;

/** Field
 *
 * A struct for tracking all of the necessary data for an agent's field.
 */
typedef struct
{
    uint8_t grid[FIELD_ROWS][FIELD_COLS];
    uint8_t smallBoatLives;
    uint8_t mediumBoatLives;
    uint8_t largeBoatLives;
    uint8_t hugeBoatLives;
} Field;

/** BoatDirection
 *
 * Declares direction constants for use with FieldAddShip.
 */
typedef enum
{
    FIELD_DIR_SOUTH,
    FIELD_DIR_EAST,
} BoatDirection;

/** BoatType
 *
 * Constants for specifying which boat the current operation refers to. This is
 * independent of the SquareStatus enum.
 */
typedef enum
{
    FIELD_BOAT_TYPE_SMALL,
    FIELD_BOAT_TYPE_MEDIUM,
    FIELD_BOAT_TYPE_LARGE,
    FIELD_BOAT_TYPE_HUGE
} BoatType;

/** BoatStatusFlag
 *
 * Track the alive state of the boats. They are arranged as mutually-exclusive
 * bits so that they can be bitwise ORed together. Used for checking the return
 * value of  `FieldGetBoatStates()`.
 *
 * For example, if a field has a MEDIUM boat and a HUGE boat, but the other two
 * boats have been sunk, then its status flag should be:
 * FIELD_BOAT_STATUS_MEDIUM | FIELD_BOAT_STATUS_HUGE, or 0b1010
 */
typedef enum
{
    FIELD_BOAT_STATUS_SMALL = 0x01,
    FIELD_BOAT_STATUS_MEDIUM = 0x02,
    FIELD_BOAT_STATUS_LARGE = 0x04,
    FIELD_BOAT_STATUS_HUGE = 0x08,
} BoatStatusFlag;

/** BoatSize
 *
 * This enum lists the number of squares, each boat occupies (and therefore, the
 * number of lives) that each boat has.
 */
typedef enum
{
    FIELD_BOAT_SIZE_SMALL = 3,
    FIELD_BOAT_SIZE_MEDIUM = 4,
    FIELD_BOAT_SIZE_LARGE = 5,
    FIELD_BOAT_SIZE_HUGE = 6
} BoatSize;

void FieldInit(Field *ownField, Field *oppField)
{
    // Declaring index variables.
    int i, j;

    // We first initialize our field, with empty squares and no modifications to lives.
    for (i = INIT_INDEX_VAL; i < FIELD_ROWS; i++)
    {
        for (j = INIT_INDEX_VAL; j < FIELD_COLS; j++)
        {
            ownField->grid[i][j] = FIELD_SQUARE_EMPTY;
        }
    }

    // Similarly, we modify our opponent's field, with unknown squares.

    for (i = INIT_INDEX_VAL; i < FIELD_ROWS; i++)
    {
        for (j = INIT_INDEX_VAL; j < FIELD_COLS; j++)
        {
            oppField->grid[i][j] = FIELD_SQUARE_UNKNOWN;
        }
    }

    // We also give our opponent's boats full lives. This is done by giving their life counters
    // the maximum possible values (size of boat).
    oppField->smallBoatLives = FIELD_BOAT_SIZE_SMALL;
    oppField->mediumBoatLives = FIELD_BOAT_SIZE_MEDIUM;
    oppField->largeBoatLives = FIELD_BOAT_SIZE_LARGE;
    oppField->hugeBoatLives = FIELD_BOAT_SIZE_HUGE;
}

SquareStatus FieldGetSquareStatus(const Field *f, uint8_t row, uint8_t col)
{
    // We first check to make sure our input is sensible, and then write
    // the status of a given point on the field, and return it.
    if ((row >= FIELD_ROWS) || (col >= FIELD_COLS))
    {
        return FIELD_SQUARE_INVALID;
    }
    else
    {
        SquareStatus status = f->grid[row][col];
        return status;
    }
}

SquareStatus FieldSetSquareStatus(Field *f, uint8_t row, uint8_t col, SquareStatus p)
{
    // Similar guarding logic. This time, the status of a field is recorded,
    // a new one is set via assignment, and the old value is returned.
    if ((row >= FIELD_ROWS) || (col >= FIELD_COLS))
    {
        return FIELD_SQUARE_INVALID;
    }

    SquareStatus old_val = f->grid[row][col];
    f->grid[row][col] = p;
    return old_val;
}

uint8_t FieldAddBoat(
    Field *ownField,
    uint8_t row,
    uint8_t col,
    BoatDirection dir,
    BoatType boatType)
{
    uint8_t boat_size;
    SquareStatus square_type;

    // Set boat size and square size based on BoatType.
    switch (boatType)
    {
    case FIELD_BOAT_TYPE_SMALL:
        boat_size = FIELD_BOAT_SIZE_SMALL;
        square_type = FIELD_SQUARE_SMALL_BOAT;
        break;
    case FIELD_BOAT_TYPE_MEDIUM:
        boat_size = FIELD_BOAT_SIZE_MEDIUM;
        square_type = FIELD_SQUARE_MEDIUM_BOAT;
        break;
    case FIELD_BOAT_TYPE_LARGE:
        boat_size = FIELD_BOAT_SIZE_LARGE;
        square_type = FIELD_SQUARE_LARGE_BOAT;
        break;
    case FIELD_BOAT_TYPE_HUGE:
        boat_size = FIELD_BOAT_SIZE_HUGE;
        square_type = FIELD_SQUARE_HUGE_BOAT;
        break;
    default:
        return STANDARD_ERROR;
    }

    // Make sure that the boat can be placed on valid squares (inside board, empty)
    for (uint8_t i = INIT_INDEX_VAL; i < boat_size; i++)
    {
        uint8_t row_checker = ((dir == FIELD_DIR_SOUTH) ? (row + i) : row);
        uint8_t col_checker = ((dir == FIELD_DIR_EAST) ? (col + i) : col);

        if ((row_checker >= FIELD_ROWS) || (col_checker) >= FIELD_COLS)
        {
            return STANDARD_ERROR;
        }
        if (ownField->grid[row_checker][col_checker] != FIELD_SQUARE_EMPTY)
        {
            return STANDARD_ERROR;
        }
    }

    // Once verified, place boat using a for loop.
    for (uint8_t i = INIT_INDEX_VAL; i < boat_size; i++)
    {
        uint8_t row_to_place = ((dir == FIELD_DIR_SOUTH) ? (row + i) : row);
        uint8_t col_to_place = ((dir == FIELD_DIR_EAST) ? (col + i) : col);
        ownField->grid[row_to_place][col_to_place] = square_type;
    }

    // Add appropriate lives.
    switch (boatType)
    {
    case FIELD_BOAT_TYPE_SMALL:
        ownField->smallBoatLives = FIELD_BOAT_SIZE_SMALL;
        break;
    case FIELD_BOAT_TYPE_MEDIUM:
        ownField->mediumBoatLives = FIELD_BOAT_SIZE_MEDIUM;
        break;
    case FIELD_BOAT_TYPE_LARGE:
        ownField->largeBoatLives = FIELD_BOAT_SIZE_LARGE;
        break;
    case FIELD_BOAT_TYPE_HUGE:
        ownField->hugeBoatLives = FIELD_BOAT_SIZE_HUGE;
        break;
    default:
        break;
    }

    return SUCCESS;
}

SquareStatus FieldRegisterEnemyAttack(Field *ownField, GuessData *opp_guess)
{
    uint8_t row_attacked = opp_guess->row;
    uint8_t col_attacked = opp_guess->col;

    SquareStatus prev_val = ownField->grid[row_attacked][col_attacked];
    // Assigns a miss if the square is empty.
    if (prev_val == FIELD_SQUARE_EMPTY)
    {
        ownField->grid[row_attacked][col_attacked] = FIELD_SQUARE_MISS;
        opp_guess->result = RESULT_MISS;
    }
    else
    {
        // If the square isn't empty, the status is update to HIT,
        // and depending on the boat type, relevant lives are decremented.
        // Then, the result is updated, and original status is returned.
        ownField->grid[row_attacked][col_attacked] = FIELD_SQUARE_HIT;
        switch (prev_val)
        {
        case FIELD_SQUARE_SMALL_BOAT:
            ownField->smallBoatLives--;
            opp_guess->result = ((ownField->smallBoatLives == NO_LIVES) ?
             (RESULT_SMALL_BOAT_SUNK) : RESULT_HIT);
            break;
        case FIELD_SQUARE_MEDIUM_BOAT:
            ownField->mediumBoatLives--;
            opp_guess->result = ((ownField->mediumBoatLives == NO_LIVES) ?
             (RESULT_MEDIUM_BOAT_SUNK) : RESULT_HIT);
            break;
        case FIELD_SQUARE_LARGE_BOAT:
            ownField->largeBoatLives--;
            opp_guess->result = ((ownField->largeBoatLives == NO_LIVES) ?
             (RESULT_LARGE_BOAT_SUNK) : RESULT_HIT);
            break;
        case FIELD_SQUARE_HUGE_BOAT:
            ownField->hugeBoatLives--;
            opp_guess->result = ((ownField->hugeBoatLives == NO_LIVES) ?
             (RESULT_HUGE_BOAT_SUNK) : RESULT_HIT);
            break;
        default:
            opp_guess->result = RESULT_HIT;
            break;
        }
    }
    return prev_val;
}

// Converse of the previous function.
SquareStatus FieldUpdateKnowledge(Field *oppField, const GuessData *own_guess)
{
    // Recording and initializing variables.
    uint8_t row_attacked = own_guess->row;
    uint8_t col_attacked = own_guess->col;
    SquareStatus prev_val = oppField->grid[row_attacked][col_attacked];

    // Update understanding of opponent's field based on result. If it's a miss,
    // we know the field is empty. Otherwise, the status is updated to hit.
    // Finally, we check if any boats have sunk. If a boat has sunk, the lives
    // of the boat are set to 0.
    if (own_guess->result == RESULT_MISS)
    {
        oppField->grid[row_attacked][col_attacked] = FIELD_SQUARE_EMPTY;
    }
    else
    {
        oppField->grid[row_attacked][col_attacked] = FIELD_SQUARE_HIT;
        switch (own_guess->result)
        {
        case RESULT_SMALL_BOAT_SUNK:
            oppField->smallBoatLives = NO_LIVES;
            break;
        case RESULT_MEDIUM_BOAT_SUNK:
            oppField->mediumBoatLives = NO_LIVES;
            break;
        case RESULT_LARGE_BOAT_SUNK:
            oppField->largeBoatLives = NO_LIVES;
            break;
        case RESULT_HUGE_BOAT_SUNK:
            oppField->hugeBoatLives = NO_LIVES;
            break;
        default:
            break;
        }
    }

    return prev_val;
}

uint8_t FieldGetBoatStates(const Field *f)
{
    // We XOR the statuses of all boats into a bit mask, which is then returned.
    uint8_t boat_states = INITIAL_STATE_VAL;
    if (f->smallBoatLives > NO_LIVES)
    {
        boat_states |= FIELD_BOAT_STATUS_SMALL;
    }
    if (f->mediumBoatLives > NO_LIVES)
    {
        boat_states |= FIELD_BOAT_STATUS_MEDIUM;
    }
    if (f->largeBoatLives > NO_LIVES)
    {
        boat_states |= FIELD_BOAT_STATUS_LARGE;
    }
    if (f->hugeBoatLives > NO_LIVES)
    {
        boat_states |= FIELD_BOAT_STATUS_HUGE;
    }

    return boat_states;
}

uint8_t FieldAIPlaceAllBoats(Field *ownField)
{
    // Initialize this for convenience.
    BoatType boat_types[FIELD_NUM_BOATS] = {
        FIELD_BOAT_TYPE_SMALL,
        FIELD_BOAT_TYPE_MEDIUM,
        FIELD_BOAT_TYPE_LARGE,
        FIELD_BOAT_TYPE_HUGE,
    };

    // Iterate over this loop until all boats are placed.
    // Picks a random row and column, and if those alongside direction are valid,
    // the boat is placed. Repeats until all boats are placed.
    for (int i = INIT_INDEX_VAL; i < FIELD_NUM_BOATS; i++)
    {
        uint8_t placed_status = STANDARD_ERROR;

        while (placed_status != SUCCESS)
        {
            uint8_t row = rand() % FIELD_ROWS;
            uint8_t col = rand() % FIELD_COLS;
            BoatDirection dir = (rand() % NUM_DIRECTIONS == INITIAL_STATE_VAL) 
            ? (FIELD_DIR_SOUTH) : FIELD_DIR_EAST;

            placed_status = FieldAddBoat(ownField, row, col, dir, boat_types[i]);
        }
    }

    return SUCCESS;
}

GuessData FieldAIDecideGuess(const Field *oppField)
{
    GuessData own_guess;
    uint8_t row_guessed, col_guessed;

    // Guess randomly, making sure that the guess hasn't been done before.
    do
    {
        // Generate a guess.
        row_guessed = rand() % FIELD_ROWS;
        col_guessed = rand() % FIELD_COLS;
    } while (FieldGetSquareStatus(oppField, row_guessed, col_guessed) != FIELD_SQUARE_UNKNOWN);

    own_guess.row = row_guessed;
    own_guess.col = col_guessed;
    own_guess.result = RESULT_MISS; // Not relevant for function.

    return own_guess;
}
