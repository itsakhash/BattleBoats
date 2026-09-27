/**
 * @file    Negotiation.c
 *
 * Implements the commitment-scheme coin flip used to fairly decide
 * turn order between two BattleBoats agents.
 *
 * @author  Akhash Arjundayal (aarjunda)
 */
#include "Negotiation.h"

/** NegotiationHash(secret)
 *
 * Implements the "Beef Hash": (secret^2) mod 0xBEEF.
 * This is a one-way function: easy to compute, hard to invert.
 */
NegotiationData NegotiationHash(NegotiationData secret)
{
    // Cast to a wider type before squaring so we don't overflow/wrap
    // a 16-bit value during the multiplication.
    uint32_t squared = (uint32_t)secret * (uint32_t)secret;
    return (NegotiationData)(squared % PUBLIC_KEY);
}

/** NegotiationVerify(secret, commitment)
 *
 * Confirms that hashing 'secret' reproduces 'commitment'.
 * Used by the accepting agent to check that the challenging agent
 * didn't change their secret number after committing to it.
 */
int NegotiationVerify(NegotiationData secret, NegotiationData commitment)
{
    return (NegotiationHash(secret) == commitment);
}

/** NegotiateCoinFlip(A, B)
 *
 * Determines HEADS/TAILS from the bit-parity of (A XOR B).
 * Parity is 1 (odd number of 1-bits) -> HEADS, otherwise TAILS.
 */
NegotiationOutcome NegotiateCoinFlip(NegotiationData A, NegotiationData B)
{
    NegotiationData xorResult = A ^ B;
    uint8_t parity = 0;

    // NegotiationData is a uint16_t, so check all 16 bits.
    for (int i = 0; i < 16; i++)
    {
        if (xorResult & (1 << i))
        {
            parity ^= 1;
        }
    }

    return (parity == 1) ? HEADS : TAILS;
}

/*
 * NegotiateGenerateBGivenHash() and NegotiateGenerateAGivenB() are
 * OPTIONAL extra-credit functions for a "cheating" agent. Left
 * unimplemented for now — only needed if we decide to pursue the
 * negotiation-cheating extra credit item (separate from the Field AI
 * extra credit).
 */