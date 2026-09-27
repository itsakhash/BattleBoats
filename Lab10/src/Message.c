/**
 * @file    Message.c
 *
 *  Message module for BattleBoats lab.
 *
 * @author  Pranav Kamat (pkamat)
 * @date    23 Aug 2026
 */

// Including relevant files.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <BOARD.h>
#include <ctype.h>
#include <stdlib.h>
#include "Message.h"

/* We also define a hexadecimal conversion factor. */
#define HEX_CONVERSION_FACTOR 16

#define PAYLOAD_TYPE_LEN 3

#define STRINGS_EQUAL_VAL 0
#define SHIFT_MESSAGE_TYPE 4
#define BASE_TEN_FACTOR 10

#define MESSAGE_NONE_VAL 0

#define CHECKSUM_TERMINATOR_SHIFT 1

#define INIT_INDEX_VAL 0

#define COMMA_STRING ','
#define START_OF_MSG '$'
#define DELIMITER_PAYLOAD_CHECKSUM '*'
#define ESCAPE_SEQUENCE '\0'

#define R_CHAR '\r'
#define N_CHAR '\n'

#define A_CHAR 'A'
#define F_CHAR 'F'

typedef enum
{
    DECODING_BEFORE_MSG, // Represents Decode before '$'
    DECODING_PAYLOAD,    // Decode between '$' and '*'
    DECODING_CHECKSUM,   // Decode of checksum
    DECODING_R_CHAR,     // Decode of '\r'
    DECODING_N_CHAR,     // Decode of '\n'
} DecodeState;

// Finally, we define payload structures.
#define PAYLOAD_TEMPLATE_CHA "CHA,%u"       // Challenge message:  		hash_a (see protocol)
#define PAYLOAD_TEMPLATE_ACC "ACC,%u"       // Accept message:	 		B (see protocol)
#define PAYLOAD_TEMPLATE_REV "REV,%u"       // Reveal message: 			A (see protocol)
#define PAYLOAD_TEMPLATE_SHO "SHO,%d,%d"    // Shot (guess) message: 	row, col
#define PAYLOAD_TEMPLATE_RES "RES,%u,%u,%u" // Result message: 			row, col, GuessResult

// This all comes together to make the larger message template.
#define MESSAGE_TEMPLATE "$%s*%02X\r\n"

/* The following function calculates a checksum for a given payload. As we want to implement
an XOR checksum, we start at 0000 0000, and compare with the i-th char in payload. This is done
using the XOR operator ^. For the binary representation, if the char of index (i+1) has a different
value of its j-th digit, then the value of the corresponding digit in the checksum flips as well. */
uint8_t Message_CalculateChecksum(const char *payload)
{
    uint8_t checksum = 0;
    size_t len = strlen(payload);
    for (size_t i = 0; i < len; i++)
    {
        checksum ^= payload[i];
    }
    return checksum;
}

/* We first check that our checksum is properly formatted. Then, we compare the checksum to the
 checksum of the payload, obtained by CalculateChecksum. Finally, we recover the type using strncmp
 and then use strtol to get each DATA_k. We then initialize message_event with appropriate values. */
