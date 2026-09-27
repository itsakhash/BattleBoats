/**
 * @file    Buttons.c
 *
 * Implementation of the debounced button event library, as specified by
 * Buttons.h. Tracks each button's stable state in software and only reports
 * an UP/DOWN event once a new reading has held steady for
 * BUTTONS_DEBOUNCE_PERIOD consecutive calls to Buttons_CheckEvents().
 *
 * @author  Akhash Arjundayal (aarjunda)
 *
 * @date    July 27 2026
 */

#include <stdio.h>
#include <stdlib.h>
#include "Buttons.h"
#include "BOARD.h"

// per-button debounce bookkeeping: last stable reading and how many
// consecutive calls the current raw reading has matched a candidate change
static uint8_t stableState = 0x00;
static uint8_t candidateState = 0x00;
static uint8_t candidateCount = 0;

void Buttons_Init(void)
{
    // enable clocks and configure BTN1/2/3 (PC4, PC5, PC12) and BTN4 (PD2) as inputs
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // stable/candidate tracking starts from "all buttons up", per the spec
    stableState = 0x00;
    candidateState = 0x00;
    candidateCount = 0;
}

uint8_t Buttons_End(void)
{
    return 0;
}

uint8_t Buttons_CheckEvents(void)
{
    uint8_t events = BUTTON_EVENT_NONE;

    // read the raw hardware state exactly once per call, as required.
    // buttons are wired active-low (idle = 1, pressed = 0), so invert here
    // to keep the rest of this function's convention as 1 = pressed.
    uint8_t rawState = (~BUTTON_STATES()) & 0x0F;

    if (rawState == stableState)
    {
        // no change from the last known-good reading; reset any in-progress candidate
        candidateState = stableState;
        candidateCount = 0;
        return events;
    }

    if (rawState == candidateState)
    {
        // still matches the change we're watching; count another stable reading
        candidateCount++;
    }
    else
    {
        // a different reading than what we were debouncing; restart the count
        candidateState = rawState;
        candidateCount = 1;
    }

    if (candidateCount >= BUTTONS_DEBOUNCE_PERIOD)
    {
        // the candidate reading has held steady long enough to be a real event
        uint8_t changedBits = stableState ^ candidateState;

        if (changedBits & BUTTON_STATE_1)
        {
            events |= (candidateState & BUTTON_STATE_1) ? BUTTON_EVENT_1DOWN : BUTTON_EVENT_1UP;
        }
        if (changedBits & BUTTON_STATE_2)
        {
            events |= (candidateState & BUTTON_STATE_2) ? BUTTON_EVENT_2DOWN : BUTTON_EVENT_2UP;
        }
        if (changedBits & BUTTON_STATE_3)
        {
            events |= (candidateState & BUTTON_STATE_3) ? BUTTON_EVENT_3DOWN : BUTTON_EVENT_3UP;
        }
        if (changedBits & BUTTON_STATE_4)
        {
            events |= (candidateState & BUTTON_STATE_4) ? BUTTON_EVENT_4DOWN : BUTTON_EVENT_4UP;
        }

        stableState = candidateState;
        candidateCount = 0;
    }

    return events;
}