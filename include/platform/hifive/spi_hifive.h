#ifndef __SPI_HIFIVE_H__
#define __SPI_HIFIVE_H__

// Memory map
// Register offsets
#define FMT 0x40     // Frame format
#define TX_DATA 0x48 // Tx FIFO Data
#define RX_DATA 0x4C // Rx FIFO data
#define TX_MARK 0x50 // Tx FIFO watermark
#define RX_MARK 0x54 // Rx FIFO watermark

// Serial Clock Divisor Register (sckdiv)
#define SCKDIV 0x00
// Serial Clock Mode Register (sckmode)
#define SCKMODE 0x04
// Chip Select ID Register (csid)
#define CSID 0x10
// Chip Select Default Register (csdef)
#define CSDEF 0x14
// Chip Select Mode Register (csmode)
#define CSMODE 0x18

// Chip Select Modes
// AUTO Assert/deassert CS at the beginning/end of each frame
#define CSMODE_AUTO 0x1
// HOLD Keep CS continuously asserted after the initial frame
#define CSMODE_HOLD 0x2
// OFF Disable hardware control of the CS pin
#define CSMODE_OFF 0x3

#endif