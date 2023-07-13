#ifndef __SPI_HIFIVE_H__
#define __SPI_HIFIVE_H__
#include "common.h"

void QSPI2_Init();
void spi_write(uint8 dataframe);
uint8 spi_read();

// base address
#define QSPI_2_BASE ((unsigned int)0x10050000)

// SPI control registers address
// (present only on controllers with the direct-map flash interface)
#define QSPI2_SCKDIV        *(unsigned int *)(QSPI_2_BASE + 0x00)  // Serial clock divisor
#define QSPI2_SCKMODE       *(unsigned int *)(QSPI_2_BASE + 0x04) // Serial clock mode
#define QSPI2_CSID          *(unsigned int *)(QSPI_2_BASE + 0x10)    // Chip select ID
#define QSPI2_CSDEF         *(unsigned int *)(QSPI_2_BASE + 0x14)   // Chip select default
#define QSPI2_CSMODE        *(unsigned int *)(QSPI_2_BASE + 0x18)  // Chip select mode
#define QSPI2_DELAY0        *(unsigned int *)(QSPI_2_BASE + 0x28)  // Delay control 0
#define QSPI2_DELAY1        *(unsigned int *)(QSPI_2_BASE + 0x2C)  // Delay control 1
#define QSPI2_FMT           *(unsigned int *)(QSPI_2_BASE + 0x40)     // Frame format
#define QSPI2_TXDATA        *(unsigned int *)(QSPI_2_BASE + 0x48)  // Tx FIFO data
#define QSPI2_RXDATA        *(unsigned int *)(QSPI_2_BASE + 0x4C)  // Rx FIFO data
#define QSPI2_TXMARK        *(unsigned int *)(QSPI_2_BASE + 0x50)  // Tx FIFO watermark
#define QSPI2_RXMARK        *(unsigned int *)(QSPI_2_BASE + 0x54)  // Rx FIFO watermark
#define QSPI2_FCTRL         *(unsigned int *)(QSPI_2_BASE + 0x60)   // SPI flash interface control
#define QSPI2_FFMT          *(unsigned int *)(QSPI_2_BASE + 0x64)    // SPI flash instruction format
#define QSPI2_IE            *(unsigned int *)(QSPI_2_BASE + 0x70)      // SPI interrupt enablle
#define QSPI2_IP            *(unsigned int *)(QSPI_2_BASE + 0x74)      // SPI interrupt pending



// chip select mode
#define CSMODE_AUTO          0       // AUTO Assert/deassert CS at the beginning/end of each frame
#define CSMODE_HOLD          2       // HOLD Keep CS continuously asserted after the initial frame
#define CSMODE_OFF           3       // OFF Disable hardware control of the CS pin

#define wmb() __asm__ __volatile__("fence w,o" \
                                   :           \
                                   :           \
                                   : "memory")
#define rmb() __asm__ __volatile__("fence i,r" \
                                   :           \
                                   :           \
                                   : "memory")


#endif