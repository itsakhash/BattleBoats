#ifndef FIELD_H
#define FIELD_H
/**
 * @file    Field.h
 *
 * @author  HARE Lab
 * @author  jLab
 * @version 1.0.1
 *
 * @date    1 May 2025
 */
#include <stdint.h>


/*  MODULE-LEVEL DEFINITIONS, MACROS    */

/**
 * Define the dimensions of the game field. They can be overridden by
 * compile-time specifications. All references to the dimensions of the field
 * should use these constants instead of hard-coding a numeric value so that the
 * field dimensions can be changed with minimal coding changes.
 */
#ifndef FIELD_COLS
#define FIELD_COLS 10
#endif
#ifndef FIELD_ROWS
#define FIELD_ROWS 6
#endif

/** SquareStatus
 *
 * Set different constants used for conveying different information about the
 * different locations of the field. These values should be used for the actual
 * storage of the field state, which is almost every usage. For displaying the
 * field using FieldPrint(), the SquareStatus Display enum values should be used
 * instead.
 */
typedef enum {
    // These denote field positions useful for representing the local board.
    FIELD_SQUARE_EMPTY = 0, 	// An empty field position.
    FIELD_SQUARE_SMALL_BOAT, 	// This position contains part of the small
                                //  boat.
    FIELD_SQUARE_MEDIUM_BOAT, 	// This position contains part of the medium
                                //  boat.
    FIELD_SQUARE_LARGE_BOAT, 	// This position contains part of the large
                                //  boat.
    FIELD_SQUARE_HUGE_BOAT, 	// This position contains part of the huge boat.

    // These denote field positions useful for representing the enemy's board.
    FIELD_SQUARE_UNKNOWN, 		// It is unknown what is here. Useful for
                                //  denoting a position on the enemy's board 
                                //  that hasn't been checked.

    // These statuses may be used on either field:    
    FIELD_SQUARE_HIT,			// A field position that was attacked and
                                //  contained part of a boat.
    FIELD_SQUARE_MISS, 			// This position was attacked by the enemy, but
                                //  was empty.

    // This may be useful for implementing extra-credit features:
    FIELD_SQUARE_CURSOR, 		// This is used merely for use in FieldOled.c
                                //  for indicating the current cursor when
                                //  selecting a position to attack.
    FIELD_SQUARE_INVALID,       // Occasionally, it may be necessary to
                                //  indicate an error using a square status.
} SquareStatus;

/** ShotResult
 *
 * These are the possible results of shots:
 */
typedef enum {
    RESULT_MISS,                // 0
    RESULT_HIT,                 // 1
    RESULT_SMALL_BOAT_SUNK,     // 2
    RESULT_MEDIUM_BOAT_SUNK,    // 3
    RESULT_LARGE_BOAT_SUNK,     // 4
    RESULT_HUGE_BOAT_SUNK,      // 5
} ShotResult;

/** GuessData
 *
 * GuessData is used for exchanging coordinate data along with information about
 * of coordinate.
 */
typedef struct {
    uint8_t row;        // Row of the coordinate that was guessed.
    uint8_t col;        // Column of the coordinate guessed.
    ShotResult result;  // Result of a shot at this coordinate.
} GuessData;

/** Field
 *
 * A struct for tracking all of the necessary data for an agent's field.
 */
typedef struct {
    uint8_t grid[FIELD_ROWS][FIELD_COLS];
    uint8_t smallBoatLives;
    uint8_t mediumBoatLives;
    uint8_t largeBoatLives;
    uint8_t hugeBoatLives;
} Field;

/**
 * Specify how many boats there exist on the field. There is 1 boat of each of
 * the 4 types, so 4 total.
 */
#define FIELD_NUM_BOATS 4

/** BoatDirection
 *
 * Declares direction constants for use with FieldAddShip.
 */
typedef enum {
    FIELD_DIR_SOUTH,
    FIELD_DIR_EAST,
} BoatDirection;

/** BoatType
 *
 * Constants for specifying which boat the current operation refers to. This is
 * independent of the SquareStatus enum.
 */
