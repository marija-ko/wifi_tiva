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

//*****************************************************************************
//
// The UART5 interrupt handler.
//
//*****************************************************************************
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

    UARTCharPutNonBlocking(UART0_BASE, UARTCharGetNonBlocking(UART5_BASE));
    while(UARTCharsAvail(UART5_BASE))
    {
        char k = UARTCharGetNonBlocking(UART5_BASE);
        UARTCharPutNonBlocking(UART0_BASE, k);
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
        UARTSend(UART0_BASE, (uint8_t *)"Command List:\r\n 1. Set mode \r\n ",
                        strlen("Command List:\r\n 1. Set mode \r\n "));

        uint8_t choice = UARTCharGet(UART0_BASE);

        UARTCharPut(UART0_BASE, choice);
        UARTSend(UART0_BASE, "\r\n", strlen("\r\n"));

        switch(choice)
        {

        }
    }
}
