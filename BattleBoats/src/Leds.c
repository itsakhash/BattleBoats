/**
 * @file    Leds.c
 *
 * This library provides an interface for controlling the LEDs on the UCSC
 * Nucleo I/O Shield development board.
 *
 * @author  Akhash Arjundayal (aarjunda)
 *
 * @date    July 26 2026
 */
// Standard libraries.
#include <stdio.h>
#include <stdlib.h>

// Course libraries.
#include <Leds.h>

/**
 * LEDs_Init() Initializes the LED bar by doing three things:
 *      1) Enables usage of the GPIO clocks for needed ports.
 *      2) Use the appropriate SFRs to set each LED pin to "output" mode.
 *      3) Use the appropriate SFRs to set each LED pin's output value to 0 (low
 *         voltage).
 * After calling LEDs_Init(), the other functions in this file can be used to
 * manipulate the LED bar.
 */
int8_t LEDs_Init(void)
{
    // Enable GPIO clocks for ports C and B.
#ifdef STM32F4
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Init GOIO output pins for LEDs.
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif             /*  STM32F4 */
    LEDs_Set(0x0); // Reset LEDs.

    return SUCCESS;
}

/**
 * LEDs_Set() controls the output on the LED bar.  Input values are 8-bit
 * patterns, where each bit describes the desired state of the corresponding
 * LED. So, for example, LEDs_Set(0x80) should  leave the first LED on, and the
 * last 7 LEDs off. LEDs_Set(0x1F) should turn off the first three LEDs and turn
 * on the remaining five LEDs.
 *
 * @param newPattern:  An 8-bit value describing the desired output on the LED
 * bar.
 *
 * LEDs_Set should not change any LED pins to inputs.
 */
void LEDs_Set(uint8_t newPattern)
{
#ifdef STM32F4
    /***************************************************************************
     * Your code goes in between this comment and the following one with
     * asterisks.
     **************************************************************************/
    // LD1-LD4 (bits 7-4) -> PC8-PC11, bit-reversed since LD1 is the MSB but the lowest pin
    uint8_t cTopBits = (newPattern >> 4) & 0x0F;
    uint8_t cPins = ((cTopBits & 0x08) >> 3) | ((cTopBits & 0x04) >> 1) |
                    ((cTopBits & 0x02) << 1) | ((cTopBits & 0x01) << 3);

    // LD5-LD8 (bits 3-0) -> PB0-PB3, same bit-reversal
    uint8_t bBottomBits = newPattern & 0x0F;
    uint8_t bPins = ((bBottomBits & 0x08) >> 3) | ((bBottomBits & 0x04) >> 1) |
                    ((bBottomBits & 0x02) << 1) | ((bBottomBits & 0x01) << 3);

    // BSRR: low 16 bits set a pin, high 16 bits reset it - one atomic write per port
    uint32_t cSet = (cPins & 0x0F) << 8;
    uint32_t cReset = ((~cPins) & 0x0F) << 8;
    GPIOC->BSRR = cSet | (cReset << 16);

    uint32_t bSet = bPins & 0x0F;
    uint32_t bReset = (~bPins) & 0x0F;
    GPIOB->BSRR = bSet | (bReset << 16);
    /***************************************************************************
     * Your code goes in between this comment and the preceding one with
     * asterisks.
     **************************************************************************/
#endif /*  STM32F4 */
}

/**
 * LEDs_Get() returns the current state of the LED bar.  Return values are 8-bit
 * patterns, where each bit describes the current state of the corresponding
 * LED. So, for example, if the LED bar's LED's are
 *
 * [ON OFF ON OFF   OFF ON OFF ON],
 *
 * LEDs_Get() should return 0xA5.
 *
 * @return  (uint8_t)   An 8-bit value describing the current output on the LED
 *                      bar.
 *
 * LEDs_Get() should not change the state of the LEDs, or any SFRs.
 */
uint8_t LEDs_Get(void)
{
    uint8_t ledState = 0x00;
#ifdef STM32F4
    /***************************************************************************
     * Your code goes in between this comment and the following one with
     * asterisks.
     **************************************************************************/
    uint16_t cOdr = GPIOC->ODR;
    uint16_t bOdr = GPIOB->ODR;

    uint8_t cPins = (cOdr >> 8) & 0x0F; // PC8-PC11 -> LD1-LD4
    uint8_t bPins = bOdr & 0x0F;        // PB0-PB3  -> LD5-LD8

    // reverse each nibble back into LED order (LD1 = MSB of its group)
    uint8_t cTopBits = ((cPins & 0x01) << 3) | ((cPins & 0x02) << 1) |
                       ((cPins & 0x04) >> 1) | ((cPins & 0x08) >> 3);
    uint8_t bBottomBits = ((bPins & 0x01) << 3) | ((bPins & 0x02) << 1) |
                          ((bPins & 0x04) >> 1) | ((bPins & 0x08) >> 3);

    ledState = (cTopBits << 4) | bBottomBits;
    /***************************************************************************
     * Your code goes in between this comment and the preceding one with
     * asterisks.
     **************************************************************************/
#endif /*  STM32F4 */
    return ledState;
}
