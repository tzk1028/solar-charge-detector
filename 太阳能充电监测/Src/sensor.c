#include "board.h"
static GPIO_TypeDef *cp,*dp;
static unsigned cn,dn;
static void sda(bool v) { pin_write(dp,dn,v); delay_us(5); }
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
static bool start(void) { sda(true); if(!scl(true)||!pin_read(dp,dn)) return false; sda(false); return scl(false); }
static void stop(void) { sda(false); scl(true); sda(true); }
static bool send(uint8_t v) {
    for(unsigned i=0;i<8u;++i) { sda((v&0x80u)!=0u); if(!scl(true)) return false; scl(false); v=(uint8_t)(v<<1); }
    sda(true); if(!scl(true)) return false; bool ack=!pin_read(dp,dn); scl(false); return ack;
}
bool i2c_write(uint8_t a,const uint8_t *b,unsigned n) {
    bool ok=start(); if(ok) ok=send((uint8_t)(a<<1));
    for(unsigned i=0;i<n && ok;++i) ok=send(b[i]); stop(); return ok;
}
bool i2c_read(uint8_t a,uint8_t *b,unsigned n) {
    bool ok=start(); if(ok) ok=send((uint8_t)(((uint32_t)a<<1)|1u));
    for(unsigned i=0;i<n && ok;++i) { uint8_t v=0; sda(true);
        for(unsigned j=0;j<8u;++j) { if(!scl(true)) {ok=false;break;} v=(uint8_t)(((uint32_t)v<<1)|(pin_read(dp,dn)?1u:0u)); scl(false); }
        b[i]=v; sda(i+1u==n); if(!scl(true)) ok=false; scl(false);
    } stop(); return ok;
}
/* AHT20 使用PB6(SCL)和PB7(SDA)。PB3/4/5与PB6共封装脚，保持模拟。 */
static void sensor_bus(void) { i2c_init(GPIOB,6,GPIOB,7); }
void sensor_init(void) {
    uint8_t status=0, cmd=0x71; sensor_bus(); delay_ms(40);
    if(i2c_write(0x38,&cmd,1) && i2c_read(0x38,&status,1) && !(status&8u)) {
        const uint8_t init[]={0xBE,0x08,0x00}; i2c_write(0x38,init,3); delay_ms(10);
    }
}
bool sensor_read(int16_t *temp,uint16_t *rh) {
    const uint8_t trigger[]={0xAC,0x33,0x00}; uint8_t b[7]; sensor_bus();
    if(!i2c_write(0x38,trigger,3)) return false;
    delay_ms(80);
    if(!i2c_read(0x38,b,7) || (b[0]&0x80u) || !(b[0]&8u)) return false;
    uint8_t crc=0xFF;
    for(unsigned i=0;i<6u;++i) { crc^=b[i]; for(unsigned j=0;j<8u;++j) crc=(uint8_t)((crc&0x80u)?((uint32_t)crc<<1)^0x31u:(uint32_t)crc<<1); }
    if(crc!=b[6]) return false;
    uint32_t h=((uint32_t)b[1]<<12)|((uint32_t)b[2]<<4)|(b[3]>>4);
    uint32_t t=((uint32_t)(b[3]&15u)<<16)|((uint32_t)b[4]<<8)|b[5];
    *rh=(uint16_t)((h*1000u)>>20); *temp=(int16_t)((int32_t)((t*2000u)>>20)-500); return true;
}

