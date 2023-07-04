#include "platform/hifive/sdcard.h"
#include "platform/hifive/spi_hifive.h"
#include "driver/disk.h"
#include "atomic/semaphore.h"
#include "fs/bio.h"
#include "debug.h"

#include <stdint.h>

int polltest(size_t timer, uint8 tar, uint8 mask, char* msg) {
    while (timer) {
        if ( (spi_read() & mask) == tar) {
            break;
        }
        --timer;
    }
    if (timer == 0) panic(msg);
    return 1;
}

// CRC7 多项式：x^7 + x^3 + 1
#define CRC7_POLY 0x89
// 计算SD命令的CRC7  ( written by chatGPT)
uint8_t calculate_crc7(const uint8_t *data, size_t length) {
    uint8_t crc = 0;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC7_POLY;
            } else {
                crc <<= 1;
            }
        }
    }
    
    crc = (crc << 1) | 1;  // 加上结束位
    
    return crc & 0x7F;  // 只保留7位CRC值
}

// ( written by chatGPT)
uint8 gen_crc(uint8 cmd, uint32 arg) {
    uint8 sd_cmd_buffer[6];
    sd_cmd_buffer[0] = cmd;
    sd_cmd_buffer[1] = (arg >> 24) & 0xFF;
    sd_cmd_buffer[2] = (arg >> 16) & 0xFF;
    sd_cmd_buffer[3] = (arg >> 8) & 0xFF;
    sd_cmd_buffer[4] = arg & 0xFF;
    
    // 调用calculateCRC7函数计算CRC7值
    uint8 calculated_crc = calculate_crc7(sd_cmd_buffer, 5); // 传入除CRC字段之外的5个字节
    return calculated_crc;
}


static struct sdcard_disk {
    struct semaphore mutex_disk;
} sdcard_disk;

void __sdcard_cmd(uint8 cmd, uint32 arg, uint8 crc) {
    ASSERT(cmd < 64);
    QSPI2_CSMODE &= (~0x3);         
    QSPI2_CSMODE |= 0x2;    // 设置为 HOLD 模式         

    spi_write( 0x40 | cmd );
    spi_write(arg >> 24);
    spi_write(arg >> 16);
    spi_write(arg >> 8);
    spi_write(arg);
    spi_write(( crc << 1) | 1);
    
    wmb();
    
    QSPI2_CSMODE &= (~0x3); // 设置回 AUTO 模式         

}

/*
    ==== 基本命令集 ====
*/
// CMD0
void cmd_go_idle_state() {
    // SD 卡复位
    __sdcard_cmd(SD_CMD0,0,0x4A);   
    return;
}

// CMD9
void cmd_send_csd() {
    ;
    // __sdcard_cmd(SD_CMD9, );
}


// CMD12
void cmd_stop_transmission() {
    // 停止读多块
    uint8 crc = gen_crc(SD_CMD12,0);
    __sdcard_cmd(SD_CMD12,0,crc);
    return;
}

/*
    ==== 块读命令集 ====
*/
// CMD16
void cmd_set_blocklen(uint blksize) {
    // 设置块的长度
    ASSERT(blksize == BSIZE);
    uint8 crc = gen_crc(SD_CMD16,blksize);
    __sdcard_cmd(SD_CMD16,blksize,crc);

}

// CMD17
void cmd_read_single_block(uint sector) {
    // 读单块
    __sdcard_cmd(SD_CMD17,sector * BSIZE,0x2A); 
    return;
}

// CMD18
void cmd_read_mutiple_block(uint sector) {
    // 读多块
    uint8 crc = gen_crc(SD_CMD18,sector*BSIZE);
    __sdcard_cmd(SD_CMD18,sector * BSIZE,crc);
    return;
}

/*
    ==== 块写命令集
*/
// CMD24
void cmd_write_single_block(uint sector) {
    // 写单块
    uint8 crc = gen_crc(SD_CMD24,sector*BSIZE);
    __sdcard_cmd(SD_CMD24,sector * BSIZE,crc);
    return;
}

// CMD25
void cmd_write_mutiple_block(uint sector) {
    // 写多块
    uint8 crc = gen_crc(SD_CMD25,sector*BSIZE);
    __sdcard_cmd(SD_CMD25,sector * BSIZE,crc);
    return;
}


/*
    ==== 应用命令 ====
*/
    // ;

void r1_response() {
    ;
}

void r2_response() {
    ;
}


// TO BE REWRITTEN
void sdcard_disk_init() {
    QSPI2_Init();
    // 上电延时
    for ( int _ = 0; _ != 100; ++_);

    cmd_go_idle_state();     // SD 卡复位

    cmd_set_blocklen(BSIZE); // 设置块长度为 512B

    sema_init(&sdcard_disk.mutex_disk, 1, "sdcard_sem");
    return;
}

static void __sd_single_read(void * addr, uint sec) {
    volatile uint8 *pos;
	pos = (uint8*)addr;
    int timer;
    // 发送 CMD17
    cmd_read_single_block(sec);

    // SD 卡响应 R1 
    // r1_response();
    timer = 10000;
    polltest(timer,0xfe, 0xff, "wait R1 response");

    // 接收一个扇区的数据
    uint32 tot = BSIZE;
    timer = 10000;
    while (tot > 0 && timer) {
        *pos++ = spi_read();
        tot -= 8;
        --timer;
    }

    // 接收两个字节的 CRC，若无开启，这两个字节读取后可丢弃
    spi_read();
    spi_read();

    // 8 CLK 之后禁止片选 : （未处理）

    
    cmd_stop_transmission();
    return;
}

