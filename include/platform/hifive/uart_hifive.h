#ifndef __UART_HIFIVE_H__
#define __UART_HIFIVE_H__

// Memory map
#define UART_BASE     0x10000000L // UART base address
#define TXDATA        0x00// Transmit data register
#define RXDATA        0x04// Receive data register
#define TXCTRL        0x08// Transmit control register
#define RXCTRL        0x0C// Receive control register
#define IE            0x10// UART interrupt enable
#define IP            0x14// UART interrupt pending
#define DIV           0x18// Baud rate divisor

// Transmit Control Register (txctrl)
#define TXEN_BIT  0
#define TXEN_MASK  (0x1 << TXEN_BIT)

// read and write registers
#define Reg(reg) ((volatile unsigned char *)(UART_BASE + reg))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// init
void uart_init();

// put and get char
void uart_putc(char ch);
int uart_getc();




#endif