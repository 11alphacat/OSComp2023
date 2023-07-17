#include "platform/hifive/uart_hifive.h"
#include "common.h"

volatile uarts_t *uarths = (volatile uarts_t *)UART0_BASE;
struct uart_hifve uart;
extern volatile int panicked;

// init
void uart_hifive_init() {
    // tx and rx channel is active.
    uarths->txctrl.txen = 1;
    uarths->rxctrl.rxen = 1;
    // threshold of interrupt triggers
    uarths->txctrl.txcnt = 0;
    uarths->rxctrl.rxcnt = 0;
    // raised less than txcnt
    uarths->ip.txwm = 1;
    uarths->ip.rxwm = 1;
    // raised less than txcnt
    uarths->ie.txwm = 0;
    uarths->ie.rxwm = 1;

    // spinlock for mutex
    initlock(&uart.uart_tx_lock, "uart");
    // semaphore for Sync
    sema_init(&uart.uart_tx_r_sem, 0, "uart_tx_r_sem");
}

// uart interrupt
void uart_hifive_intr(void) {
    while (1) {
        char ch = uart_hifive_getc();
        if (ch == -1)
            break;
        // consoleintr(c);
    }
    acquire(&uart.uart_tx_lock);
    uart_hifve_submit();
    release(&uart.uart_tx_lock);
}

// for asynchronous
void uart_hifve_submit() {
    while (1) {
        // if (BUF_IS_EMPETY(uart) || UART_TX_FULL(uarths)) {
        if (BUF_IS_EMPETY(uart) || UART_TX_FULL) {
            return;
        }
        char ch = UART_BUF_GETCHAR(uart);
        sema_signal(&uart.uart_tx_r_sem);
        // UART_TX_PUTCHAR(uarths, ch);
        UART_TX_PUTCHAR(ch);
    }
}

// asynchronous
void uart_hifive_putc_asyn(char ch) {
    if (panicked) {
        for (;;)
            ;
    }
    while (BUF_IS_FULL(uart)) {
        sema_wait(&uart.uart_tx_r_sem);
    }
    acquire(&uart.uart_tx_lock);
    uart_hifve_submit();
    release(&uart.uart_tx_lock);
}

// synchronous
void uart_hifive_putc_syn(char ch) {
    push_off();
    if (panicked) {
        for (;;)
            ;
    }
    // uint32 txdata = ReadReg_hifive(TXDATA);
    while (ReadReg_hifive(TXDATA) & TX_FULL_MASK);
    // int* uartRegTXFIFO = (int*)(UART0_BASE + TXDATA);
    // if(readl(uartRegTXFIFO)&TX_FULL_MASK) {
    //     int b = 2;
    //     b++;
    // } else {
    //     int a =1;
    //     a++;
    // }
	// while (readl(uartRegTXFIFO) & UART_TXFIFO_FULL);
    // writel(ch, uartRegTXFIFO);
    // if(UART_TX_FULL(uarths)) {
    // if(uarths->txdata.full & TX_FULL_MASK) {
    //     int a = 1;
    //     a ++;
    // } else {
    //     int b = 2;
    //     b ++;
    // }
    // while (UART_TX_FULL(uarths))
    //     ;
    // UART_TX_PUTCHAR(uarths, ch);
    WriteReg_hifive(TXDATA, ch);

    pop_off();
}

int uart_hifive_getc() {
    // if (UART_RX_EMPTY(uarths))
    char ch = UART_RX_GETCHAR;
    if(ch & RX_EMPTY_MASK)
        return -1;
    else {
        return ch; 
    }
        // return UART_RX_GETCHAR;
        // return UART_RX_GETCHAR(uarths);
}

// interfaces for upper layer
void uartinit() {
    uart_hifive_init();
}

void uartintr(void) {
    uart_hifive_intr();
}

void uartputc(int ch) {
    uart_hifive_putc_asyn(ch);
}

void uartputc_sync(int ch) {
    uart_hifive_putc_syn(ch);
}

int uartgetc(void) {
    return uart_hifive_getc();
}