int Message_ParseMessage(
    const char *payload,
    const char *checksum_string,
    BB_Event *message_event)
{
    // Checksum size comparison.
    if (strlen(checksum_string) != MESSAGE_CHECKSUM_LEN)
    {
        message_event->type = BB_EVENT_ERROR;
        return STANDARD_ERROR;
    }

    // Compare converted input checksum to checksum of payload.
    uint8_t input_checksum = (uint8_t)strtoul(checksum_string, NULL, HEX_CONVERSION_FACTOR);
    if (input_checksum != Message_CalculateChecksum(payload))
    {
        message_event->type = BB_EVENT_ERROR;
        return STANDARD_ERROR;
    }

    // All the branches are pretty similar, so I'll explain the structure once.
    // Effectively, the start of the payload string is compared to the message type strings,
    // after which it is shifted. Then, the numbers are recovered using strtol, and the 
    // values are stored using assignments.
    if (strncmp(payload, "CHA", PAYLOAD_TYPE_LEN) == STRINGS_EQUAL_VAL)
    {
        const char *p = payload + SHIFT_MESSAGE_TYPE;
        char *endptr;

        long hash_a = strtol(p, &endptr, BASE_TEN_FACTOR);

        if (endptr == p)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }

        message_event->type = BB_EVENT_CHA_RECEIVED;
        message_event->param0 = (uint16_t)hash_a;
        return SUCCESS;
    }

    else if (strncmp(payload, "ACC", PAYLOAD_TYPE_LEN) == STRINGS_EQUAL_VAL)
    {
        const char *p = payload + SHIFT_MESSAGE_TYPE;
        char *endptr;

        long B = strtol(p, &endptr, BASE_TEN_FACTOR);
        if (endptr == p)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }

        message_event->type = BB_EVENT_ACC_RECEIVED;
        message_event->param0 = (uint16_t)B;
        return SUCCESS;
    }

    else if (strncmp(payload, "REV", PAYLOAD_TYPE_LEN) == STRINGS_EQUAL_VAL)
    {
        const char *p = payload + SHIFT_MESSAGE_TYPE;
        char *endptr;

        long A = strtol(p, &endptr, BASE_TEN_FACTOR);
        if (endptr == p)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }

        message_event->type = BB_EVENT_REV_RECEIVED;
        message_event->param0 = (uint16_t)A;
        return SUCCESS;
    }

    else if (strncmp(payload, "SHO", PAYLOAD_TYPE_LEN) == STRINGS_EQUAL_VAL)
    {
        const char *p = payload + SHIFT_MESSAGE_TYPE; // Shift by message type and comma (4).
        char *endptr;

        long row = strtol(p, &endptr, BASE_TEN_FACTOR);
        if (endptr == p)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }

        p = endptr;

        if (*p != COMMA_STRING)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }
        p++; // increment to go past comma.

        long col = strtol(p, &endptr, BASE_TEN_FACTOR);
        if (endptr == p)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }

        message_event->type = BB_EVENT_SHO_RECEIVED;
        message_event->param0 = (uint16_t)row;
        message_event->param1 = (uint16_t)col;
        return SUCCESS;
    }

    else if (strncmp(payload, "RES", PAYLOAD_TYPE_LEN) == STRINGS_EQUAL_VAL)
    {
        const char *p = payload + SHIFT_MESSAGE_TYPE;
        char *endptr;

        long row = strtol(p, &endptr, BASE_TEN_FACTOR);
        if (endptr == p)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }

        p = endptr;

        if (*p != COMMA_STRING)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }
        p++; // increment to go past comma.

        long col = strtol(p, &endptr, BASE_TEN_FACTOR);
        if (endptr == p)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }

        p = endptr;
        if (*p != COMMA_STRING)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }
        p++;

        long GuessResult = strtol(p, &endptr, BASE_TEN_FACTOR);
        if (endptr == p)
        {
            message_event->type = BB_EVENT_ERROR;
            return STANDARD_ERROR;
        }

        message_event->type = BB_EVENT_RES_RECEIVED;
        message_event->param0 = (uint16_t)row;
        message_event->param1 = (uint16_t)col;
        message_event->param2 = (uint16_t)GuessResult;
        return SUCCESS;
    }

    else
    {
        message_event->type = BB_EVENT_ERROR;
        return STANDARD_ERROR;
    }
}

int Message_Encode(char *message_string, Message message_to_encode)
{
    char payload[MESSAGE_MAX_PAYLOAD_LEN];

    // The logic is relatively similar for all branches of the switch statement.
    // Effectively, based on the type of the input message, we go to a specific 
    // branch, which writes the string using templates and sprintf. 
    switch (message_to_encode.type)
    {
    case MESSAGE_NONE:
        return MESSAGE_NONE_VAL;

    case MESSAGE_CHA:
        sprintf(payload, PAYLOAD_TEMPLATE_CHA, message_to_encode.param0);
        break;

    case MESSAGE_ACC:
        sprintf(payload, PAYLOAD_TEMPLATE_ACC, message_to_encode.param0);
        break;

    case MESSAGE_REV:
        sprintf(payload, PAYLOAD_TEMPLATE_REV, message_to_encode.param0);
        break;

    case MESSAGE_SHO:
        sprintf(payload, PAYLOAD_TEMPLATE_SHO, message_to_encode.param0,
                message_to_encode.param1); // Indent to avoid exceeding 100 characters on one line.
        break;

    case MESSAGE_RES:
        sprintf(payload, PAYLOAD_TEMPLATE_RES, message_to_encode.param0,
                message_to_encode.param1, message_to_encode.param2);
        break;

    default:
        return MESSAGE_ERROR; // Raises an error if input message isn't suitable.
    }

    // After our payload string is assembled, we derive the checksum, and format the whole
    // message using another sprintf statement.
    uint8_t checksum = Message_CalculateChecksum(payload);
    sprintf(message_string, MESSAGE_TEMPLATE, payload, checksum);

    return strlen(message_string);
}

