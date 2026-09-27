#include "Oled.h"
#include "OledDriver.h"
#include "Field.h"
#include "FieldOled.h"
#include "Ascii.h"

#define FIELD_SYMBOL_WIDTH 3
#define FIELD_SYMBOL_HEIGHT 4
const uint8_t gridSymbols[10][FIELD_SYMBOL_WIDTH] = {
    [FIELD_SQUARE_EMPTY] =
    {
        0b0000,
        0b0000,
        0b0000,
    },
    [FIELD_SQUARE_SMALL_BOAT] =
    {
        0b1001,
        0b1011,
        0b1111,
    },
    [FIELD_SQUARE_MEDIUM_BOAT] =
    { // 
        0b0111,
        0b0100,
        0b1111,
    },
    [FIELD_SQUARE_LARGE_BOAT] =
    { // 
        0b1011,
        0b1011,
        0b1101,
    },
    [FIELD_SQUARE_HUGE_BOAT] =
    { // 
        0b1111,
        0b1101,
        0b1101,
    },
    [FIELD_SQUARE_UNKNOWN] =
    { // 
        0b1111,
        0b1111,
        0b1111,
    },
    [FIELD_SQUARE_HIT] =
    { // 
        0b1001,
        0b0110,
        0b1001,
    },
    [FIELD_SQUARE_MISS] =
    { // 
        0b0000,
        0b0110,
        0b0000,
    },

    [FIELD_SQUARE_CURSOR] =
    { // 
        0b1111,
        0b1001,
        0b1111,
    },

    [FIELD_SQUARE_INVALID] =
    { // 
        0b1111,
        0b1111,
        0b1111,
    }
};

uint8_t _FieldOledDrawSymbol(int x, int y, SquareStatus s);
void _FieldOledDrawField(const Field *f, int xOffset);

void FieldOledDrawScreen(
        const Field *myField,
        const Field *theirField,
        FieldOledTurn playerTurn,
        uint8_t turn_number
        ) {
#ifndef __NUCLEO_SIMULATOR
    OLED_Clear(OLED_COLOR_BLACK);
    _FieldOledDrawField(myField, 0);
    if (theirField)
    {
        _FieldOledDrawField(theirField, 76);
    }
    else
    {
        OLED_Update();
        return;
    }

    // Draw inner artwork.
    OLED_DrawChar(53, 1, 'P');
    OLED_DrawChar(76 - ASCII_FONT_WIDTH - 1, 1, 'O');
    if (playerTurn == FIELD_OLED_TURN_MINE) {
        OLED_DrawChar(53, ASCII_FONT_HEIGHT + 1, '<');
    } else if (playerTurn == FIELD_OLED_TURN_THEIRS) {
        OLED_DrawChar(76 - ASCII_FONT_WIDTH - 1, ASCII_FONT_HEIGHT + 1, '>');
    }

    // Draw turn number:
    int x;
    x = 76 - ASCII_FONT_WIDTH * 2;
    OLED_DrawChar(x, ASCII_FONT_HEIGHT * 3, turn_number % 10 + '0');
    x -= ASCII_FONT_WIDTH;
    turn_number /= 10;
    OLED_DrawChar(x, ASCII_FONT_HEIGHT * 3, turn_number % 10 + '0');

    OLED_Update();
#endif
}

/**
 * Draw the given player's grid at the given x-coordinate.
 */
void _FieldOledDrawField(const Field *f, int xOffset)
{
    int i;
    int finalCol = 10 * 5 + 2;

    int finalRowOffset = (
            OLED_DRIVER_PIXEL_ROWS / OLED_DRIVER_BUFFER_LINE_HEIGHT - 1
            ) * OLED_DRIVER_PIXEL_COLUMNS;

    // Draw the horizontal grid borders.
    for (i = 0; i < finalCol; ++i) {
        rgbOledBmp[xOffset + i] |= 1;
        rgbOledBmp[finalRowOffset + xOffset + i] |= 0x80;
    }
    // Draw the vertical grid borders.
    for (i = 0; i < OLED_DRIVER_PIXEL_ROWS / OLED_DRIVER_BUFFER_LINE_HEIGHT; ++i) {
        rgbOledBmp[i * OLED_DRIVER_PIXEL_COLUMNS + xOffset + 0] = 0xFF;
        rgbOledBmp[i * OLED_DRIVER_PIXEL_COLUMNS + xOffset + finalCol - 1] = 0xFF;
    }

    // Draw each item in the grid.
    int yOffset = 2;
    xOffset += 1;
    for (i = 0; i < FIELD_COLS; ++i) {
        int j;
        for (j = 0; j < FIELD_ROWS; ++j) {
            _FieldOledDrawSymbol(xOffset + 1 + 5 * i, yOffset + 5 * j, f->grid[j][i]);
        }
    }
}

/**
 * Draw the desired symbol at the given x/y coordinates.
 */
uint8_t _FieldOledDrawSymbol(int x, int y, SquareStatus s)
{
    if (x < OLED_DRIVER_PIXEL_COLUMNS - FIELD_SYMBOL_WIDTH && y < OLED_DRIVER_PIXEL_ROWS - FIELD_SYMBOL_HEIGHT) {
        // Now first determine the columns and rows of the OLED bits that need to be modified
        int rowMin, rowMax, colMin, colMax;
        rowMin = y / OLED_DRIVER_BUFFER_LINE_HEIGHT;
        int rowY = y % OLED_DRIVER_BUFFER_LINE_HEIGHT;
        rowMax = (y + FIELD_SYMBOL_HEIGHT) / OLED_DRIVER_BUFFER_LINE_HEIGHT;
        colMin = x;
        colMax = x + FIELD_SYMBOL_WIDTH;
        {
            // Generate a positive mask for where in the column the new symbol will be drawn.
            int colMask = ((1 << FIELD_SYMBOL_HEIGHT) - 1) << rowY;
            int j;
            for (j = 0; j < colMax - colMin; ++j) {
                int oledCol = colMin + j;
                uint8_t newCharCol = rgbOledBmp[rowMin * OLED_DRIVER_PIXEL_COLUMNS + oledCol] & ~colMask;
                // Make sure we always grab from the top part of the character.
                newCharCol |= (gridSymbols[(int) s][j] & (colMask >> rowY)) << rowY;
                rgbOledBmp[rowMin * OLED_DRIVER_PIXEL_COLUMNS + oledCol] = newCharCol;
            }
        }
        if (rowMax > rowMin) {
            // Generate a positive mask for where in the column the new symbol will be drawn.
            // Since we need the lower portion of the symbol, we recalculate its height.
            int colMask = ((1 << FIELD_SYMBOL_HEIGHT) - 1) >> (OLED_DRIVER_BUFFER_LINE_HEIGHT - rowY);
            int j;
            for (j = 0; j < colMax - colMin; ++j) {
                int oledCol = colMin + j;
                uint8_t newCharCol = rgbOledBmp[rowMax * OLED_DRIVER_PIXEL_COLUMNS + oledCol] & ~colMask;
                // Make sure we grab the proper part of the character from the font.
                newCharCol |= (gridSymbols[(int) s][j] & (colMask << (OLED_DRIVER_BUFFER_LINE_HEIGHT - rowY))) >>
                    (OLED_DRIVER_BUFFER_LINE_HEIGHT - rowY);
                rgbOledBmp[rowMax * OLED_DRIVER_PIXEL_COLUMNS + oledCol] = newCharCol;
            }
        }
    }

    return FALSE;
}
