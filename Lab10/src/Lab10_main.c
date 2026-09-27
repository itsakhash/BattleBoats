/**
 * @file    Lab10_main.c
 *
 * Welcome to BattleBoats!
 *
 * @author  HARE Lab
 * @author  jLab
 *
 * @date    17 May 2025
 * @version 1.2.1
 */
// Standard libraries.
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

// Course libraries.
#include <BOARD.h>
#include <Buttons.h>
#include <Leds.h>
#include <Oled.h>
#include <Timers.h>
#include <Uart.h>

// Battleboats libraries.
#include "BattleBoats.h"
#include "Agent.h"
#include "Negotiation.h"
#include "Message.h"
#include "Field.h"


/*  PROTOTYPES  */
void setup(void);


/*  MODULE-LEVEL DEFINITIONS, MACROS    */

// The following Macro switches provide useful debugging tools:

// Trace Mode:  Print a trace of events as they are detected:
//#define TRACE_MODE

// Unseeded Mode:  Do not reseed rand, and seed with switches (useful for
// creating repeatable tests):
//#define UNSEEDED_MODE

// PC Mode: Lets you play BattleBoats against a PC user.
//#define PC_MODE

#ifndef TRACE_MODE
#define debug_printf(...)
#else
#define debug_printf printf
#endif

#ifdef UNSEEDED_MODE
#define seed_rand(x) 
#define AgentInit() {srand(SWITCH_STATES()); AgentInit();}
#else
#define seed_rand(x) srand(x)
#endif

// The amount of time between UART updates (in 100ths of a second).
#define TRANSMIT_PERIOD     1
#define UART_BUFFER_SIZE    (MESSAGE_MAX_LEN + 1)

/*  Static data for BattleBoats top level:  */

// This is the top-level event flag:
static BB_Event battleboatEvent;

// A freerunning timer is used to inject randomness using external events,
// and to throttle the outgoing transmission speed:
static uint32_t freeRunningTimer = 0;

/** transmissionState
 *
 * The Transmission Outgoing submodule has two states.  It can only send one
 * message at a time, so new outgoing messages can only be started when it is in
 * IDLE mode. 
 * 
 * An indexed buffer stores the message until it is completely sent.
 */
enum
{
    SENDING,
    IDLE
} transmissionState = IDLE;

static uint8_t outgoingMessageBuffer[MESSAGE_MAX_LEN + 1];
static uint8_t bufferMessageUART[UART_BUFFER_SIZE];
static uint8_t bufferMessageSerial[UART_BUFFER_SIZE];

volatile int txIndex = 0;
volatile int rxUARTIndex = 0;
volatile int rxSerialIndex = 0;

volatile int8_t messageReceivedUART = FALSE;
volatile int8_t messageReceivedSerial = FALSE;


/*  FUNCTIONS   */

/** Transmission_StartSendingMessage(*messageToSend)
 *
 * This function copies a message into the Transmission outgoing message buffer
 * and begins the sending process. Once this function is called, the
 * Transmission module switches into the SENDING state.
 * 
 * This function should not be called if it is already in the SENDING state!
 *
 * @param   *messageToSend
 */
void Transmission_StartSendingMessage(const Message *messageToSend)
{
    // This should only be called if sender is in IDLE.
    switch (transmissionState)
    {
        case SENDING:
            // The Agent state machine should not allow a new message to be
            // started while another message is in the buffer:
            OLED_Clear(OLED_COLOR_BLACK);
            OLED_DrawString("Fatal Transmission Error!");
            OLED_Update();
            break;
        case IDLE:
            // Copy message into sending buffer:
            Message_Encode((char *)outgoingMessageBuffer, *messageToSend);
            txIndex = 0;
            // Switch into sending mode:
            transmissionState = SENDING;
            break;
        default: break;
    }
}

/** Transmission_SendChar()
 *
 * If an outgoing message is in the buffer, this module sends one character each
 * time it is called. When the message is sent, the module switches to IDLE
 * mode.
 */