int Message_Decode(unsigned char char_in, BB_Event *decoded_message_event)
{
    // First, we initialize our variables.
    static DecodeState state = DECODING_BEFORE_MSG;
    static char payload_buf[MESSAGE_MAX_PAYLOAD_LEN];
    static char checksum_buf[MESSAGE_CHECKSUM_LEN + CHECKSUM_TERMINATOR_SHIFT];
    static int payload_index = INIT_INDEX_VAL, checksum_index = INIT_INDEX_VAL;

    decoded_message_event->type = BB_EVENT_NO_EVENT;

    switch (state)
    {
    // Ignores anything but '$'. When '$', it moves onto the next state.
    case DECODING_BEFORE_MSG:
        if (char_in == START_OF_MSG)
        {
            payload_index = INIT_INDEX_VAL;
            state = DECODING_PAYLOAD;
        }
        return SUCCESS;
    
    // This branch writes the current character into the payload buffer, terminating
    // when it reads a '*' character. Then, it transitions to the next state, in 
    // accordance with the NMEA-like definition. I also added the relevant logic
    // that filters for length and '$' or newline characters.
    case DECODING_PAYLOAD:
        if (char_in == DELIMITER_PAYLOAD_CHECKSUM)
        {
            payload_buf[payload_index] = ESCAPE_SEQUENCE;
            checksum_index = INIT_INDEX_VAL;
            state = DECODING_CHECKSUM;
        }
        else if ((char_in == START_OF_MSG) || (char_in == N_CHAR))
        {
            decoded_message_event->type = BB_EVENT_ERROR;
            state = DECODING_BEFORE_MSG;
            return STANDARD_ERROR;
        }
        else if (payload_index >= MESSAGE_MAX_PAYLOAD_LEN)
        {
            decoded_message_event->type = BB_EVENT_ERROR;
            state = DECODING_BEFORE_MSG;
            return STANDARD_ERROR;
        }
        else
        {
            payload_buf[payload_index++] = char_in;
        }
        return SUCCESS;

    // Check to see that checksum contains only hex characters. I.e., we raise an error
    // if char_in is not a digit or not a capital letter between A and F inclusive.
    case DECODING_CHECKSUM:
        if (!isdigit(char_in) && ((char_in < A_CHAR) || (char_in > F_CHAR)))
        {
            decoded_message_event->type = BB_EVENT_ERROR;
            state = DECODING_BEFORE_MSG;
            return STANDARD_ERROR;
        }
        checksum_buf[checksum_index++] = char_in;

        if (checksum_index == MESSAGE_CHECKSUM_LEN)

        {
            checksum_buf[checksum_index] = ESCAPE_SEQUENCE;
            state = DECODING_R_CHAR;
        }
        return SUCCESS;
    
    // Pretty standard logic here. Either it detects the \r character or it doesn't.
    // Raises errors until it encounters an \r character.
    case DECODING_R_CHAR:
        if (char_in == R_CHAR)
        {
            state = DECODING_N_CHAR;
        }
        else
        {
            decoded_message_event->type = BB_EVENT_ERROR;
            state = DECODING_BEFORE_MSG;
            return STANDARD_ERROR;
        }
        return SUCCESS;

    // Similar to DECODING_R_CHAR, except upon finding the newline, the message is written
    // to a string using ParseMessage, and returned, alongside the appropriate state transition.
    case DECODING_N_CHAR:
        if (char_in == N_CHAR)
        {
            int result = Message_ParseMessage(payload_buf, checksum_buf, decoded_message_event);
            state = DECODING_BEFORE_MSG;
            return result;
        }
        else
        {
            decoded_message_event->type = BB_EVENT_ERROR;
            state = DECODING_BEFORE_MSG;
            return STANDARD_ERROR;
        }

    default:
        return STANDARD_ERROR;
    }

    return STANDARD_ERROR;
}
