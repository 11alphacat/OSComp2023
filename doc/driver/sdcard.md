# SD卡驱动

## 一、SPI接口

​		SiFive FU740 支持基于 SPI 通信协议来驱动 SD 卡，板卡上与 SD 卡连接的 SPI控制器为 QSPI2。查阅 [FU740 的手册](https://sifive.cdn.prismic.io/sifive/1a82e600-1f93-4f41-b2d8-86ed8b16acba_fu740-c000-manual-v1p6.pdf)，可以找到相关寄存器的物理地址。我们采用寄存器开发模式，将 SPI 的寄存器地址强制转化为指针，作为左值使用。当然，内核页表也要加入对这些物理地址的直接映射。

**SPI 寄存器**

```c
// base address
#define QSPI_2_BASE ((unsigned int)0x10050000)

// SPI control registers address
#define QSPI2_TXDATA        *(volatile unsigned int *)(QSPI_2_BASE + 0x48)  // Tx FIFO data

#define QSPI2_RXDATA        *(volatile unsigned int *)(QSPI_2_BASE + 0x4C)  // Rx FIFO data

...
```

**SPI 通信模式**

​	SPI 通信模式主要是主从方式通信，这种模式下通常只有一个主机和一个或多个从机。主机和从机都有一个串行移位寄存器，当主机需要往从机发送数据时，便向自己的 SPI 串行寄存器写入一个字节来发起传输；当主机需要读取从机的一个字节时，则需要发送一个空字节来引发从机的传输。
​	也就是说，SPI 是一个环形总线结构，**主从的读写是同步的**。

​	基于此原理，我们编写了底层的 SPI 传输函数，并以此封装了 **spi_read()** 与 **spi_write()** 供 SD 卡驱动层使用。

```c
// 读写同步很重要
static uint8 __spi_xfer(uint8 dataframe) {
    int r;
    QSPI2_TXDATA = dataframe;
    rmb();

    do {
        r = QSPI2_RXDATA;
    } while (r < 0);
    
    return (r & 0xff);
}

inline void spi_write(uint8 dataframe) {
    __spi_xfer(dataframe);
}

inline uint8 spi_read() {
    return __spi_xfer(0xff);
}
```

## 二、SD卡初始化

​	启动板卡时，参考使用 SD 卡 启动方式时板卡固件程序的行为，可知步骤如下：

1. 启动初始化命令前等待 1 ms；
2. 将 QSPI 控制器时钟频率调至 400 kHz;
3. 在 CS 片选信号未激活的状态下发送 10 个 SPI 时钟；
4. 发送 CMD0，CMD8，ACMD41，CMD58，CMD16；
5. 将 QSPI 控制器时钟频率调至 20 MHz。

我们在 **QSPI2_Init()** 中实现了对 QSPI 控制器的初始化，在 **__sd_init()** 中完成对 SD 卡的初始化。

## 三、SD卡协议

​	对 SD 卡的控制操作不能像操作 SPI 相关寄存器那样直接访问，而是必须要通过命令来控制。

**SD 卡命令**

​	SD 卡拥有统一的命令格式，固定为 48 bit，格式如下图所示。

![sd卡命令格式](../image/sdcard.assets/SD%E5%8D%A1%E5%91%BD%E4%BB%A4%E6%A0%BC%E5%BC%8F.png) 
(图源自：[SD卡协议中文](https://picture.iczhiku.com/resource/eetop/wHKwutAIPkWkWMbn.pdf))
```c
static void __sdcard_cmd(uint8 cmd, uint32 arg, uint8 crc) {

    QSPI2_CSMODE = CSMODE_HOLD; // 设置为 HOLD 模式

    spi_write(0x40 | cmd);
    spi_write(arg >> 24);
    spi_write(arg >> 16);
    spi_write(arg >> 8);
    spi_write(arg);
    spi_write((crc << 1) | 1);

    return;
}
```

​	在 SD  卡初始化中使用的 CMD0 为 SD 卡复位命令，CMD8 为 条件检测命令，ACMD41 为发送操作条件命令，CMD58 为读取 OCR 寄存器命令，CMD16 为设置读写块长度命令。

**SD 卡响应格式**

​	SD 卡对于命令的响应有许多中， R1 为标准响应 48 位，最为常用，格式如下。

![R1响应格式](../image/sdcard.assets/R1%E5%93%8D%E5%BA%94.png) 
(图源自：[SD卡协议中文](https://picture.iczhiku.com/resource/eetop/wHKwutAIPkWkWMbn.pdf))


此外，还有 R1b 、R3（针对命令 CMD58，读取 OCR寄存器）、R7（CMD8 的响应）等响应。

## 四、SD卡读写

​	我们实现了SD 卡的单块读写和多块读写函数。这里仅展示单块读的代码。

```c
static void __sd_single_read(void *addr, uint sec) {
    volatile uint8 *pos;
    pos = (uint8 *)addr;

    // 发送 CMD17
    cmd_read_single_block(sec);
    spi_write(0xff);

    // 接收数据启始令牌 0xFE
    polltest(NULL,0xFE,0xff,"sdcard single read fail!");

    // 接收一个扇区的数据
    uint32 tot = BSIZE;
    uint16 crc = 0;
    uint8 d;
    while (tot-- > 0) {
        d = spi_read();
        *pos++ = d;
        crc = crc16(crc,d);
    }

    // 接收两个字节的 CRC，若无开启，这两个字节读取后可丢弃
    uint16 crc16_ret;
    crc16_ret = (spi_read() << 8);
    crc16_ret |= spi_read();
    
    if (crc != crc16_ret ) {
        panic("single read : crc error!\n");
    }

    // 8 CLK 之后禁止片选（未处理）
    for (int _ = 0; _ != 11; ++_) {;}

    spi_write(0xff);
    QSPI2_CSMODE = CSMODE_OFF;
    
    return;
}
```

​	我们编写了一个测试函数来检验 I/O 抗压性能。我们的代码能够通过该测试，并且从后续板卡上的测试情况来看，驱动的稳定性较好。

```c
void __sd_test() {
    int sec = 0;
    // just for test
    printf("SD card I/O test ...\n");
    // 单块读写连续性测试
    for ( int repeat = 1; repeat <= 114; ++repeat ) {
        char addr[BSIZE] = {0};
        char data_tmp[BSIZE] = {0};
        __sd_single_read(addr, sec);        // 填充进 addr
        memmove(data_tmp,addr,BSIZE);
        __sd_single_write(data_tmp,sec);
        __sd_single_read(addr, sec);        // 填充进 addr
        ASSERT(!memcmp(addr, data_tmp,BSIZE));
printf("No.%d [pass]\n",repeat);
    }

    // 多块读写连续性测试
    for (int nr_sec = 2; nr_sec <= 514; nr_sec *= 2) {
        char *data_tmp = (char*)kzalloc(nr_sec * BSIZE);
        char *addr = (char*)kzalloc(nr_sec * BSIZE);
        ASSERT(data_tmp);
        __sd_multiple_read(addr, sec,nr_sec);        // 填充进 addr
        memmove(data_tmp,addr,nr_sec*BSIZE);
        __sd_multiple_write(data_tmp,sec,nr_sec);
        __sd_multiple_read(addr,sec,nr_sec);
        ASSERT(!memcmp(addr, data_tmp, nr_sec * BSIZE)); 
printf("compare [ok]\n");            
        kfree(data_tmp);             
printf("nr_sec = %d [pass]\n",nr_sec);
    }

    printf("SD card I/O test [ok]\n");

    return;
}
```

