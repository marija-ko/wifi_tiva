//*****************************************************************************
//
// uart_echo.c - Example demonstrating UART module in internal loopback mode.
//
// Copyright (c) 2015-2017 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//   Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the
//   distribution.
//
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision 2.1.4.178 of the Tiva Firmware Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_uart.h"
#include "inc/hw_types.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "drivers/buttons.h"

//*****************************************************************************
//
//! \addtogroup uart_examples_list
//! <h1>UART Loopback (uart_loopback)</h1>
//!
//! This example demonstrates the use of a UART port in loopback mode.  On
//! being enabled in loopback mode, the transmit line of the UART is internally
//! connected to its own receive line.  Hence, the UART port receives back the
//! entire data it transmitted.
//!
//! This example echoes data sent to the UART's transmit FIFO back to the same
//! UART's receive FIFO.  To achieve this, the UART is configured in loopback
//! mode.  In the loopback mode, the Tx line of the UART is directly connected
//! to its Rx line internally and all the data placed in the transmit buffer is
//! internally transmitted to the Receive buffer.
//!
//! This example uses the following peripherals and I/O signals.  You must
//! review these and change as needed for your own board.
//! - UART7 peripheral - For internal Loopback
//! - UART0 peripheral - As console to display debug messages.
//!     - UART0RX - PA0
//!     - UART0TX - PA1
//!
//! UART parameters for the UART0 and UART7 port:
//! - Baud rate - 115,200
//! - 8-N-1 operation
//
//*****************************************************************************

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

char ssid_list[32][32];
//*****************************************************************************
//
// Insert into list with SSID's
//
//*****************************************************************************
void put(char* name, int index)
{
    int i;
    for(i = 0; i < 32; i++) {
        ssid_list[index][i] = name[i];
    }
}

//*****************************************************************************
//
// Get a particular SSID from the list with SSID's
//
//*****************************************************************************
char* get(int index)
{
    return ssid_list[index];
}
//*****************************************************************************
//
// The UART5 interrupt handler.
//
//*****************************************************************************
char command [256] = "";
int without_echo = 0;
int passthrough_mode = 0;

char ssid_entry[32][128];
int listing_networks = 0;
int num_ssid = 0;
int command_size = 0;
int ssid_size = 0;
int command_finished = 0;
void
UART5IntHandler(void)
{
    uint32_t ui32Status;

    //
    // Get the interrupt status.
    //
    ui32Status = UARTIntStatus(UART5_BASE, true);

    //
    // Clear the asserted interrupts.
    //
    UARTIntClear(UART5_BASE, ui32Status);

    while(UARTCharsAvail(UART5_BASE))
    {
        char k = UARTCharGetNonBlocking(UART5_BASE);

        if(listing_networks == 0 && without_echo == 0) {
            command[command_size++] = k;
            UARTCharPutNonBlocking(UART0_BASE, k);
        }

        if (strstr(command,"AT+CWLAP\x0d") && listing_networks == 0) {
            listing_networks = 1;
            without_echo = 1;
            num_ssid = 0;
            UARTCharPutNonBlocking(UART0_BASE, '\r');
            UARTCharPutNonBlocking(UART0_BASE, '\n');
        } else if (listing_networks == 1) {
            if(k =='\x0d') {
                if (strstr(ssid_entry[num_ssid],"OK")) {
                    listing_networks = 0;
                    without_echo = 0;
                    command_size = 0;
                    memset(command, 0, strlen(command));
                    command_finished = 1;
                } else {
                    ssid_size = 0;
                    num_ssid++;
                }
            } else if(k =='\x0a') {
                continue;
            } else {
                ssid_entry[num_ssid][ssid_size++] = k;
            }
        }

        if (strstr(command,"AT+CWJAP=")){
            without_echo = 1;
        }

        if(k =='\x0d') {
            if (strstr(command, "OK") || strstr(command, "ERROR") && passthrough_mode == 0) {
               command_finished = 1;
            }
            without_echo = 0;
            command_size = 0;
            memset(command, 0, strlen(command));
        }
    }
}

//*****************************************************************************
//
// The Button0 interrupt handler.
//
//*****************************************************************************
void
Button0IntHandler(void)
{
    uint32_t status = 0;

    status = GPIOIntStatus(BUTTONS_GPIO_BASE, true);
    GPIOIntClear(BUTTONS_GPIO_BASE, status);

    if (status & GPIO_INT_PIN_4){
        uint8_t  value = GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_3);

        if (value == 0)
          GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);
        else
          GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0);
    }
}

