/**
 * @file    MessageTest.c
 *
 * Test harness for Message.c. Verifies Message_CalculateChecksum(),
 * Message_Encode(), Message_ParseMessage(), and Message_Decode()
 * strictly against what Message.h documents -- including the three
 * worked examples given in the Lab 10 manual -- so this file behaves
 * the same whether linked against Message_correct.o or the partner's
 * real Message.c.
 *
 * @author  Akhash Arjundayal (aarjunda)
 */
#include <stdio.h>
#include <string.h>
#include <BOARD.h>
#include "Message.h"

static int testsRun = 0;
static int testsPassed = 0;

#define CHECK(description, condition)                   \
      do                                                \
      {                                                 \
            testsRun++;                                 \
            if (condition)                              \
            {                                           \
                  testsPassed++;                        \
                  printf("[PASS] %s\r\n", description); \
            }                                           \
            else                                        \
            {                                           \
                  printf("[FAIL] %s\r\n", description); \
            }                                           \
      } while (0)

/** _FeedString(str, outEvent)
 * Feeds an entire NMEA message string into Message_Decode() one
 * character at a time. Checks that every character EXCEPT the last
 * reports BB_EVENT_NO_EVENT (per the Message_Decode spec), then
 * returns the final character's return code. The final event
 * (set on the last char) is left in *outEvent for the caller to check.
 */
static int _FeedString(const char *str, BB_Event *outEvent, int checkIntermediate)
{
      int len = strlen(str);
      int ret = SUCCESS;

      for (int i = 0; i < len; i++)
      {
            ret = Message_Decode((unsigned char)str[i], outEvent);
            if (checkIntermediate && i < len - 1)
            {
                  CHECK("Intermediate character reports NO_EVENT",
                        outEvent->type == BB_EVENT_NO_EVENT);
            }
      }
      return ret;
}