typedef enum {
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
typedef enum {
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
typedef enum {
    FIELD_BOAT_SIZE_SMALL = 3,
    FIELD_BOAT_SIZE_MEDIUM = 4,
    FIELD_BOAT_SIZE_LARGE = 5,
    FIELD_BOAT_SIZE_HUGE = 6
} BoatSize;


/*  PROTOTYPES  */

/** FieldPrint_UART(*ownField, *oppField)
 *
 * This function is optional, but recommended. It prints a representation o both
 * fields, similar to the OLED display.
 *
 * @param   f   The field to initialize.
 * @param   p   The data to initialize the entire field to, should be a member
 *              of enum SquareStatus.
 */
void FieldPrint_UART(Field *ownField, Field * oppField);

/** FieldInit(*ownField, *oppField)
 *
 * FieldInit() initializes two passed Field structs for the beginning of play.
 * Each field's grid should be filled with the appropriate SquareStatus (
 * FIELD_SQUARE_EMPTY for your own field, FIELD_SQUARE_UNKNOWN for opponent's).
 * Additionally, your opponent's field's boatLives parameters should be filled
 *  (your own field's boatLives will be filled when boats are added).
 * 
 * FieldAI_PlaceAllBoats() should NOT be called in this function.
 *
 * For the enemy Field, we start by giving them full lives.
 * For our Field (which does not yet have boats placed on it), we set boat
 * lives on boat placement.
 * 
 * @param   *ownField   A field representing the agents own ships.
 * @param   *oppField   A field representing the opponent's ships.
 */
void FieldInit(Field *ownField, Field * oppField);

/** FieldGetSquareStatus(*f, row, col)
 *
 * Retrieves the value at the specified field position.
 *
 * @param   *f  The Field being referenced.
 * @param   row The row-component of the location to retrieve
 * @param   col The column-component of the location to retrieve
 * @return  FIELD_SQUARE_INVALID if row and col are not valid field locations
 *          Otherwise, return the status of the referenced square.
 */
SquareStatus FieldGetSquareStatus(const Field *f, uint8_t row, uint8_t col);

/** FieldSetSquareStatus(*f, row, col, p)
 *
 * This function provides an interface for setting individual locations within a
 * Field struct. This is useful when FieldAddBoat() doesn't do exactly what you
 * need. For example, if you'd like to use FIELD_SQUARE_CURSOR, this is the
 * function to use.
 * 
 * @param   *f  The Field to modify.
 * @param   row The row-component of the location to modify.
 * @param   col The column-component of the location to modify.
 * @param   p   The new value of the field location.
 * @return  The old value at that field location.
 */
SquareStatus FieldSetSquareStatus(
        Field *f,
        uint8_t row,
        uint8_t col,
        SquareStatus p);

/** FieldAddBoat(*ownField, row, col, dir, boatType)
 *
 * FieldAddBoat() places a single ship on the player's field based on arguments
 * 2-5. Arguments 2, 3 represent the x, y coordinates of the pivot point of the
 * ship.  Argument 4 represents the * direction of the ship, and argument 5 is
 * the length of the ship being placed. 
 * 
 * All spaces that the boat would occupy are checked to be clear before the
 * field is modified so that if the boat can fit in the desired position, the
 * field is modified and SUCCESS is returned. Otherwise the field is unmodified
 * and STANDARD_ERROR is returned. There is no hard-coded limit to how many
 * times a boat can be added to a field within this function.
 * 
 * In addition, this function should update the appropriate boatLives parameter
 * of the field.
 *
 * So this is valid test code:
 * {
 *   Field myField;
 *   FieldInit(&myField, FIELD_SQUARE_EMPTY);
 *   FieldAddBoat(&myField, 0, 0, FIELD_BOAT_DIRECTION_EAST, FIELD_BOAT_TYPE_SMALL);
 *   FieldAddBoat(&myField, 1, 0, FIELD_BOAT_DIRECTION_EAST, FIELD_BOAT_TYPE_MEDIUM);
 *   FieldAddBoat(&myField, 1, 0, FIELD_BOAT_DIRECTION_EAST, FIELD_BOAT_TYPE_HUGE);
 *   FieldAddBoat(&myField, 0, 6, FIELD_BOAT_DIRECTION_SOUTH, FIELD_BOAT_TYPE_SMALL);
 * }
 *
 * should result in a field like:
 *      0 1 2 3 4 5 6 7 8 9
 *     ---------------------
 *  0 [ 1 1 1 . . . 1 . . . ]
 *  1 [ 2 2 2 2 . . 1 . . . ]
 *  2 [ . . . . . . 1 . . . ]
 *  3 [ . . . . . . . . . . ]
 *  4 [ . . . . . . . . . . ]
 *  5 [ . . . . . . . . . . ]
 *     
 * @param   f           The field to grab data from.
 * @param   row         The row that the boat will start from, valid range is
 *                      from 0 and to FIELD_ROWS - 1.
 * @param   col         The column that the boat will start from, valid range is
 *                      from 0 and to FIELD_COLS - 1.
 * @param   dir         The direction that the boat will face once places, from
 *                      the BoatDirection enum.
 * @param   boatType    The type of boat to place. Relies on the
 *                      FIELD_SQUARE_*_BOAT values from the SquareStatus enum.
 * @return  SUCCESS for success, STANDARD_ERROR for failure.
 */
uint8_t FieldAddBoat(
        Field *ownField,
        uint8_t row,
        uint8_t col,
        BoatDirection dir,
        BoatType boatType);

/** FieldRegisterEnemyAttack(*ownField, *opp_guess)
 *
 * This function registers an attack at the gData coordinates on the provided
 * field. This means that 'f' is updated with a FIELD_SQUARE_HIT or
 * FIELD_SQUARE_MISS depending on what was at the coordinates indicated in
 * 'gData'. 'gData' is also updated with the proper HitStatus value depending on
 * what happened AND the value of that field position BEFORE it was attacked.
 * Finally this function also reduces the lives for any boat that was hit from
 * this attack.
 *
 * @param   f       The field to check against and update.
 * @param   gData   The coordinates that were guessed. The result is stored in
 *                  gData->result as an output.  The result can be a RESULT_HIT,
 *                  RESULT_MISS, or RESULT_***_SUNK.
 * @return  The data that was stored at the field position indicated by gData
 *          before this attack.
 */
SquareStatus FieldRegisterEnemyAttack(Field *ownField, GuessData *opp_guess);

/** FieldUpdateKnowledge(*oppField, *own_guess)
 *
 * This function updates the FieldState representing the opponent's game board 
 * with whether the guess indicated within gData was a hit or not. If it was a
 * hit, then the field is updated with a FIELD_SQUARE_HIT at that position. If
 * it was a miss, display a FIELD_SQUARE_EMPTY instead, as it is now known that
 * there was no boat there. The FieldState struct also contains data on how many
 * lives each ship has. Each hit only reports if it was a hit on any boat or if
 * a specific boat was sunk, this function also clears a boats lives if it
 * detects that the hit was a RESULT_*_BOAT_SUNK.
 *
 * @param   *f      The field to grab data from.
 * @param   gData   The coordinates that were guessed along with their
 *                  HitStatus.
 * @return  The previous value of that coordinate position in the field before
 *          the hit/miss was registered.
 */
SquareStatus FieldUpdateKnowledge(Field *oppField, const GuessData *own_guess);

/** FieldGetBoatStates(*f)
 *
 * This function returns the alive states of all 4 boats as a 4-bit bitfield
 * (stored as a uint8). The boats are ordered from smallest to largest starting
 * at the least-significant bit. So that: 0b00001010 indicates that the small
 * boat and large boat are sunk, while the medium and huge boat are still alive.
 *
 * See the BoatStatus enum for the bit arrangement.
 *
 * @param   *f  The field to grab data from.
 * @return  A 4-bit value with each bit corresponding to whether each ship is
 *          alive or not.
 */
uint8_t FieldGetBoatStates(const Field *f);

/** FieldAIPlaceAllBoats(*ownField)
 *
 * This function is responsible for placing all four of the boats on a field.
 * 
 * @param   f   Agent's own field, to be modified in place.
 * @return  SUCCESS if all boats could be placed, STANDARD_ERROR otherwise.
 * 
 * This function should never fail when passed a properly initialized field!
 */
uint8_t FieldAIPlaceAllBoats(Field *ownField);

/** FieldAIDecideGuess(*oppField)
 *
 * Given a field, decide the next guess.
 *
 * This function should not attempt to shoot a square which has already been
 * guessed.
 *
 * You may wish to give this function static variables.  If so, that data should
 * be reset when FieldInit() is called.
 * 
 * @param   *f  An opponent's field.
 * @return  A GuessData struct whose row and col parameters are the coordinates
 *          of the guess.  The result parameter is irrelevant.
 */
GuessData FieldAIDecideGuess(const Field *oppField);

/************************************************************
 * FOR EXTRA CREDIT:  Make the two "AI" functions above     *
 * smart enough to beat our AI in more than 55% of games.   *
 ************************************************************/


#endif // FIELD_H