//*****************************************************************************
//
// Send a string to the UART.  This function sends a string of characters to a
// particular UART module.
//
//*****************************************************************************
void
UARTSend(uint32_t ui32UARTBase, const uint8_t *pui8Buffer, uint32_t ui32Count)
{
    //
    // Loop while there are more characters to send.
    //
    while(ui32Count--)
    {
        //
        // Write the next character to the UART.
        //
        UARTCharPut(ui32UARTBase, *pui8Buffer++);
    }
}

//*****************************************************************************
//
// Configue UART in internal loopback mode and tranmsit and receive data
// internally.
//
//*****************************************************************************
int
main(void)
{
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                       SYSCTL_XTAL_16MHZ);

    //
    // Enable the peripherals used by this example.
    // UART0 :  To dump information to the console about the example.
    // UART7 :  Enabled in loopback mode. Anything transmitted to Tx will be
    //          received at the Rx.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART5);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    //
    // Enable processor interrupts.
    //
    IntMasterEnable();

    //
    // Set GPIO A0 and A1 as UART pins.
    //
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    GPIOPinConfigure(GPIO_PE4_U5RX);
    GPIOPinConfigure(GPIO_PE5_U5TX);
    GPIOPinTypeUART(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_1);

    IntEnable(INT_UART5);
    UARTIntEnable(UART5_BASE, UART_INT_RX | UART_INT_RT);

    //
    // Configure the UART for 115,200, 8-N-1 operation.
    //

    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));
    UARTConfigSetExpClk(UART5_BASE, SysCtlClockGet(), 115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_1, GPIO_PIN_1);

    UARTSend(UART0_BASE, (uint8_t *)"\033[2J\033[1;1H", 10);

    //
    // Loop forever echoing data through the UART.
    //
    while(1)
    {
        UARTSend(UART0_BASE, (uint8_t *)"Command List:\r\n 1. Set mode \r\n 2. Connect to WiFi \r\n 3. Choose port for communication \r\n 4. Enter passthrough mode \r\n 5. Restore Factory Default Settings\r\n",
                        strlen("Command List:\r\n 1. Set mode \r\n 2. Connect to WiFi \r\n 3. Choose port for communication \r\n 4. Enter passthrough mode \r\n 5. Restore Factory Default Settings\r\n"));

        uint8_t choice = UARTCharGet(UART0_BASE);

        UARTCharPut(UART0_BASE, choice);
        UARTSend(UART0_BASE, "\r\n", strlen("\r\n"));

        switch(choice)
        {
        case '1':
            UARTSend(UART5_BASE, (uint8_t *)"AT+CWMODE=3\r\n", strlen("AT+CWMODE=3\r\n"));

            while(command_finished == 0) {
            }

            command_finished = 0;
            break;
        case '2':
            UARTSend(UART5_BASE, (uint8_t *)"AT+CWLAP\r\n", strlen("AT+CWLAP\r\n"));

            while(command_finished == 0) {
            }

            command_finished = 0;
            char listed_number[4];
            char delimiter_comma[2] = ",";
            char delimiter_quote[2] = "\"";
            int i;

            for(i = 0; i < num_ssid-1; i++) {
                snprintf(listed_number, 4, "%d. ", i+1);
                char* token;
                char* ssid;

                token = strtok(ssid_entry[i], delimiter_comma);
                token = strtok(NULL, delimiter_comma);
                ssid = strtok(token, delimiter_quote);

                UARTSend(UART0_BASE, (uint8_t *)listed_number, strlen(listed_number));
                UARTSend(UART0_BASE, (uint8_t *)ssid, strlen(ssid));
                UARTSend(UART0_BASE, (uint8_t *)"\n\r", strlen("\n\r"));
                put(ssid, i);
            }

            UARTSend(UART0_BASE, (uint8_t *)"Choose network: \n\r Type 0 to exit \n\r", strlen("Choose network: \n\r Type 0 to exit \n\r"));

            char choice2_char[4] = "";
            i = 0;

            while(1) {
                 char k = UARTCharGet(UART0_BASE);
                 UARTCharPut(UART0_BASE, k);
                 if(k == '\n' || k == '\r') break;
                 choice2_char[i++] = k;
            }

            UARTSend(UART0_BASE, "\r\n", strlen("\r\n"));

            int choice2 = atoi(choice2_char) - 1;
            if (choice2 < 0 || choice2 >= num_ssid)
                continue;

            UARTSend(UART0_BASE, (uint8_t *) ssid_list[choice2], strlen(ssid_list[choice2]));
            UARTSend(UART0_BASE, (uint8_t *)"\n\r", strlen("\n\r"));

            UARTSend(UART0_BASE, (uint8_t *)"Password: \n\r", strlen("Password: \n\r"));

            char password[64];
            i = 0;
            password[i] = UARTCharGet(UART0_BASE);
            UARTCharPut(UART0_BASE, '*');

            while(1) {
                 char k = UARTCharGet(UART0_BASE);
                 UARTCharPut(UART0_BASE, '*');
                 if(k == '\n' || k == '\r') break;
                 password[++i] = k;
            }
            password[++i] = '\0';

            UARTSend(UART0_BASE, "\r\n", strlen("\r\n"));

            char text[128];
            snprintf(text, 128, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid_list[choice2], password);

            UARTSend(UART5_BASE, (uint8_t *)text, strlen(text));

            while(command_finished == 0) {
            }
            command_finished = 0;

            break;
        case '3':
            UARTSend(UART0_BASE, (uint8_t *)"First run server.py file. \n\r Type the number of port you'd like to use. \n\r", strlen("First run server.py file. \n\r Type the number of port you'd like to use. \n\r"));

            char port_number_char[5] = "";
            i = 0;
            port_number_char[i] = UARTCharGet(UART0_BASE);
            UARTCharPut(UART0_BASE, port_number_char[i]);

            while(1) {
                 char k = UARTCharGet(UART0_BASE);
                 UARTCharPut(UART0_BASE, k);
                 if(k == '\n' || k == '\r') break;
                 port_number_char[++i] = k;
            }

            UARTSend(UART0_BASE, "\r\n", strlen("\r\n"));

            int port_number = atoi(port_number_char);

            UARTSend(UART0_BASE, (uint8_t *)"Enter IP address you'd like to message. \n\r", strlen("Enter IP address you'd like to message. \n\r"));

            char ip_address[16] = "";
            i = 0;
            ip_address[i] = UARTCharGet(UART0_BASE);
            UARTCharPut(UART0_BASE, ip_address[i]);

            while(1) {
                 char k = UARTCharGet(UART0_BASE);
                 UARTCharPut(UART0_BASE, k);
                 if(k == '\n' || k == '\r') break;
                 ip_address[++i] = k;
            }
            ip_address[++i] = '\0';
            UARTSend(UART0_BASE, "\r\n", strlen("\r\n"));

            snprintf(text, 128, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ip_address, port_number);
            UARTSend(UART5_BASE, (uint8_t *)text, strlen(text));

            while(command_finished == 0) {
            }

            command_finished = 0;
            break;
        case '4':
            passthrough_mode = 1;
            UARTSend(UART0_BASE, (uint8_t *)"Write your messages. +++ to exit \n\r", strlen("Write your messages. +++ to exit \n\r"));

            while(passthrough_mode == 1) {
                char message [128] = "";
                i = 0;
                message[i] = UARTCharGet(UART0_BASE);
                UARTCharPut(UART0_BASE, message[i]);

                while(1) {
                     char k = UARTCharGet(UART0_BASE);
                     UARTCharPut(UART0_BASE, k);
                     if(k == '\n' || k == '\r') break;
                     message[++i] = k;
                }
                message[++i] = '\0';

                if (strcmp(message, "+++") == 0) {
                    passthrough_mode = 0;
                    break;
                }

                char text1[128];
                snprintf(text1, 128, "AT+CIPSEND=%d\r\n", strlen(message));

                UARTSend(UART5_BASE, (uint8_t *)text1, strlen(text1));

                SysCtlDelay(1000 * (SysCtlClockGet() / 3 / 1000));

                while(command_finished == 0) {
                }

                command_finished = 0;

                UARTSend(UART5_BASE, (uint8_t *)message, strlen(message));
            }

            break;
        case '5':
            UARTSend(UART5_BASE, (uint8_t *)"AT+RESTORE\r\n", strlen("AT+RESTORE\r\n"));
            break;
        default:
            break;
        }
    }
}
