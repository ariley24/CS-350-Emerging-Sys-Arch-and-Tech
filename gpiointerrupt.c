/*
 * Copyright (c) 2015-2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== gpiointerrupt.c ========
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/Timer.h>

/* Driver configuration */
#include "ti_drivers_config.h"

/* Initialize buttons for callback */
int leftButton = 0;
int rightButton = 0;

/*
 *  ======== gpioButtonFxn0 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_0.
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */
void gpioButtonFxn0(uint_least8_t index)
{
    leftButton = 1;
}

/*
 *  ======== gpioButtonFxn1 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_1.
 *  This may not be used for all boards.
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */
void gpioButtonFxn1(uint_least8_t index)
{
    rightButton = 1;
}

/*
 * ======== UART Code ========
 */
#define DISPLAY(x) UART_write(uart, &output, x);

// UART Global Variables
char output[64];
int bytesToSend;

// Driver Handles - Global Variables
UART_Handle uart;

void initUART(void) {
    UART_Params uartParams;

    // Init the driver
    UART_init();

    // Configure the driver
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.baudRate = 115200;

    // Open the driver
    uart = UART_open(CONFIG_UART_0, &uartParams);

    if (uart == NULL) {
        /* UART_open() failed */
        while(1);
    }
}

/*
 *  ======== I2C Code ========
 */
// I2C Global Variables
static const struct {
    uint8_t address;
    uint8_t resultReg;
    char *id;
}
sensors[3] = {
              { 0x48, 0x0000, "11x" },
              { 0x49, 0x0000, "116" },
              { 0x41, 0x0001, "006" }
};
uint8_t txBuffer[1];
uint8_t rxBuffer[2];
I2C_Transaction i2cTransaction;

// Driver Handles - Global Variables
I2C_Handle i2c;

// Make sure you call initUART() before calling this function
void initI2C(void) {
    int8_t i, found;
    I2C_Params i2cParams;

    DISPLAY(snprintf(output, 64, "Initializing I2C Driver - "));

    // Init the driver
    I2C_init();

    // Configure the driver
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // Open the driver
    i2c = I2C_open(CONFIG_I2C_0, &i2cParams);
    if (i2c == NULL) {
        DISPLAY(snprintf(output, 64, "Failed\n\r"));
        while(1);
    }

    DISPLAY(snprintf(output, 32, "Passed\n\r"));

    // Boards were shipped with different sensors.
    // Welcome to the world of embedded systems.
    // Try to determine which sensor we have.
    // Scan through the possible sensor addresses.

    /* Common I2C transaction setup */
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 0;

    found = false;
    for (i=0; i<3; ++i) {
        i2cTransaction.slaveAddress = sensors[i].address;
        txBuffer[0] = sensors[i].resultReg;
        DISPLAY(snprintf(output, 64, "Is this %s? ", sensors[i].id));

        if (I2C_transfer(i2c, &i2cTransaction)) {
            DISPLAY(snprintf(output, 64, "Found\n\r"));
            found = true;
            break;
        }
        DISPLAY(snprintf(output, 64, "No\n\r"));
    }

    if (found) {
        DISPLAY(snprintf(output, 64, "Detected TMP%s I2C address: %x\n\r", sensors[i].id, i2cTransaction.slaveAddress));
    }
    else {
        DISPLAY(snprintf(output, 64, "Temperature sensor not found, contact professor\n\r"));
    }
}

int16_t readTemp(void) {
    int16_t temperature = 0;
    i2cTransaction.readCount = 2;
    if (I2C_transfer(i2c, &i2cTransaction)) {
        /*
         * Extract degrees C from the received data;
         * see TMP sensor datasheet
         */
        temperature = (rxBuffer[0] << 8) | (rxBuffer[1]);
        temperature *= 0.0078125;

        /*
         * If the MSB is set '1', then we have a 2's complement
         * negative value which needs to be sign extended
         */
        if (rxBuffer[0] & 0x80) {
            temperature |= 0xF000;
        }
    }
    else {
        DISPLAY(snprintf(output, 64, "Error reading temperature sensor (%d)\n\r", i2cTransaction.status));
        DISPLAY(snprintf(output, 64, "Please power cycle your board by unplugging USB and plugging back in.\n\r"));
    }
    return temperature;
}

