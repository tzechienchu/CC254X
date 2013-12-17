#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"

#include "OnBoard.h"
#include "hal_adc.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_lcd.h"

#include "hal_uart.h"


#include "gatt.h"
#include "ll.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "central.h"
#include "gapbondmgr.h"
#include "simpleGATTprofile.h"
#include "simpleBLEPeripheral.h"


#include "SerialApp.h"

static uint8 sendMsgTo_TaskID;
extern uint8 sbpGattWriteString(uint8 *pBuffer, uint16 length);
void parseCommand(uint8 *buf);
void indexNMEA(uint8 *buff,uint8 leng);
void parseGPRMC(uint8 *str,uint8 leng);

#define NMXLENG 4
uint8 nmeaWordIndex[NMXLENG];
uint8 nmeaCRIndex[NMXLENG];
uint8 nmeaCommaIndex[16];

uint8 GPSTime[12] = {0};
uint8 GPSLong[12] = {0};
uint8 GPSLat[12]  = {0};
uint8 xtraByte = 0;

void SerialApp_Init( uint8 taskID )
{
  serialAppInitTransport();
  sendMsgTo_TaskID = taskID;
}

void serialAppInitTransport( )
{
  halUARTCfg_t uartConfig;

  // configure UART
  uartConfig.configured           = TRUE;
  uartConfig.baudRate             = SBP_UART_BR;//疏杻薹
  uartConfig.flowControl          = SBP_UART_FC;//霜諷秶
  uartConfig.flowControlThreshold = SBP_UART_FC_THRESHOLD;//霜諷秶蓒硉ㄛ絞羲flowControl奀ㄛ蜆扢离衄虴
  uartConfig.rx.maxBufSize        = SBP_UART_RX_BUF_SIZE;//uart諉彶遣喳湮苤
  uartConfig.tx.maxBufSize        = SBP_UART_TX_BUF_SIZE;//uart楷冞遣喳湮苤
  uartConfig.idleTimeout          = SBP_UART_IDLE_TIMEOUT;
  uartConfig.intEnable            = SBP_UART_INT_ENABLE;//岆瘁羲笢剿
  uartConfig.callBackFunc         = sbpSerialAppCallback;//uart諉彶隙覃滲杅ㄛ婓蜆滲杅笢黍褫蚚uart杅擂

  // start UART
  // Note: Assumes no issue opening UART port.
  (void)HalUARTOpen( SBP_UART_PORT, &uartConfig );

  return;
}
uint16 numBytes;
char   uuid1[18];
char   uuid2[18];