static void __sd_multiple_read(void * addr, uint sec, uint nr_sec) {
    volatile uint8 *pos;
	pos = (uint8*)addr;
    int timer;

    // 发送 CMD18
    cmd_read_mutiple_block(sec);

    // SD 卡响应 R1
    // r1_response();
    timer = 10000;
    polltest(timer,0xfe, 0xff, "wait R1 response");

    // 按块数量连续接收扇区数据
    uint32 tot = nr_sec * BSIZE;
    timer = 10000;
    while (tot > 0 && timer) {
        *pos++ = spi_read();
        tot -= 8;
        --timer;
    }

    // 接收两个字节的 CRC，若无开启，这两个字节读取后可丢弃
    spi_read();
    spi_read();    

    // 发送 CMD12 停止命令
    cmd_stop_transmission();

    // 8 CLK 之后禁止片选 : （未处理）
    return;
}

static void __sd_single_write(void * addr, uint sec) {
    volatile uint8 *pos;
	pos = (uint8*)addr;
    int timer;
    // 发送 CMD24，收到 0x00 表示成功
    cmd_write_single_block(sec);
    timer = 10000;
    polltest(timer, 0x00, 0xff,"sd_single_write:time out!");
    
    // SD 卡响应 R1
    // r1_reponse();
    spi_read();
    spi_read();
    spi_read();

    // 发送写单块开始字节 0xFE
    spi_write(0xfe);

    // 发送 512个字节数据
    uint32 tot = BSIZE;
    timer = 10000;
    while (tot > 0 && timer) {
        spi_write(*pos++);
        tot -= 8;
        --timer;
    }

    // 发送两个字节的伪 CRC，(可以均为 0xff)
    spi_write(0xff);
    spi_write(0xff);

    // 连续读直到读到 XXX00101 表示数据写入成功
    timer = 10000;
    polltest(timer, 0x5, 0x15, "sd_single_write:write failed!");
    
    // 继续读进行忙检测 (读到 0x00 表示SD卡正忙)，当读到 0xff 表示写操作完成  
    timer = 10000;
    polltest(timer, 0xff, 0xff, "sd_single_write:busytest failed!");

    return;
}

static void __sd_multiple_write(void * addr, uint sec, uint nr_sec) {
    volatile uint8 *pos;
	pos = (uint8*)addr;
    int timer;
    
    // 发送 CMD25, 收到 0x00 表示成功
    cmd_write_mutiple_block(sec);
    timer = 10000;
    polltest(timer, 0x00, 0xff, "sd_multiple_write:time out!");
    
    do {
        // 发送若干时钟
        for ( int _ = 0; _ != 100; ++_) {;}

        // 发送写多块开始字节 0xfc
        spi_write(0xfc);

        // 发送 512 字节
        uint32 tot = BSIZE;
        timer = 10000;
        while (tot > 0 && timer) {
            spi_write(*pos++);
            tot -= 8;
            --timer;
        }

        // 发送两个 CRC （可以均为 0xff）
        spi_write(0xff);
        spi_write(0xff);

        // 6.连续读直到读到 XXX00101 表示数据写入成功
        timer = 10000;
        polltest(timer, 0x5, 0x1f, "sd_multiple_write:write failed!");

        // 7.继续读进行忙检测，直到读到 0xFF 表示写操作完成
        timer = 10000;
        polltest(timer, 0xff, 0xff, "sd_multiple_write:write failed!");

    } while (--nr_sec);

    // 发送写多块停止字节 0xFD 来停止写操作
    spi_write(0xfd);

    // 10.进行忙检测直到读到 0xFF
    timer = 10000;
    polltest(timer, 0xff, 0xff, "sd_multiple_write:write failed!");

    return;
}


void sdcard_disk_read(void * addr, uint sec, uint nr_sec) {
    sema_wait(&sdcard_disk.mutex_disk);
    
    if (nr_sec == 1) {
        __sd_single_read(addr,sec);
    } else {
        __sd_multiple_read(addr,sec,nr_sec);
    }

    sema_signal(&sdcard_disk.mutex_disk);
}

void sdcard_disk_write(void * addr, uint sec, uint nr_sec) {
    sema_wait(&sdcard_disk.mutex_disk);
    
    if (nr_sec == 1) {
        __sd_single_write(addr,sec);
    } else {
        __sd_multiple_write(addr,sec,nr_sec);
    }

    sema_signal(&sdcard_disk.mutex_disk);
}

void sdcard_disk_rw(struct bio_vec *bio_vec,  int write) {
    uint sec;
    uint nr_sec;
    
    sec = bio_vec->blockno_start * (BSIZE / 512);
    nr_sec = bio_vec->block_len;
	
    if (write) {
        sdcard_disk_write(bio_vec->data,sec, nr_sec);
    } else {
        sdcard_disk_read(bio_vec->data, sec, nr_sec);
    }

    return;
}   

inline void disk_rw(void * bio_vec,  int write, int type) {
	sdcard_disk_rw((struct bio_vec *)bio_vec, write);
}

void disk_intr() {
    return ;
}

void disk_init() {
    sdcard_disk_init();
}
