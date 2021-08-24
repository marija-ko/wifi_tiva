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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <semaphore.h>
#include <string.h>

#define TASKSTACKSIZE     768

Task_Struct task0Struct, task1Struct, task2Struct;
Char task0Stack[TASKSTACKSIZE], task1Stack[TASKSTACKSIZE], task2Stack[TASKSTACKSIZE];

UART_Handle uart0, uart5;

#define MAX_BUFFER 256
char input[MAX_BUFFER];

sem_t mutex, empty0, full0, command_finished;

int last_put0 = 0, last_taken0 = 0, last_put5 = 0, last_taken5 = 0;

/*
 *  reverse:  reverse string s in place
 */
void reverse(char s[])
{
    int i, j;
    char c;

    for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/*
 *  itoa:  convert n to characters in s
 */
void itoa(int n, char s[])
{
    int i, sign;

    if ((sign = n) < 0)
        n = -n;
    i = 0;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    reverse(s);
}

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
 * Insert into list with SSID's
 */
char ssid_list[32][33];
void put(char* name, int index)
{
    int i;
    for(i = 0; i < strlen(name) && i < 32; i++) {
        ssid_list[index][i] = name[i];
    }
    ssid_list[index][i] = '\0';
}

/*
 *   Get a particular SSID from the list with SSID's
 */
char* get(int index)
{
    return ssid_list[index];
}

/*
 *  Processing response from UART5
 */
char command[MAX_BUFFER] = "";
int without_echo = 0;
int passthrough_mode = 0;

char ssid_entry[32][128];
int listing_networks = 0;
int num_ssid = 0;
int command_size = 0;
int ssid_size = 0;

int send_attempted;
void process_response(char inputc)
{
    if(listing_networks == 0 && without_echo == 0) {
        command[command_size++] = inputc;
        UART_write(uart0, &inputc, 1);
    }

    if (strstr(command,"AT+CWLAP\x0d") && listing_networks == 0) {
        listing_networks = 1;
        without_echo = 1;
        num_ssid = 0;
    } else if (listing_networks == 1) {
        if (inputc =='\x0d') {
            if (strstr(ssid_entry[num_ssid],"OK") || strstr(ssid_entry[num_ssid],"ERROR")) {
                listing_networks = 0;
                without_echo = 0;
                command_size = 0;

                memset(command, 0, strlen(command));
                sem_post(&command_finished);
            } else {
                ssid_size = 0;
                num_ssid++;
            }
        } else if (inputc =='\x0a') {
        } else {
            ssid_entry[num_ssid][ssid_size++] = inputc;
        }
    }

    if (strstr(command,"AT+CWJAP=")){
        without_echo = 1;
    }

    if (strstr(command,"AT+CIPSEND=")){
        send_attempted = 1;
    }

    if(strstr(command, "CLOSED")) {
        passthrough_mode = 0;
    }

    if(inputc =='\x0d') {
        if (strstr(command, "OK") || strstr(command, "ERROR") || strstr(command, "FAIL") && passthrough_mode == 0) {
           sem_post(&command_finished);
        }

        if ((strstr(command, "ERROR") || strstr(command, "FAIL")) && send_attempted) {
            passthrough_mode = 0;
            send_attempted = 0;
            sem_post(&command_finished);
        } else if (strstr(command, "OK") && send_attempted) {
            send_attempted = 0;
            sem_post(&command_finished);
        }

        without_echo = 0;
        command_size = 0;
        memset(command, 0, strlen(command));
    }
}

/*
 *  Print available networks
 */
void list_networks() {

    char listed_number[4];

    int i;

    char* token;
    char* ssid;

    for (i = 0; i < num_ssid-1; i++) {

        char delimiter_comma[2] = ",";
        char delimiter_quote[2] = "\"";

        token = strtok(ssid_entry[i], delimiter_comma);
        token = strtok(NULL, delimiter_comma);
        ssid = strtok(token, delimiter_quote);
        itoa(i+1, listed_number);
        strcat(listed_number, ". ");

        console_print(listed_number);
        console_print(ssid);
        console_print("\r\n");

        put(ssid, i);
    }
}

/*
 *  Connect to network
 */
void choose_network()
{
    console_print("Type 0 to return \n\r");
    char choice_char[4] = "";
    char k;
    int i = 0;

    while (1) {
        UART_read(uart0, &k, 1);
        if(k == '\n' || k == '\r' || i>=3) break;
        UART_write(uart0, &k, 1);
        choice_char[i++] = k;
    }

    int choice = atoi(choice_char) - 1;
    if (choice < 0 || choice >= num_ssid) {
        console_print("\r\n");
        sem_post(&command_finished);
        return;
    }

    console_print(ssid_list[choice]);
    console_print("\r\n");
    console_print("Enter password\r\n");

    char password[64];
    i = 0;


    while (1) {
        UART_read(uart0, &k, 1);
        if(k == '\n' || k == '\r' || i >= 63) break;
        password[i++] = k;
        char star = '*';
        UART_write(uart0, &star, 1);
    }
    password[i++] = '\0';

    console_print("\r\n");

    char text[128];
    snprintf(text, 128, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid_list[choice], password);

    device_send(text);
}

/*
 *  Make TCP connection
 */
void choose_port()
{
    char port_number_char[5] = "";
    int i = 0;
    char k;

    console_print("First run server.py file. \n\r Type the number of port you'd like to use. \n\r");

    while (1) {
        UART_read(uart0, &k, 1);
        if(k == '\n' || k == '\r' || i >= 4) break;
        UART_write(uart0, &k, 1);
        port_number_char[i++] = k;
    }

    console_print("\r\n");

    int port_number = atoi(port_number_char);

    console_print("Enter IP address you'd like to message. \n\r");

    char ip_address[16] = "";
    i = 0;

    while (1) {
        UART_read(uart0, &k, 1);
        UART_write(uart0, &k, 1);
        if(k == '\n' || k == '\r' || i >= 15) break;
        ip_address[i++] = k;
    }
    ip_address[i++] = '\0';

    console_print("\r\n");

    char text[128];

    sprintf(text, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", ip_address, port_number_char);
    device_send(text);
}

/*
 *  Enter passthrough mode
 */
int val;
void enter_passthrough()
{
    passthrough_mode = 1;
    console_print("Entered passthrough mode. \r\nWrite your messages. \r\n ++pin to send LED0 pin value. \n\r +++ to exit. \n\r");

    while(passthrough_mode == 1) {
        char message [128] = "";
        int i = 0;
        char k;

        while (1) {
             if (passthrough_mode == 0) break;
             UART_read(uart0, &k, 1);
             if(k == '\n' || k == '\r' || i >= 128) break;
             UART_write(uart0, &k, 1);
             message[i++] = k;
        }
        message[i++] = '\0';
        console_print("\r\n");

        if (strcmp(message, "+++") == 0) {
            passthrough_mode = 0;
            break;
        }


        if (strcmp(message, "++pin") == 0) {
            val = GPIO_read(Board_LED0);

            memset(message, 0, strlen(message));
            itoa(val, message);
        }

        char text[128];
        char msg_len[4];

        itoa(strlen(message), msg_len);
        snprintf(text, 128, "AT+CIPSEND=%s\r\n", msg_len);

        device_send(text);

        SysCtlDelay(1000 * (SysCtlClockGet() / 3 / 1000));

        sem_wait(&command_finished);

        device_send(message);
    }
    console_print("Exited passthrough mode. \r \n");
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

        char last = input[(last_put0 - 1 + MAX_BUFFER) % MAX_BUFFER];
        char second_last = input[(last_put0 - 2 + MAX_BUFFER) % MAX_BUFFER];

        sem_post(&full0);

        if(last == '\x0d' && !first) {
            console_print("\n\r");
            switch (second_last) {
                case '1':
                    device_send("AT+CWMODE=3\r\n");
                    sem_wait(&command_finished);
                    break;
                case '2':
                    device_send("AT+CWLAP\r\n");
                    sem_wait(&command_finished);
                    list_networks();
                    choose_network();
                    sem_wait(&command_finished);
                    break;
                case '3':
                    choose_port();
                    sem_wait(&command_finished);
                    break;
                case '4':
                    enter_passthrough();
                    while(passthrough_mode == 1) {

                    }
                    break;
                case '5':
                    device_send("AT+RESTORE\r\n");
                    break;
                default:
                    console_print("Invalid option \n\r");
                    break;
            }
            console_print(command_list);
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
        process_response(input_char);
    }
}

/*
 *  Callback function for the GPIO interrupt on Board_BUTTON0.
 */
int count = 0;
void gpioButtonFxn0(unsigned int index)
{
    /* Clear the GPIO interrupt and toggle an LED */
    GPIO_toggle(Board_LED0);

    if (count++ == 100) {
        count = 0;
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

    /* install Button callback */
    GPIO_setCallback(Board_BUTTON0, gpioButtonFxn0);

    /* Enable interrupts */
    GPIO_enableInt(Board_BUTTON0);

    /* Construct BIOS objects */
    Task_Params taskParamsRead0, taskParamsWrite, taskParamsRead5;

    sem_init(&empty0,0,MAX_BUFFER);
    sem_init(&full0,0,0);
    sem_init(&command_finished,0,0);
    int i;
    for (i = 0; i < 32; i++) {
        memset(ssid_entry[i], 0, strlen(ssid_entry[i]));
    }

    Task_Params_init(&taskParamsRead0);
    taskParamsRead0.stackSize = TASKSTACKSIZE;
    taskParamsRead0.stack = &task1Stack;
    taskParamsRead0.instance->name = "read0";
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
    uart0Params.baudRate = 115200;
    uart0 = UART_open(Board_UART0, &uart0Params);

    if (uart0 == NULL) {
        System_abort("Error opening the UART");
    }

    UART_Params_init(&uart5Params);
    uart5Params.writeDataMode = UART_DATA_BINARY;
    uart5Params.readDataMode = UART_DATA_BINARY;
    uart5Params.readReturnMode = UART_RETURN_FULL;
    uart5Params.readEcho = UART_ECHO_OFF;
    uart5Params.baudRate = 115200;
    uart5 = UART_open(Board_UART5, &uart5Params);

    if (uart5 == NULL) {
        System_abort("Error opening the UART");
    }

    /* Start BIOS */
    BIOS_start();

    return (0);
}