/*
 * ======== Timer Code ========
 */
// Driver Handles - Global Variables
Timer_Handle timer0;

volatile unsigned char TimerFlag = 0;
void timerCallback(Timer_Handle myHandle, int_fast16_t status) {
    TimerFlag = 1;
}

void initTimer(void) {
    Timer_Params params;

    // Init the driver
    Timer_init();

    // Configure the driver
    Timer_Params_init(&params);
    params.period = 100000;
    params.periodUnits = Timer_PERIOD_US;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;

    // Open the driver
    timer0 = Timer_open(CONFIG_TIMER_0, &params);

    if (timer0 == NULL) {
        /* Failed to initialized timer */
        while (1) {
        }
    }
    if (Timer_start(timer0) == Timer_STATUS_ERROR) {
        /* Failed to start timer*/
        while (1) {
        }
    }
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED and button pins */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* Turn on user LED */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    /* Install Button callback */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn0);

    /* Enable interrupts */
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);

    /*
     *  If more than one input pin is available for your device, interrupts
     *  will be enabled on CONFIG_GPIO_BUTTON1.
     */
    if (CONFIG_GPIO_BUTTON_0 != CONFIG_GPIO_BUTTON_1) {
        /* Configure BUTTON1 pin */
        GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

        /* Install Button callback */
        GPIO_setCallback(CONFIG_GPIO_BUTTON_1, gpioButtonFxn1);
        GPIO_enableInt(CONFIG_GPIO_BUTTON_1);
    }

    /*
     * ======== Main Code ========
     *
     * Builds functionality that checks buttons, reads temperature,
     * updates the LED, and reports to the server.
     * A button press increases or decreases the temp set-point by
     * 1 degree every 200ms. If the temp is greater than the set-point,
     * the LED should turn off. If the temp is less that the set-point,
     * the LED should turn on.
     *
     */

    /* Driver init functions */
    initUART();
    initI2C();
    initTimer();

    /* Variables */
    unsigned long buttonTime = 0;              // Init elapsed time for button check
    unsigned long displayTime = 0;             // Init elapsed time for display check
    unsigned long tempTime = 0;                // Init elapsed time for temp check
    const unsigned long buttonPeriod = 200;    // button check period (200ms)
    const unsigned long displayPeriod = 1000;  // display check period (1000ms)
    const unsigned long tempPeriod = 500;      // temp check period (500ms)
    const unsigned long timerPeriod = 100;     // timer period (100ms)
    int heat = 0;                              // Init heat setting ('0' = off, '1' = on)
    int seconds = 0;                           // Init seconds since last board reset
    int temperature = 0;                       // Init temp
    int setpoint = 30;                         // Init set-point

    /*
     * ======== Main Code While Loop ========
    */
    while (1) {
        readTemp();
        // Check button presses every 200ms
        if (buttonTime >= buttonPeriod) {
            // Check for right button press
            if (rightButton == 1) {
                // right button press increments setpoint
                setpoint += 1;
                // reset button
                rightButton = 0;
            }
            // check for left button press
            if (leftButton == 1) {
                // left button press decrements setpoint
                setpoint -= 1;
                // rest button
                leftButton = 0;
            }
        }

        // Check temp and update LED(heat) every 500ms
        if (tempTime >= tempPeriod) {
            temperature = readTemp();
            if (temperature < setpoint) {
                // Temp is lower than setpoint, LED indicates heat is ON
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
                heat = 1;
            }
            else {
                // Temp is equal to or higher than setpoint, LED indicates heat is OFF
                GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
                heat = 0;
            }
        }

        // UART output to display every 1000ms (1 second)
        if (displayTime >= displayPeriod) {
            // Display information on terminal
            DISPLAY(snprintf(output, 64, "<%02d,%02d,%d,%04d\n\r", temperature, setpoint, heat, seconds));
            // Increment seconds every 1000ms
            ++ seconds;
        }
        while (!TimerFlag){}         // Wait for timer period
        TimerFlag = 0;               // Lower flag raised by timer
        buttonTime += timerPeriod;   // Increment buttonTime by 100ms
        tempTime += timerPeriod;     // Increment tempTime by 100ms
        displayTime += timerPeriod;  // Increment displayTime by 100ms
    }
}