void Transmission_SendChar(void)
{
    // If in SENDING, send one char.  Otherwise, do nothing.
    if (transmissionState != SENDING) return;

    // First send our current char:
    uint8_t toSend = outgoingMessageBuffer[txIndex];
    if (toSend == '\0')
    {
        // This means our message is fully transmitted.
        battleboatEvent.type = BB_EVENT_MESSAGE_SENT;
        txIndex = 0;
        transmissionState = IDLE;
        return;
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)&toSend, 1, HAL_MAX_DELAY);
    // Send characters to UART2 through printf() to enable the Python game.
    printf("%c", toSend);
    txIndex++;
}

/** Transmission_ReceiveMessage()
 *
 * Check for incoming messages.  This module uses Message_Decode() to parse
 * messages in the UART input stream, and generates events if any messages are
 * detected.
 **/
void Transmission_ReceiveMessage(void)
{
    if (messageReceivedUART == TRUE)
    {
        for (int i = 0; i < strlen((char *)bufferMessageUART); i++)
        {
            Message_Decode(bufferMessageUART[i], &battleboatEvent);
        }
        messageReceivedUART = FALSE;
    }
    else if (messageReceivedSerial == TRUE)
    {
        for (int i = 0; i < strlen((char *)bufferMessageSerial); i++)
        {
            Message_Decode(bufferMessageSerial[i], &battleboatEvent);
        }
        messageReceivedSerial = FALSE;
    }

    // Also, re-seed our random number using the time:
    seed_rand(rand() + freeRunningTimer);
}

// Functions that stringify state names and event names for display.
#ifdef TRACE_MODE
#define printcase(x) case x: strcat(tracestr,#x); break;

void TraceState(void)
{
    char tracestr[100] = "---TRACE:  Current state=";
    switch (AgentGetState()) {
        printcase(AGENT_STATE_START);
        printcase(AGENT_STATE_CHALLENGING);
        printcase(AGENT_STATE_ACCEPTING);
        printcase(AGENT_STATE_ATTACKING);
        printcase(AGENT_STATE_DEFENDING);
        printcase(AGENT_STATE_WAITING_TO_SEND);
        printcase(AGENT_STATE_END_SCREEN);
        default: return;
    }
    strcat(tracestr, "\n");
    printf("%s", tracestr);
}

void TraceEvent(void)
{
    char tracestr[100] = "---TRACE:  Detected Event: ";
    switch (battleboatEvent.type) {
        printcase(BB_EVENT_NO_EVENT);
        printcase(BB_EVENT_START_BUTTON);
        printcase(BB_EVENT_RESET_BUTTON);
        printcase(BB_EVENT_SOUTH_BUTTON);
        printcase(BB_EVENT_EAST_BUTTON);
        printcase(BB_EVENT_CHA_RECEIVED);
        printcase(BB_EVENT_ACC_RECEIVED);
        printcase(BB_EVENT_REV_RECEIVED);
        printcase(BB_EVENT_SHO_RECEIVED);
        printcase(BB_EVENT_RES_RECEIVED);
        printcase(BB_EVENT_MESSAGE_SENT);
        printcase(BB_EVENT_ERROR);
    }
    sprintf(
            tracestr, "%s - %d,%d,%d\n",
            tracestr,
            battleboatEvent.param0,
            battleboatEvent.param1,
            battleboatEvent.param2
           );
    printf("%s", tracestr);
}

#else
#define TraceEvent() 
#define TraceState()
#endif

/** setup()
 *
 * BattleBoats initialization.
 */
void setup(void)
{
    BOARD_Init();

    // Only use UART1 if PC Mode disabled.
    if (!(UART1_Init(115200, bufferMessageUART) == SUCCESS))
    {
        printf("ERROR: Could not properly configure UART1; exiting.\r\n");
    }

    // Initialize ECE13 libraries:
    Buttons_Init();
    Timers_Init();
    OLED_Init();

    //Initialize Agent module:
    AgentInit();
}

/** main()
 *
 * Engage!
 */
