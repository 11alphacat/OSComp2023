#include "platform/hifive/spi_hifive.h"

void QSPI2_Init() {
    // may need TODO():hfpclkpll 时钟初始化； 先假定上电后会初始化好
    QSPI2_SCKDIV &= (~0xfff);
    QSPI2_SCKDIV |= 0x3;            // 波特率为 pclk 时钟 8 分频
    QSPI2_CSID = 0;                 // 设置默认片选 id 为 0
    QSPI2_CSDEF = 1;                // 设置 片选位宽为 1
    QSPI2_CSMODE &= (~0x3);         // 设置 AUTO mode
    QSPI2_FMT |= 0x80005;           // 数据帧 8 位，小端，双工
    QSPI2_FCTRL |= 1;               // 控制器进行直接内存映射

    // QSPI2_IE ...
    
	// REG32(spi, SPI_REG_FMT) = 0x80000;       // 数据帧格式
	// REG32(spi, SPI_REG_CSDEF) |= 1;          
	// REG32(spi, SPI_REG_CSID) = 0;
	// REG32(spi, SPI_REG_SCKDIV) = f;  // ?? 传 3000 是什么操作
	// REG32(spi, SPI_REG_CSMODE) = SPI_CSMODE_OFF; 
	// for (i = 10; i > 0; i--) {
	// 	sd_dummy();
	// }
	// REG32(spi, SPI_REG_CSMODE) = SPI_CSMODE_AUTO;
}

void spi_write(uint8 dataframe) {
    int txdata; 
    do {
        txdata = QSPI2_TXDATA;
    } while ( txdata < 0 );

    wmb();

    QSPI2_TXDATA = dataframe;   // 第 31 位full为 readonly，假设赋值操作不会修改
    return;
}

uint8 spi_read() {
    int rxdata;
    uint8 dataframe;

    spi_write(0x00);    // 发送一个空字节来引发从机的传输
    
    do {
        rxdata = QSPI2_RXDATA;
    } while ( rxdata < 0);

    rmb();

    dataframe = QSPI2_RXDATA & 0xff;

    return dataframe;
}