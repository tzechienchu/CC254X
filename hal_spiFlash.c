
//#include<ioCC2540.h>
#include "hal_i2c.h"
#include "hal_lcd.h"

//#define XNV_STAT_CMD  0x05
//#define XNV_WREN_CMD  0x06
//#define XNV_WRPG_CMD  0x02
//#define XNV_READ_CMD  0x0B
#define XNV_STAT_WIP  0x01

#define SPIFLASH_WRITEENABLE      0x06        // write enable
#define SPIFLASH_WRITEDISABLE     0x04        // write disable

#define SPIFLASH_BLOCKERASE_4K    0x20        // erase one 4K block of flash memory
#define SPIFLASH_BLOCKERASE_32K   0x52        // erase one 32K block of flash memory
#define SPIFLASH_BLOCKERASE_64K   0xD8        // erase one 64K block of flash memory
#define SPIFLASH_CHIPERASE        0x60        // chip erase (may take several seconds depending on size)
                                              // but no actual need to wait for completion (instead need to check the status register BUSY bit)
#define SPIFLASH_STATUSREAD       0x05        // read status register
#define SPIFLASH_STATUSWRITE      0x01        // write status register
#define SPIFLASH_ARRAYREAD        0x0B        // read array (fast, need to add 1 dummy byte after 3 address bytes)
#define SPIFLASH_ARRAYREADLOWFREQ 0x03        // read array (low frequency)

#define SPIFLASH_SLEEP            0xB9        // deep power down
#define SPIFLASH_WAKE             0xAB        // deep power wake up
#define SPIFLASH_BYTEPAGEPROGRAM  0x02        // write (1 to 256bytes)
#define SPIFLASH_IDREAD           0x9F        // read JEDEC manufacturer and device ID (2 bytes, specific bytes for each manufacturer and device)
                                              // Example for Atmel-Adesto 4Mbit AT25DF041A: 0x1F44 (page 27: http://www.adestotech.com/sites/default/files/datasheets/doc3668.pdf)
                                              // Example for Winbond 4Mbit W25X40CL: 0xEF30 (page 14: http://www.winbond.com/NR/rdonlyres/6E25084C-0BFE-4B25-903D-AE10221A0929/0/W25X40CL.pdf)


#define BV(n) (1<<(n))
#define st(x)      do { x } while (__LINE__ == -1)
/* ----------- XNV ---------- */
#define XNV_SPI_BEGIN()             st(P1_3 = 0;)
#define XNV_SPI_TX(x)               st(U1CSR &= ~0x02; U1DBUF = (x);)
#define XNV_SPI_RX()                U1DBUF
#define XNV_SPI_WAIT_RXRDY()        st(while (!(U1CSR & 0x02));)
#define XNV_SPI_END()               st(P1_3 = 1;)


// The TI reference design uses UART1 Alt. 2 in SPI mode.
#define XNV_SPI_INIT() \
st( \
  /* Mode select UART1 SPI Mode as master. */\
  U1CSR = 0; \
  \
  /* Setup for 115200 baud. */\
  U1GCR = 11; \
  U1BAUD = 216; \
  \
  /* Set bit order to MSB */\
  U1GCR |= BV(5); \
  \
  /* Set UART1 I/O to alternate 2 location on P1 pins. */\
  PERCFG |= 0x02;  /* U1CFG */\
  \
  /* Select peripheral function on I/O pins but SS is left as GPIO for separate control. */\
  P1SEL |= 0xE0;  /* SELP1_[7:4] */\
  /* P1.1,2,3: reset, LCD CS, XNV CS. */\
  P1SEL &= ~0x0E; \
  P1 |= 0x0E; \
  P1_1 = 0; \
  P1DIR |= 0x0E; \
  \
  /* Give UART1 priority over Timer3. */\
  P2SEL &= ~0x20;  /* PRI2P1 */\
  \
  /* When SPI config is complete, enable it. */\
  U1CSR |= 0x40; \
  /* Release XNV reset. */\
  P1_1 = 1; \
)