int main(void)
{
      BOARD_Init();
      printf("\r\n--- MessageTest starting ---\r\n");

      /* ------------------------------------------------------------
       * SECTION A: Message_CalculateChecksum()
       * Includes the manual's own worked "CAT" example, plus two
       * checksums cross-referenced against complete example messages
       * given elsewhere in the manual.
       * ------------------------------------------------------------ */
      printf("\r\n-- Section A: Message_CalculateChecksum() --\r\n");
      CHECK("Checksum('CAT') == 0x56 (manual's worked example)",
            Message_CalculateChecksum("CAT") == 0x56);
      CHECK("Checksum('SHO,2,9') == 0x5F (matches $SHO,2,9*5F example)",
            Message_CalculateChecksum("SHO,2,9") == 0x5F);
      CHECK("Checksum('RES,1,0,3') == 0x5A (matches $RES,1,0,3*5A example)",
            Message_CalculateChecksum("RES,1,0,3") == 0x5A);
      CHECK("Checksum('CHA,43182') == 0x5A (matches $CHA,43182*5A example)",
            Message_CalculateChecksum("CHA,43182") == 0x5A);
      CHECK("Checksum('') == 0 (XOR of nothing)",
            Message_CalculateChecksum("") == 0);

      /* ------------------------------------------------------------
       * SECTION B: Message_Encode()
       * Checks exact output against the manual's worked example
       * message, plus round-trip agreement with our own verified
       * checksum function for other message types.
       * ------------------------------------------------------------ */
      printf("\r\n-- Section B: Message_Encode() --\r\n");
      char buf[MESSAGE_MAX_LEN + 1];

      Message sho = {MESSAGE_SHO, 2, 9, 0};
      int shoLen = Message_Encode(buf, sho);
      CHECK("Encode(SHO,2,9) produces exactly \"$SHO,2,9*5F\\r\\n\"",
            strcmp(buf, "$SHO,2,9*5F\r\n") == 0);
      CHECK("Encode(SHO,2,9) returns the correct length",
            shoLen == (int)strlen("$SHO,2,9*5F\r\n"));

      Message res = {MESSAGE_RES, 1, 0, 3};
      Message_Encode(buf, res);
      CHECK("Encode(RES,1,0,3) produces exactly \"$RES,1,0,3*5A\\r\\n\"",
            strcmp(buf, "$RES,1,0,3*5A\r\n") == 0);

      Message cha = {MESSAGE_CHA, 43182, 0, 0};
      Message_Encode(buf, cha);
      CHECK("Encode(CHA,43182) produces exactly \"$CHA,43182*5A\\r\\n\"",
            strcmp(buf, "$CHA,43182*5A\r\n") == 0);

      Message none = {MESSAGE_NONE, 0, 0, 0};
      int noneLen = Message_Encode(buf, none);
      CHECK("Encode(MESSAGE_NONE) returns length 0",
            noneLen == 0);

      /* ------------------------------------------------------------
       * SECTION C: Message_ParseMessage()
       * Directly test payload+checksum parsing without going through
       * the char-by-char decoder.
       * ------------------------------------------------------------ */
      printf("\r\n-- Section C: Message_ParseMessage() --\r\n");
      BB_Event event;

      int ret1 = Message_ParseMessage("SHO,2,9", "5F", &event);
      CHECK("ParseMessage valid SHO returns SUCCESS", ret1 == SUCCESS);
      CHECK("ParseMessage valid SHO -> correct event type",
            event.type == BB_EVENT_SHO_RECEIVED);
      CHECK("ParseMessage valid SHO -> correct params",
            event.param0 == 2 && event.param1 == 9);

      int ret2 = Message_ParseMessage("SHO,2,9", "00", &event);
      CHECK("ParseMessage with wrong checksum returns STANDARD_ERROR",
            ret2 == STANDARD_ERROR);
      CHECK("ParseMessage with wrong checksum -> ERROR event",
            event.type == BB_EVENT_ERROR);

      int ret3 = Message_ParseMessage("SHO,2,9", "5", &event);
      CHECK("ParseMessage with a 1-char checksum returns STANDARD_ERROR",
            ret3 == STANDARD_ERROR);

      // Isolate "unrecognized message type" as a failure mode, independent
      // of checksum correctness, by computing a genuinely correct checksum
      // for a bogus payload via our already-verified checksum function.
      const char *bogusPayload = "XYZ,1,2";
      uint8_t bogusChecksum = Message_CalculateChecksum(bogusPayload);
      char checksumStr[3];
      sprintf(checksumStr, "%02X", bogusChecksum);
      int ret4 = Message_ParseMessage(bogusPayload, checksumStr, &event);
      CHECK("ParseMessage with a correctly-checksummed but unknown message type fails",
            ret4 == STANDARD_ERROR && event.type == BB_EVENT_ERROR);

      /* ------------------------------------------------------------
       * SECTION D: Message_Decode() -- full character-by-character
       * parsing, including the intermediate NO_EVENT guarantee.
       * ------------------------------------------------------------ */
      printf("\r\n-- Section D: Message_Decode() (valid messages) --\r\n");

      BB_Event decoded;
      int decodeRet = _FeedString("$SHO,2,9*5F\r\n", &decoded, 1);
      CHECK("Decode valid SHO message returns SUCCESS", decodeRet == SUCCESS);
      CHECK("Decode valid SHO -> correct event type",
            decoded.type == BB_EVENT_SHO_RECEIVED);
      CHECK("Decode valid SHO -> correct params",
            decoded.param0 == 2 && decoded.param1 == 9);

      decodeRet = _FeedString("$CHA,43182*5A\r\n", &decoded, 0);
      CHECK("Decode valid CHA message returns SUCCESS", decodeRet == SUCCESS);
      CHECK("Decode valid CHA -> correct event type and param",
            decoded.type == BB_EVENT_CHA_RECEIVED && decoded.param0 == 43182);

      decodeRet = _FeedString("$RES,1,0,3*5A\r\n", &decoded, 0);
      CHECK("Decode valid RES message returns SUCCESS", decodeRet == SUCCESS);
      CHECK("Decode valid RES -> correct event type and params",
            decoded.type == BB_EVENT_RES_RECEIVED &&
                decoded.param0 == 1 && decoded.param1 == 0 && decoded.param2 == 3);

      printf("\r\n-- Section E: Message_Decode() (corrupted messages) --\r\n");

      // Flip one checksum character in an otherwise-valid message.
      decodeRet = _FeedString("$SHO,2,9*00\r\n", &decoded, 0);
      CHECK("Decode with a bad checksum returns STANDARD_ERROR",
            decodeRet == STANDARD_ERROR);
      CHECK("Decode with a bad checksum -> ERROR event",
            decoded.type == BB_EVENT_ERROR);

      // Confirm the decoder recovers afterward and can parse a
      // subsequent valid message correctly (per the state diagram,
      // an error returns to "waiting for start delimiter").
      decodeRet = _FeedString("$SHO,4,8*58\r\n", &decoded, 0);
      CHECK("Decoder recovers after an error and parses the next valid message",
            decodeRet == SUCCESS &&
                decoded.type == BB_EVENT_SHO_RECEIVED &&
                decoded.param0 == 4 && decoded.param1 == 8);

      printf("\r\n--- MessageTest complete: %d/%d passed ---\r\n",
             testsPassed, testsRun);

      while (1)
      {
            // Idle so serial output stays visible.
      }

      return SUCCESS;
}