int main()
{
    setup();

    //TraceState();

    // Main loop:
    while(1)
    {
        // If there is a top-level event, the Agent module should respond to it:
        if (battleboatEvent.type != BB_EVENT_NO_EVENT)
        {
            TraceEvent();

            Message messageToSend = AgentRun(battleboatEvent);

            TraceState();

            // Send a message, if there is one to send:
            if (messageToSend.type != MESSAGE_NONE)
            {
                Transmission_StartSendingMessage(&messageToSend);
            }
            // Consume the event:
            battleboatEvent.type = BB_EVENT_NO_EVENT;
        }
        // Update the LEDs to show the agent's current state:
        LEDs_Set(1 << AgentGetState()); HAL_Delay(1);
    }
}

/** HAL_TIM_PeriodElapsedCallback(*htim)
 *
 * This is the interrupt handler for the Timer2, Timer3, and Timer4 peripherals.
 * + Timer2 increments a counter used to track time.
 * + Timer3 polls for USER button inputs and processes messages.
 * + Timer4 polls for USER serial input (over USB).
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    //static uint32_t indexUART1 = 0;
    static uint32_t indexSTDIN = 0;
    if (htim == &htim2)
    {
        // Increment the timer.
        freeRunningTimer++;
    }
    else if (htim == &htim3)
    {
        // Check for any button events
        uint8_t buttonEvent = Buttons_CheckEvents();

        if (buttonEvent & BUTTON_EVENT_4DOWN)
        {
            battleboatEvent.type = BB_EVENT_START_BUTTON;
        }
        if (buttonEvent & BUTTON_EVENT_3DOWN)
        {
            battleboatEvent.type = BB_EVENT_EAST_BUTTON;
        }
        if (buttonEvent & BUTTON_EVENT_2DOWN)
        {
            battleboatEvent.type = BB_EVENT_SOUTH_BUTTON;
        }
        if (buttonEvent & BUTTON_EVENT_1DOWN)
        {
            battleboatEvent.type = BB_EVENT_RESET_BUTTON;
        }

        // Also, re-seed our random number using the time.
        if (buttonEvent) seed_rand(rand() + freeRunningTimer);

        // Every TRANSMIT_PERIOD cycles, attempt to run the transmission module.
        if (freeRunningTimer % TRANSMIT_PERIOD == 0)
        {
            Transmission_SendChar();
            if (battleboatEvent.type == BB_EVENT_MESSAGE_SENT) return;
            Transmission_ReceiveMessage();
        }
    }
    else if (htim == &htim4)
    {
        uint32_t indexDMA2 = UART_RX_CIRCULAR_BUFFER_SIZE -
            __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
        while(indexDMA2 != indexSTDIN)
        {
            bufferMessageSerial[rxSerialIndex] = fgetc(stdin);
            if (bufferMessageSerial[rxSerialIndex] == '\n')
            {
                messageReceivedSerial = TRUE;
                // Add a "stopping point" to the string.
                bufferMessageSerial[rxSerialIndex+1] = '\0';
                rxSerialIndex = 0;
            }
            else
            {
                rxSerialIndex++;
            }
            indexSTDIN = (indexSTDIN + 1) % UART_RX_CIRCULAR_BUFFER_SIZE;
            indexDMA2 = UART_RX_CIRCULAR_BUFFER_SIZE -
                __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
        }
    }
}


/** HAL_UART_RxCpltCallback(*huart)
 *
 * UART callback functions.
 *
 * @param   *huart
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // NOTE: This printf() is necessary.
#pragma GCC diagnostic ignored "-Wformat-zero-length"
        printf("");
#pragma GCC diagnostic warning "-Wformat-zero-length"
        if (bufferMessageUART[rxUARTIndex] == '\n')
        {
            messageReceivedUART = TRUE;
            // Add a "stopping point" to the string.
            bufferMessageUART[rxUARTIndex+1] = '\0';
            rxUARTIndex = 0;
        }
        else
        {
            rxUARTIndex++;
        }
        HAL_UART_Receive_IT(&huart1, &bufferMessageUART[rxUARTIndex], 1);
    }
}

// Provide weak implementations for students that didn't implement every
// function.
__attribute__((weak)) GuessData FieldAIDecideGuess(const Field *opp_field)
{
    GuessData ret = {0,0,RESULT_MISS};
    return ret;
}