void sbpSerialAppCallback(uint8 port, uint8 event)
{
  uint8  pktBuffer[SBP_UART_RX_BUF_SIZE+8];
  int microSecs = 500;
  //Delay for Data
  while(microSecs--)
  {
    /* 3 NOPs == 1 usecs */
    asm("NOP");asm("NOP");asm("NOP");
  }
  (void)event;
  for(int i=0;i<SBP_UART_RX_BUF_SIZE+8;i++) pktBuffer[i]='\0';
  if ( (numBytes = Hal_UART_RxBufLen(port)) >= (127) ){   
    (void)HalUARTRead (port, pktBuffer, numBytes); 
    indexNMEA(pktBuffer,numBytes);
    parseGPRMC(pktBuffer,numBytes);    
    //SimpleProfile_SetParameter( SIMPLEPROFILE_CHAR2, UART_LEN,pktBuffer );
    //HalLcdWriteString(pktBuffer, HAL_LCD_LINE_7 );
    //parseCommand(pktBuffer);
  }  
  
}
void sbpSerialAppWrite(uint8 *pBuffer, uint16 length)
{
   HalUARTWrite (SBP_UART_PORT, pBuffer, length);
}
//UUID,01,5AFFFFFFFFFFFFFF
//UUID,02,FFFFFFFFFFFFFFFF
extern uint8 beaconUUID[16];
uint8 hex2val(uint8 hex)
{
  switch (hex) {
  case '0': return 0;
  case '1': return 1;
  case '2': return 2;
  case '3': return 3;
  case '4': return 4;
  case '5': return 5;
  case '6': return 6;
  case '7': return 7;
  case '8': return 8;
  case '9': return 9;
  case 'A': return 10;  
  case 'B': return 11; 
  case 'C': return 12; 
  case 'D': return 13; 
  case 'E': return 14; 
  case 'F': return 15; 
  }
  return 0;
}
void str2hexuuid(uint8 *uuid1str,uint8 *uuid2str,uint8 *uuidhex)
{
  uint8 hex1,hex2;
  uint8 ix = 0;
  while(*uuid1str != 0) {
    hex1 = (uint8) *uuid1str++;
    hex2 = (uint8) *uuid1str++;
    uuidhex[ix++] = hex2val(hex1)*16+hex2val(hex2);
  }
  while(*uuid2str != 0) {
    hex1 = (uint8) *uuid2str++;
    hex2 = (uint8) *uuid2str++;
    uuidhex[ix++] = hex2val(hex1)*16+hex2val(hex2);
  }  
}
void parseCommand(uint8 *buf) 
{
  char cmd1[4];
  char cmd2[4];
  strncpy(cmd1,buf,4);
  strncpy(cmd2,buf+5,2);cmd2[2] = '\0';
}
void showStrngInRange(uint8 *str,uint8 from,uint8 to,uint8 line)
{
  int subleng;
  uint8 substr[32];
  uint8 *buffix;
  if (from == 255) return;
  if (to == 255) return;
  if (from >= to) return;
  
  subleng = to - from;
  buffix  = str + from;
  for(int i=0;i<32;i++) substr[i] = '\0';
  if ((subleng > 0) && (subleng < 32)) {
    subleng = 12;
    strncpy(substr,buffix,subleng);
    HalLcdWriteString(substr, line );
  }
}
void subStringByComma(uint8 *dest,uint8 *src,uint8 from,uint8 to)
{
  uint8 leng = to - from - 1;
  if (leng == 0) return;
  if (leng >= 12) leng = 10;
  if (to == 255)   return;
  if (from == 255) return;
  if (from >= to) return;
  
  strncpy(dest,src+(from+1),leng);
}
void parseGPRMC(uint8 *str,uint8 leng)
{
  uint8 buff[96];
  uint8 buffleng;
  uint8 ix = 0;
  uint8 gotGPRMC = 0;
  uint8 commaCount = 0;
  uint8 matchix = 255;
  gotGPRMC = 0;
  //uint8 *sample = "$GPRMC,053322.682,A,2502.6538,N,12121.4838,E,0.00,315.00,080905,,,A*6F";
  for(int i=0;i<96;i++) buff[i] = '\0';
  for(int i=0;i<NMXLENG;i++) {
    if ((str[nmeaWordIndex[i]+5] == 'C') && (str[nmeaWordIndex[i]+4] == 'M') && (str[nmeaWordIndex[i]+3] == 'R') && (nmeaCRIndex[i] != 255)) 
   {
     if (nmeaCRIndex[i]>nmeaWordIndex[i]) {
        buffleng = nmeaCRIndex[i]-nmeaWordIndex[i];
        if (buffleng > 96) buffleng = 96;
        strncpy(buff,str+nmeaWordIndex[i],buffleng);
        //HalLcdWriteString(buff, 4);
        HalLcdWriteString4(10,leng,buffleng,i,0,4);
        matchix = i;
        gotGPRMC = 1;
      }
    }
  }
  //Test use Sample String
  if (gotGPRMC == 0) {
    xtraByte = 0;
    return;
  }  
  ix = 0;
  for(int i=0;i<16;i++) nmeaCommaIndex[i] = 255;
  for(int i=0;i<buffleng;i++) {
    if (buff[i] == ',') {
      nmeaCommaIndex[ix] = i;
      ix++;
    }
  }
  commaCount = 0;
  for(int i=0;i<16;i++) {
    if (nmeaCommaIndex[i] != 255) commaCount++;
  }
  for(int i=0;i<12;i++) GPSTime[i] = 0;
  for(int i=0;i<12;i++) GPSLong[i] = 0;
  for(int i=0;i<12;i++) GPSLat[i] = 0;
  
  if ((buffleng > 16) && (commaCount == 12))  {
    if (gotGPRMC == 1) {
      subStringByComma(GPSTime,buff,nmeaCommaIndex[0],nmeaCommaIndex[1]);
      subStringByComma(GPSLong,buff,nmeaCommaIndex[2],nmeaCommaIndex[3]); 
      subStringByComma(GPSLat ,buff,nmeaCommaIndex[4],nmeaCommaIndex[5]);
    }
    //subStringByComma(GPSLat ,buff,nmeaCommaIndex[4],nmeaCommaIndex[5]); 
    //HalLcdWriteString4(10,nmeaCommaIndex[0],nmeaCommaIndex[1],nmeaCommaIndex[2],nmeaCommaIndex[3],7);
    HalLcdWriteString4(10,nmeaCommaIndex[4],nmeaCommaIndex[5],nmeaWordIndex[matchix],nmeaCRIndex[matchix],8);
    
    HalLcdWriteString(GPSTime, 5);
    HalLcdWriteString(GPSLong, 6);
    HalLcdWriteString(GPSLat , 7);  
  } else {
    //HalLcdWriteString("Wrong Format",7);  
  }
}
void indexNMEA(uint8 *buff,uint8 leng)
{
  uint8 *buffix;
  uint8 ixcr = 0; 
  uint8 ixwd = 0;
  uint8 substr[32];
  uint8 *org;
  int   subleng = 0;
  org = buff;
  uint8 gotGPRMC = 0;
  uint8 commaCount = 0;
  uint8 buffleng;
  uint8 ix;
  for(int i=0;i<NMXLENG;i++) {
    nmeaWordIndex[i] = 255;
    nmeaCRIndex[i] = 255;
  }  
  for(int i=0;i<leng;i++) {
    if (*buff == '$') {
      nmeaWordIndex[ixwd] = i;
      ixcr = ixwd;
      ixwd++;
    }
    if (*buff == 0x0D) {
      nmeaCRIndex[ixcr] = i;
      ixcr++;
    }   
    buff++;
  }
//  HalLcdWriteString4(10,nmeaWordIndex[0],nmeaCRIndex[0],nmeaWordIndex[1],nmeaCRIndex[1],8);
//  HalLcdWriteString4(10,nmeaWordIndex[2],nmeaCRIndex[2],nmeaWordIndex[3],nmeaCRIndex[3],8);
//  if ((nmeaCRIndex[0] > nmeaWordIndex[0]) && (nmeaCRIndex[0] != 255)){
//    showStrngInRange(org,nmeaWordIndex[0],nmeaCRIndex[0],4);
//  } else {
//    HalLcdWriteString("", 4);
//  }
//  if ((nmeaCRIndex[1] > nmeaWordIndex[1]) && (nmeaCRIndex[1] != 255)) {
//    showStrngInRange(org,nmeaWordIndex[1],nmeaCRIndex[1],5);
//  } else {
//    HalLcdWriteString("", 5);
//  }  
//  showStrngInRange(org,nmeaWordIndex[1],nmeaCRIndex[1],7);
//  showStrngInRange(org,nmeaWordIndex[2],nmeaCRIndex[2],8);  
}
