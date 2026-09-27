#ifndef BATTLEBOATS_H
#define BATTLEBOATS_H
/**
 * BattleBoats.h
 *
 * This file contains standard structures that are used 
 *
 * @author  HARE Lab
 * @author  jLab
 * @version 1.0.0
 *
 * @date    29 Mar 2025
 */
#include <stdint.h>


/** BB_EventType
 */
typedef enum {
    BB_EVENT_NO_EVENT,      //0
    BB_EVENT_START_BUTTON,  //1
    BB_EVENT_RESET_BUTTON,  //2
    BB_EVENT_CHA_RECEIVED,  //3
    BB_EVENT_ACC_RECEIVED,  //4
    BB_EVENT_REV_RECEIVED,  //5
    BB_EVENT_SHO_RECEIVED,  //6
    BB_EVENT_RES_RECEIVED,  //7
    BB_EVENT_MESSAGE_SENT,  //8
    BB_EVENT_ERROR,         //9

    //the following events are only used for human agents:
    BB_EVENT_SOUTH_BUTTON,  //10
    BB_EVENT_EAST_BUTTON    //11

} BB_EventType;

/** BB_Event
 *
 * All BB events use this struct:
 */
typedef struct {
    BB_EventType type;
    uint16_t param0;        // Defined in Message.h.
    uint16_t param1;
    uint16_t param2;
} BB_Event;

/** BB_Error
 *
 * Used to signal different types of errors as the param0
 * of a BattleBoat Error event.  You are not required to utilize these,
 * but they can make error checking much more effective.
 */
typedef enum {
    BB_SUCCESS = 0,                     //0
    BB_ERROR_BAD_CHECKSUM,              //1
    BB_ERROR_PAYLOAD_LEN_EXCEEDED,      //2
    BB_ERROR_CHECKSUM_LEN_EXCEEDED,     //3
    BB_ERROR_CHECKSUM_LEN_INSUFFICIENT, //4
    BB_ERROR_INVALID_MESSAGE_TYPE,      //5
    BB_ERROR_MESSAGE_PARSE_FAILURE      //6
} BB_Error;


#endif // BATTLEBOATS_H
