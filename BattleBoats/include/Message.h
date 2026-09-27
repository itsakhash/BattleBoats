#ifndef MESSAGE_H
#define MESSAGE_H
/**
 * @file    Message.h
 *
 * Message module header file.
 *
 * @author  HARE Lab
 * @author  jLab
 * @version 1.0.1
 *
 * @date    26 Apr 2025
 */
#include <stdint.h>

#ifndef BOARD_H
#include <BOARD.h>
#endif  /*  BOARD_H */

#include "BattleBoats.h"


/**
 * According to the NMEA standard, messages cannot be longer than 82,
 * including the delimiters $, *, \r, and \n.
 */
#define MESSAGE_MAX_LEN 82

/**
 * The maximum payload length is the maximum message length,
 * -1 start delimiter ($),
 * -1 checksum delimiter (*),
 * -2 checksum characters, 
 * -1 end delimiter #1 (\r), and
 * -1 end delimiter #2 (\n).
 */
#define MESSAGE_MAX_PAYLOAD_LEN (MESSAGE_MAX_LEN - 1 - 1 - 2 - 1 - 1)

/* NMEA also defines a specific  checksum length. */
#define MESSAGE_CHECKSUM_LEN 2

/** MessageType
 * The types of messages that can be sent or received:
 */
typedef enum {
    MESSAGE_NONE, // Used if no message is to be sent.
    MESSAGE_CHA,
    MESSAGE_ACC,
    MESSAGE_REV,
    MESSAGE_SHO,
    MESSAGE_RES,
    // While not required, an error message can be a useful debugging tool:
    MESSAGE_ERROR = -1
} MessageType;

/** Message
 * Messages follow this struct:
 */
typedef struct {
    MessageType type;
    unsigned int param0;
    unsigned int param1;
    unsigned int param2;
} Message;


/** 
 * Each (almost*) follows the NMEA0183 syntax for message payloads:  
 *		The first three characters describe the message type;
 * 		zero or more comma-separated fields follow, containing various kinds of
 * 		    data.
 *
 * (* True NEMA0183 payloads begin with two characters that describe the
 *    "talker", which we omit from the BattleBoats protocol.)
 */
#define PAYLOAD_TEMPLATE_CHA "CHA,%u" 		// Challenge message:  		hash_a (see protocol)
#define PAYLOAD_TEMPLATE_ACC "ACC,%u" 		// Accept message:	 		B (see protocol)
#define PAYLOAD_TEMPLATE_REV "REV,%u"       // Reveal message: 			A (see protocol)
#define PAYLOAD_TEMPLATE_SHO "SHO,%d,%d"    // Shot (guess) message: 	row, col
#define PAYLOAD_TEMPLATE_RES "RES,%u,%u,%u" // Result message: 			row, col, GuessResult


/** 
 * NEMA0183 messages wrap the payload with a start delimiter, a checksum to
 * verify the contents of the message in case of transmission errors, and an end
 * delimiter.
 * This template defines the wrapper.
 * Note that it uses printf-style tokens so that it can be used with sprintf().
 * 
 * Here is an example message:
 *                 
 * 1 start delimiter    (a literal $)
 * 2 payload            (any string, represented by %s in the template)
 * 3 checksum delimiter (a literal *)
 * 4 checksum			(two ascii characters representing hex digits, %02x in
 *                       the template)
 * 5 end delimiter      (a literal \r\n)
 * 
 * example message:      1       3  5      
 *						 v       v  vvvv
 *                       $SHO,2,9*5F\r\n
 *                        ^^^^^^^ ^^      
 *                          2     4      
 * 
 * Note that 2 and 4 correspond to %s and %02x in the template.
 * 
 * Also note that valid BattleBoats messages use 
 * strictly upper-case letters, so $SHO,2,3*5f\r\n is an invalid message.
 */
#define MESSAGE_TEMPLATE "$%s*%02X\r\n"

/**
 * Given a payload string, calculate its checksum.
 * 
 * @param   payload The string whose checksum we wish to calculate.
 * @return  The resulting 8-bit checksum.
 */
uint8_t Message_CalculateChecksum(const char* payload);

/** Message_ParseMessage(*payload, *checksum_string, *message_event)
 *
 * ParseMessage() converts a message string into a BB_Event.  The payload and
 * checksum of a message are passed into ParseMessage(), and it modifies a
 * BB_Event struct in place to reflect the contents of the message.
 * 
 * @param   payload         The payload of a message.
 * @param   checksum        The checksum (in string form) of  a message,
 *                              should be exactly 2 chars long, plus a null
 *                              char.
 * @param   message_event   A BB_Event which will be modified by this function.
 *                          If the message could be parsed successfully,
 *                              message_event's type will correspond to the
 *                              message type and its parameters will match the
 *                              message's data fields.
 *                          If the message could not be parsed, message_events
 *                              type will be BB_EVENT_ERROR.
 * 
 * @return  STANDARD_ERROR if:
 *              the payload does not match the checksum,
 *              the checksum string is not two characters long, or
 *              the message does not match any message template;
 *          SUCCESS otherwise.
 * 
 * @note    Please note!  sscanf() has a couple compiler bugs that make it an
 *          unreliable tool for implementing this function.
 */
int Message_ParseMessage(
        const char* payload,
        const char* checksum_string,
        BB_Event * message_event
        );

/** Message_Encode(*message_string, message_to_encode)
 *
 * Encodes the coordinate data for a guess into the string `message`. This
 * string must be big enough to contain all of the necessary data. The format is
 * specified in PAYLOAD_TEMPLATE_*, which is then wrapped within the message as
 * defined by MESSAGE_TEMPLATE. 
 * 
 * The final length of this message is then returned. There is no failure mode
 * for this function as there is no checking for NULL pointers.
 * 
 * @param   message             The character array used for storing the output. 
 *                                  Must be long enough to store the entire 
 *                                  string, see MESSAGE_MAX_LEN.
 * @param   message_to_encode   A message to encode
 * @return  The length of the string stored into 'message_string'. Return 0 if
 *          message type is MESSAGE_NONE.
 */
int Message_Encode(char *message_string, Message message_to_encode);

/** Message_Decode(char_in, *decoded_message_event)
 *
 * Message_Decode reads one character at a time.  If it detects a full NMEA
 * message, it translates that message into a BB_Event struct, which can be
 * passed to other services.
 * 
 * @param   char_in         The next character in the NMEA0183 message to be
 *                              decoded.
 * @param   decoded_message A pointer to a message struct, used to "return" a
 *                              message:
 *                          If char_in is the last character of a valid message, 
 *                              then decoded_message
 *                              should have the appropriate message type.
 *                          If char_in is the last character of an invalid
 *                              message,
 *                              then decoded_message should have an ERROR type.
 *                          otherwise, it should have type NO_EVENT.
 * @return  SUCCESS if no error was detected,
 *          STANDARD_ERROR if an error was detected.
 * 
 * @note    ANY call to Message_Decode may modify decoded_message.
 * @todo    Make "returned" event variable name consistent.
 */
int Message_Decode(unsigned char char_in, BB_Event * decoded_message_event);


#endif // MESSAGE_H
