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

#include <string.h>

#define TASKSTACKSIZE     768

Task_Struct task0Struct, task1Struct, task2Struct;
Char task0Stack[TASKSTACKSIZE], task1Stack[TASKSTACKSIZE], task2Stack[TASKSTACKSIZE];

UART_Handle uart0, uart5;

#define MAX_BUFFER 256
char input[MAX_BUFFER];

sem_t mutex, empty0, empty5, full0, full5, uart0_queue, uart5_queue;

int last_put0 = 0, last_taken0 = 0, last_put5 = 0, last_taken5 = 0;

/*
 *  Console print function
 */
void console_print(char* text)
{
    int i = 0;
    while (i < strlen(text)) {
            UART_write(uart0, &text[i], 1);
            i++;
    }
}

/*
 *  Device send function
 */
void device_send(char* text)
{
    int i = 0;
    while (i < strlen(text)) {
        UART_write(uart5, &text[i], 1);
        i++;
    }
}

/*
 *  Read function
 */
int first = 1;
Void readUART0Fxn(UArg arg0, UArg arg1)
{
    char* delete = "\033[2J\033[1;1H";
    char* command_list = "Command List:\r\n "
            "1. Set mode \r\n "
            "2. Connect to WiFi \r\n "
            "3. Choose port for communication \r\n "
            "4. Enter passthrough mode \r\n "
            "5. Restore Factory Default Settings\r\n";
    console_print(delete);
    console_print(command_list);

    while (1) {
        sem_wait(&empty0);

        UART_read(uart0, &input[last_put0], 1);

        last_put0 = (last_put0 + 1) % MAX_BUFFER;

        sem_post(&full0);
        if(input[(last_put0 + MAX_BUFFER - 1) % MAX_BUFFER] == '\x0d' && !first) {
            switch (input[(last_put0 + MAX_BUFFER - 2) % MAX_BUFFER]) {
                case '1':
                    device_send("AT+CWMODE=3\r\n");
                    break;
                case '2':
                    device_send("AT+CWLAP\r\n");
                    break;
                default:
                    console_print("Invalid option \n\r");
                    console_print(command_list);
                    break;
            }
        }
        first = 0;
    }
}

/*
 *  Write function
 */
Void writeUART0Fxn(UArg arg0, UArg arg1)
{
    while (1) {
        sem_wait(&full0);

        UART_write(uart0, &input[last_taken0], 1);

        last_taken0 = (last_taken0 + 1) % MAX_BUFFER;

        sem_post(&empty0);
    }
}

/*
 *  Read UART5 function
 */
char input_char;
Void readUART5Fxn(UArg arg0, UArg arg1)
{
    while (1) {
        UART_read(uart5, &input_char, 1);

        UART_write(uart0, &input_char, 1);
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
    Task_Params taskParamsRead0, taskParamsWrite, taskParamsRead5;

    sem_init(&empty0,0,MAX_BUFFER);
    sem_init(&full0,0,0);
    sem_init(&empty5,0,MAX_BUFFER);
    sem_init(&full5,0,0);

    Task_Params_init(&taskParamsRead0);
    taskParamsRead0.stackSize = TASKSTACKSIZE;
    taskParamsRead0.stack = &task1Stack;
    taskParamsRead0.instance->name = "read";
    Task_construct(&task1Struct, (Task_FuncPtr)readUART0Fxn, &taskParamsRead0, NULL);

    Task_Params_init(&taskParamsWrite);
    taskParamsWrite.stackSize = TASKSTACKSIZE;
    taskParamsWrite.stack = &task2Stack;
    taskParamsWrite.instance->name = "write";
    Task_construct(&task2Struct, (Task_FuncPtr)writeUART0Fxn, &taskParamsWrite, NULL);

    Task_Params_init(&taskParamsRead5);
    taskParamsRead5.stackSize = TASKSTACKSIZE;
    taskParamsRead5.stack = &task0Stack;
    taskParamsRead5.instance->name = "read5";
    Task_construct(&task0Struct, (Task_FuncPtr)readUART5Fxn, &taskParamsRead5, NULL);

    /* Turn on user LED */
    GPIO_write(Board_LED0, Board_LED_ON);
    /* Chip select */
    GPIO_write(Board_E1, Board_LED_ON);

    /* UART intialization */
    UART_Params uart0Params;
    UART_Params uart5Params;

    /* Create a UART with data processing off. */
    UART_Params_init(&uart0Params);
    uart0Params.writeDataMode = UART_DATA_BINARY;
    uart0Params.readDataMode = UART_DATA_BINARY;
    uart0Params.readReturnMode = UART_RETURN_FULL;
    uart0Params.readEcho = UART_ECHO_OFF;
    uart0Params.baudRate = 9600;
    uart0 = UART_open(Board_UART0, &uart0Params);

    if (uart0 == NULL) {
        System_abort("Error opening the UART");
    }

    UART_Params_init(&uart5Params);
    uart5Params.writeDataMode = UART_DATA_BINARY;
    uart5Params.readDataMode = UART_DATA_BINARY;
    uart5Params.readReturnMode = UART_RETURN_FULL;
    uart5Params.readEcho = UART_ECHO_OFF;
    uart5Params.baudRate = 9600;
    uart5 = UART_open(Board_UART5, &uart5Params);

    if (uart5 == NULL) {
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
