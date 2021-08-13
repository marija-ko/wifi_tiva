/*
 * Copyright (c) 2015, Texas Instruments Incorporated
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
 *  ======== uartecho.c ========
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>

/* Example/Board Header files */
#include "Board.h"

#include <stdint.h>

#include <semaphore.h>

#define TASKSTACKSIZE     768

Task_Struct task0Struct, task1Struct, task2Struct;
Char task0Stack[TASKSTACKSIZE], task1Stack[TASKSTACKSIZE], task2Stack[TASKSTACKSIZE];

UART_Handle uart;

#define MAX_BUFFER 256
char input[MAX_BUFFER];

sem_t mutex, empty, full;

int last_put = 0, last_taken = 0;

/*
 *  Read function
 */
Void readFxn(UArg arg0, UArg arg1)
{
    while (1) {
        sem_wait(&empty);

        UART_read(uart, &input[last_put], 1);

        last_put = (last_put + 1) % MAX_BUFFER;

        sem_post(&full);
    }
}

/*
 *  Write function
 */
Void writeFxn(UArg arg0, UArg arg1)
{
    while (1) {
        sem_wait(&full);

        UART_write(uart, &input[last_taken], 1);

        last_taken = (last_taken + 1) % MAX_BUFFER;

        sem_post(&empty);
    }
}

/*
 *  ======== main ========
 */
int main(void)
{
    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();
    Board_initUART();

    /* Construct BIOS objects */
    Task_Params taskParamsRead, taskParamsWrite;

    sem_init(&empty,0,MAX_BUFFER);
    sem_init(&full,0,0);

    Task_Params_init(&taskParamsRead);
    taskParamsRead.stackSize = TASKSTACKSIZE;
    taskParamsRead.stack = &task1Stack;
    taskParamsRead.instance->name = "read";
    Task_construct(&task1Struct, (Task_FuncPtr)readFxn, &taskParamsRead, NULL);

    Task_Params_init(&taskParamsWrite);
    taskParamsWrite.stackSize = TASKSTACKSIZE;
    taskParamsWrite.stack = &task2Stack;
    taskParamsWrite.instance->name = "write";
    Task_construct(&task2Struct, (Task_FuncPtr)writeFxn, &taskParamsWrite, NULL);

    /* Turn on user LED */
    GPIO_write(Board_LED0, Board_LED_ON);

    /* UART intialization */
    UART_Params uartParams;

    /* Create a UART with data processing off. */
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.baudRate = 9600;
    uart = UART_open(Board_UART0, &uartParams);

    if (uart == NULL) {
        System_abort("Error opening the UART");
    }


    /* This example has logging and many other debug capabilities enabled */
    System_printf("This example does not attempt to minimize code or data "
                  "footprint\n");
    System_flush();

    System_printf("Starting the UART Echo example\nSystem provider is set to "
                  "SysMin. Halt the target to view any SysMin contents in "
                  "ROV.\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