void HalHW_WaitUs(uint16 microSecs)
{
  while(microSecs--)
  {
    /* 3 NOPs == 1 usecs */
    asm("NOP");asm("NOP");asm("NOP");
  }
}

void HalHW_WaitMS(uint16 ms)
{
  while(ms--){
    int i=0;
    for(i=20; i>0; i--)
      HalHW_WaitUs(50);
  }
}

void HalSPIInit()
{
  XNV_SPI_INIT();
}
void xnvSPIWrite(uint8 ch)
{
  XNV_SPI_TX(ch);
  XNV_SPI_WAIT_RXRDY();
}

void HalSPIRead(uint32 addr, uint8 *pBuf, uint16 len)
{

  uint8 shdw = P1DIR;
  P1DIR |= BV(3);
  
  XNV_SPI_BEGIN();
  do {
    xnvSPIWrite(SPIFLASH_STATUSREAD);
  } while (XNV_SPI_RX() & XNV_STAT_WIP);
  XNV_SPI_END();
  asm("NOP"); asm("NOP");

  XNV_SPI_BEGIN();
  xnvSPIWrite(SPIFLASH_ARRAYREAD);
  xnvSPIWrite(addr >> 16);
  xnvSPIWrite(addr >> 8);
  xnvSPIWrite(addr);
  xnvSPIWrite(0);

  while (len--)
  {
    xnvSPIWrite(0);
    *pBuf++ = XNV_SPI_RX();
  }
  XNV_SPI_END();

  P1DIR = shdw;
}

void HalSPIWrite(uint32 addr, uint8 *pBuf, uint16 len)
{
  uint8 cnt;
  uint8 shdw = P1DIR;
  P1DIR |= BV(3);
  while (len)
  {
    XNV_SPI_BEGIN();
    do {
      xnvSPIWrite(SPIFLASH_STATUSREAD);
    } while (XNV_SPI_RX() & XNV_STAT_WIP);
    XNV_SPI_END();
    asm("NOP"); asm("NOP");

    XNV_SPI_BEGIN();
    xnvSPIWrite(SPIFLASH_WRITEENABLE);
    XNV_SPI_END();
    asm("NOP"); asm("NOP");

    XNV_SPI_BEGIN();
    xnvSPIWrite(SPIFLASH_BYTEPAGEPROGRAM);
    xnvSPIWrite(addr >> 16);
    xnvSPIWrite(addr >> 8);
    xnvSPIWrite(addr);

    // Can only write within any one page boundary, so prepare for next page write if bytes remain.
    cnt = 0 - (uint8)addr;
    if (cnt)
    {
      addr += cnt;
    }
    else
    {
      addr += 256;
    }

    do
    {
      xnvSPIWrite(*pBuf++);
      cnt--;
      len--;
    } while (len && cnt);
    XNV_SPI_END();
  }
  P1DIR = shdw;
}




int TestSPI()
{
  uint8 buf[10]="123456789";
  uint8 bufrx[10]; 
  
  //HalLcd_HW_Init();
  
  HalLcd_HW_WriteLine(HAL_LCD_LINE_1, "      CC2540EM");
  HalLcd_HW_WriteLine(HAL_LCD_LINE_3, "-->SPI-FLASH_TEST"); 
  
  //XNV_SPI_INIT();

  //HalSPIWrite(0x0,buf,10);
  HalLcd_HW_WriteLine(HAL_LCD_LINE_4, "Write:"); 
  HalLcd_HW_WriteLine(HAL_LCD_LINE_5, buf); 

  //HalHW_WaitUs(800);
  //HalSPIRead(0x0,bufrx,10);
  HalLcd_HW_WriteLine(HAL_LCD_LINE_6, "Read:"); 
  HalLcd_HW_WriteLine(HAL_LCD_LINE_7, bufrx); 
  //while(1);
}
