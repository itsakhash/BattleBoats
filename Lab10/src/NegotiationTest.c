/**
 * @file    NegotiationTest.c
 *
 *  Test harness for the Negotiation module.
 *
 * @author  Pranav Kamat (pkamat)
 *
 * @date    26 Aug 2026
 */

#include <stdio.h>
#include <stdint.h>
#include <BOARD.h>
#include "Negotiation.h"

// Macros

#define NO_TESTS 0
#define NO_TESTS_DEC 0.0
#define DEC_FACTOR 100.0

#define TEST_RESULT_PASS "PASS | %s\r\n"
#define TEST_RESULT_FAIL "FAIL | %s\r\n"
#define OVERALL_STATS "Tests passed: %d/%d (%.1f)\r\n"

// Helper functions, standardized.
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

void OverallStats(void)
{
    double pass_rate = (testsRun > NO_TESTS) ? (DEC_FACTOR * testsPassed / testsRun) : NO_TESTS_DEC;

    printf(OVERALL_STATS, testsPassed, testsRun, pass_rate);
}

// NegotiationHash tests.
static void TestNegotiationHash(void)
{
    // Check whether Hash works on known values.
    CHECK("NegotiationHash works on first pair of known values", NegotiationHash(3) == 9);
    CHECK("NegotiationHash works on second pair of known values", NegotiationHash(12345) == 43182);

    // Check that 0 hashes to 0
    CHECK("Hash of 0 is 0", NegotiationHash(0) == 0);

    // Check that hash handles any uint16_t input. This is done by passing largest possible
    // uint16_t value. Its expected hash is 34011, computed using a python script.
    CHECK("Hash works on all 16 bit integers", NegotiationHash(65535) == 34011);

    // Checking that the hash is always the same.
    CHECK("Hash is deterministic", NegotiationHash(500) == NegotiationHash(500));

    // Hash generally gives different outputs for different inputs.
    CHECK("Hash gives different outputs for different inputs (generally)",
          NegotiationHash(500) != NegotiationHash(501));
}

// NegotationVerify tests.
static void TestNegotiationVerify(void)
{
    // Checking that Verify works for known pairs.
    CHECK("NegotiationVerify works for the known pair (3,9)", NegotiationVerify(3, 9) == 1);
    CHECK("NegotiationVerify works for the known pair (12345, 43182)",
          NegotiationVerify(12345, 43182) == 1);

    // Checking that verification fails for pairs that are known to be false. In this case
    // I just took a known pair and modified the second value.
    CHECK("NegotiationVerify correctly fails to verify an incorrect pair (3, 10)",
          NegotiationVerify(3, 10) == 0);
    CHECK("NegotiationVerify correctly fails to verify an incorrect pair (12345, 54321)",
          NegotiationVerify(12345, 54321) == 0);

    // Then, we check that the hash function and Verify are consistent.
    NegotiationData secret = 777;
    NegotiationData correctHash = NegotiationHash(secret);
    CHECK("NegotiationVerify agrees with NegotiationHash",
          NegotiationVerify(secret, correctHash) == 1);
    CHECK("NegotiationVerify correctly rejects shifted hash",
          NegotiationVerify(secret, (NegotiationData)(correctHash + 1)) == 0);
}

// Finally, we test NegotiationCoinFlip.

static void TestNegotiateCoinFlip(void)
{
    // A XOR A == 0 gives expected output TAILS.
    CHECK("NegotiateCoinFlip(x, x) gives tails", NegotiateCoinFlip(1234, 1234) == TAILS);

    // Single set bit gives HEADS.
    CHECK("NegotiateCoinFlip(0, 1) gives heads", NegotiateCoinFlip(0, 1) == HEADS);

    // Two set bits give TAILS.
    CHECK("NegotiateCoinFlip(0,3) gives expeted output TAILS",
          NegotiateCoinFlip(0, 3) == TAILS);

    // All 16 bits set gives TAILS.
    CHECK("NegotiateCoinFlip(0, 0xFFFF) gives expected output TAILS",
          NegotiateCoinFlip(0, 0xFFFF) == TAILS);

    // 15 bits set gives HEADS.
    CHECK("NegotiateCoinFlip(0, 0x7FFF) gives expected output HEADS",
          NegotiateCoinFlip(0, 0x7FFF) == HEADS);

    // XOR is commutative.
    CHECK("NegotiationCoinFlip is commutative",
          NegotiateCoinFlip(48710, 0) == NegotiateCoinFlip(0, 48710));
}

int main(void)
{
    BOARD_Init();
    printf("\r\nBeginning pkamat's Negotiation test harness.\r\n");

    TestNegotiationHash();
    TestNegotiationVerify();
    TestNegotiateCoinFlip();

    printf("\r\n");
    OverallStats();

    return (testsPassed == testsRun) ? 0 : 1;
}