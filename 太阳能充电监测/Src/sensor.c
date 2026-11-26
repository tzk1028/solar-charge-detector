#include "board.h"
/* I2C 用 SCL 传时钟，用 SDA 传数据。这里由程序逐个控制引脚。OLED 与 AHT20 使用不同引脚，访问前切到相应总线。 */
static GPIO_TypeDef *cp,*dp;
static unsigned cn,dn;
static void sda(bool v) { pin_write(dp,dn,v); delay_us(5); }
/* 释放 SCL 后读回引脚，允许从设备暂时拉低时钟。超过等待限制就报失败。 */
static bool scl(bool v) {
    pin_write(cp,cn,v);
    if(v) { uint32_t t=system_ms; while(!pin_read(cp,cn)) if((uint32_t)(system_ms-t)>2u) return false; }
    delay_us(5); return true;
}
void i2c_init(GPIO_TypeDef *c,unsigned cpin,GPIO_TypeDef *d,unsigned dpin) {
    cp=c;cn=cpin;dp=d;dn=dpin;
    pin_write(cp,cn,true); pin_write(dp,dn,true);
    pin_mode(cp,cn,1,true); pin_mode(dp,dn,1,true);
    /* 释放总线：给被中断的从机最多9个时钟。外部上拉是必要条件。 */
    for(unsigned i=0;i<9u && !pin_read(dp,dn);++i) { scl(false); scl(true); }
    sda(false); scl(true); sda(true);
}
/* SCL 为高时，SDA 从高变低是开始信号，从低变高是停止信号。它们标出一笔传输的边界。 */
static bool start(void) { sda(true); if(!scl(true)||!pin_read(dp,dn)) return false; sda(false); return scl(false); }
static void stop(void) { sda(false); scl(true); sda(true); }
/* 每次发送 8 位，从最高位开始。第 9 个时钟由接收方把 SDA 拉低来应答，简称 ACK。 */
static bool send(uint8_t v) {
    for(unsigned i=0;i<8u;++i) { sda((v&0x80u)!=0u); if(!scl(true)) return false; scl(false); v=(uint8_t)(v<<1); }
    sda(true); if(!scl(true)) return false; bool ack=!pin_read(dp,dn); scl(false); return ack;
}
/* 把 7 位地址左移，留出最低位给方向。最低位为 0 代表写，下面读函数用 1。 */
bool i2c_write(uint8_t a,const uint8_t *b,unsigned n) {
    bool ok=start(); if(ok) ok=send((uint8_t)(a<<1));
    for(unsigned i=0;i<n && ok;++i) ok=send(b[i]); stop(); return ok;
}
/* 读一个字节时，每跳一次时钟读一位。最后一个字节后释放 SDA 表示 NACK，告诉对方读取结束。 */
bool i2c_read(uint8_t a,uint8_t *b,unsigned n) {
    bool ok=start(); if(ok) ok=send((uint8_t)(((uint32_t)a<<1)|1u));
    for(unsigned i=0;i<n && ok;++i) { uint8_t v=0; sda(true);
        for(unsigned j=0;j<8u;++j) { if(!scl(true)) {ok=false;break;} v=(uint8_t)(((uint32_t)v<<1)|(pin_read(dp,dn)?1u:0u)); scl(false); }
        b[i]=v; sda(i+1u==n); if(!scl(true)) ok=false; scl(false);
    } stop(); return ok;
}
/* AHT20 使用PB6(SCL)和PB7(SDA)。PB3/4/5与PB6共封装脚，保持模拟。 */
static void sensor_bus(void) { i2c_init(GPIOB,6,GPIOB,7); }
/* AHT20 的 I2C 地址为 0x38。先读状态，发现校准标志未置位才发送初始化命令。 */
void sensor_init(void) {
    uint8_t status=0, cmd=0x71; sensor_bus(); delay_ms(40);
    if(i2c_write(0x38,&cmd,1) && i2c_read(0x38,&status,1) && !(status&8u)) {
        const uint8_t init[]={0xBE,0x08,0x00}; i2c_write(0x38,init,3); delay_ms(10);
    }
}
/* 发测量命令后等 80 毫秒，再读取 7 字节。如果还忙、校准标志异常或通信失败，就告诉主程序这轮数据不可用。 */
bool sensor_read(int16_t *temp,uint16_t *rh) {
    const uint8_t trigger[]={0xAC,0x33,0x00}; uint8_t b[7]; sensor_bus();
    if(!i2c_write(0x38,trigger,3)) return false;
    delay_ms(80);
    if(!i2c_read(0x38,b,7) || (b[0]&0x80u) || !(b[0]&8u)) return false;
    /* CRC 根据前 6 字节重新计算校验值，再和第 7 字节比较，用来发现传输中的数据错误。 */
    uint8_t crc=0xFF;
    for(unsigned i=0;i<6u;++i) { crc^=b[i]; for(unsigned j=0;j<8u;++j) crc=(uint8_t)((crc&0x80u)?((uint32_t)crc<<1)^0x31u:(uint32_t)crc<<1); }
    if(crc!=b[6]) return false;
    /*
     * 湿度和温度各占 20 位，共用中间一个字节，因此用移位和掩码拆开。右移 20 位相当于除以 2 的 20 次方。
     * 输出单位分别是 0.1%RH 和 0.1 摄氏度。温度换算最后扣掉 50°C 的偏移。
     */
    uint32_t h=((uint32_t)b[1]<<12)|((uint32_t)b[2]<<4)|(b[3]>>4);
    uint32_t t=((uint32_t)(b[3]&15u)<<16)|((uint32_t)b[4]<<8)|b[5];
    *rh=(uint16_t)((h*1000u)>>20); *temp=(int16_t)((int32_t)((t*2000u)>>20)-500); return true;
}

