// #ifndef __SDCARD_H__
// #define __SDCARD_H__
// #include "fs/bio.h"

// SD 卡总共有 8 个寄存器，它们只能通过对应的命令来访问

#define SD_CMD0          0      // 复位 SD 卡
#define SD_CMD1          1      // 读 OCR 寄存器
#define SD_CMD9          9      // 读 CSD 寄存器
#define SD_CMD10         10     // 读 CID 寄存器
#define SD_CMD12         12     // 停止读多块时的数据传输
#define SD_CMD13         13     // 读 Card_Status 寄存器
#define SD_CMD16         16     // 设置块的长度
#define SD_CMD17         17     // 读单块
#define SD_CMD18         18     // 读多块，直至主机发送 CMD12 为止
#define SD_CMD24         24     // 写单块
#define SD_CMD25         25     // 写多块
#define SD_CMD27         27     // 写 CSD 寄存器
#define SD_CMD28         28     // 设置地址保护组保护位
#define SD_CMD29         29     // 清除保护位   
#define SD_CMD30         30     // 要求卡发送写保护状态，参数中有要查询的dizhi

// #endif
