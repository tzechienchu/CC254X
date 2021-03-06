//HAL for RTC 8563

#include "hal_i2c.h"
#include "hal_sensor.h"

#define PCF8563_ADDRESS_WRITE   (0xA2 >> 1)
#define PCF8563_ADDRESS_READ    (0xA3 >> 1)

#define PCF8563_REG_ST1		0x00 /* status */
#define PCF8563_REG_ST2		0x01
#define PCF8563_REG_SC		0x02 /* datetime */
#define PCF8563_REG_MN		0x03
#define PCF8563_REG_HR		0x04
#define PCF8563_REG_DM		0x05
#define PCF8563_REG_DW		0x06
#define PCF8563_REG_MO		0x07
#define PCF8563_REG_YR		0x08

#define PCF8563_REG_AMN		0x09 /* alarm */
#define PCF8563_REG_AHR		0x0A
#define PCF8563_REG_ADM		0x0B
#define PCF8563_REG_ADW		0x0C

#define PCF8563_REG_CLKO	0x0D /* clock out */
#define PCF8563_REG_TMRC	0x0E /* timer control */
#define PCF8563_REG_TMR		0x0F /* timer */

#define PCF8563_SC_LV		0x80 /* low voltage */
#define PCF8563_MO_C		0x80 /* century */

typedef struct RTC {
   uint8 day;
   uint8 weekday;
   uint8 month;
   uint8 year;
   uint8 sec;
   uint8 minute;
   uint8 hour;
   uint8 st1;
   uint8 st2;
   uint8 century;
} RTC;

RTC myRTC;
uint8 decToBcd(uint8 val)
{
  return ((val/10*16)+(val%10));
}

uint8 bcdToDec(uint8 val)
{
  return ((val/16*10)+(val%16));
}

void PCF8563_Init(void)
{
   uint8 cmds[20] = {0,0,0,1,2,3,4,5,6,7,0x80,0x80,0x80,0x80,0,0};
   HalI2CInit(PCF8563_ADDRESS_WRITE,i2cClock_123KHZ);
   HalSensorWriteReg2(PCF8563_ADDRESS_WRITE,cmds,16);     
}

void PCF8563_ReadDate()
{
   uint8 cmd = 0;
   uint8 dateval[4] = {0};
   
   HalI2CInit(PCF8563_ADDRESS_WRITE,i2cClock_123KHZ);
   cmd = PCF8563_REG_DM;
   HalSensorWriteReg2(PCF8563_ADDRESS_WRITE,&cmd,1);
   
   HalSensorReadReg2(PCF8563_ADDRESS_READ,dateval,4);
   myRTC.day     = bcdToDec(dateval[0] & 0x3F);
   myRTC.weekday = bcdToDec(dateval[1] & 0x07);
   myRTC.month   = bcdToDec(dateval[2] & 0x1F);
   myRTC.year    = bcdToDec(dateval[3]);
   if (dateval[2] & 0x80) {
     myRTC.century = 1;
   }
   else {
     myRTC.century = 0;
   }   
}

void PCF8563_ReadTime()
{
   uint8 cmd = 0;
   uint8 dateval[8] = {0};
   
   HalI2CInit(PCF8563_ADDRESS_WRITE,i2cClock_123KHZ);
   cmd = PCF8563_REG_ST1;
   HalSensorWriteReg2(PCF8563_ADDRESS_WRITE,&cmd,1);
   
   HalSensorReadReg2(PCF8563_ADDRESS_READ,dateval,6);
   myRTC.st1     = dateval[0];
   myRTC.st2     = dateval[1];
   myRTC.sec     = bcdToDec(dateval[2] & 0x7F);
   myRTC.minute  = bcdToDec(dateval[3] & 0x7F);
   myRTC.hour    = bcdToDec(dateval[4] & 0x3F);   
}

void PCF8563_WriteTime(uint8 hour,uint8 minute,uint8 sec)
{
   uint8 cmds[12];
   HalI2CInit(PCF8563_ADDRESS_WRITE,i2cClock_123KHZ);
   cmds[0] = PCF8563_REG_SC;
   cmds[1] = decToBcd(sec);
   cmds[2] = decToBcd(minute);
   cmds[3] = decToBcd(hour);
   HalSensorWriteReg2(PCF8563_ADDRESS_WRITE,cmds,4);     
}

void PCF8563_WriteDate(uint8 century,uint8 year,uint8 month,uint8 day,uint8 weekday)
{
   uint8 cmds[12];
   HalI2CInit(PCF8563_ADDRESS_WRITE,i2cClock_123KHZ);
   month = decToBcd(month);
   if (century == 1) {
     month |= 0x80;
   } else {
     month &= ~0x80;
   }
   cmds[0] = PCF8563_REG_DM;
   cmds[1] = decToBcd(day);
   cmds[2] = decToBcd(weekday);
   cmds[3] = month;
   cmds[4] = decToBcd(year);
   
   HalSensorWriteReg2(PCF8563_ADDRESS_WRITE,cmds,5);     
}
void formatTime(uint8 *strOut)
{
  strOut[0] = '0' + (myRTC.hour / 10);
  strOut[1] = '0' + (myRTC.hour % 10);
  strOut[2] = ':';
  strOut[3] = '0' + (myRTC.minute / 10);
  strOut[4] = '0' + (myRTC.minute % 10);
  strOut[5] = ':';
  strOut[6] = '0' + (myRTC.sec / 10);
  strOut[7] = '0' + (myRTC.sec % 10);
  strOut[8] = '\0';  
}

void formatDate(uint8 *strDate)
{
  if ( myRTC.century == 1 ){
    strDate[0] = '1';
    strDate[1] = '9';
  }
  else {
    strDate[0] = '2';
    strDate[1] = '0';
  }
  strDate[2] = '0' + (myRTC.year / 10 );
  strDate[3] = '0' + (myRTC.year % 10);
  strDate[4] = '-';	
  strDate[5] = '0' + (myRTC.month / 10);
  strDate[6] = '0' + (myRTC.month % 10);
  strDate[7] = '-';
  strDate[8] = '0' + (myRTC.day / 10);
  strDate[9] = '0' + (myRTC.day % 10);
  strDate[10] = '\0';  
}

