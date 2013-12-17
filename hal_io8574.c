//hal_io8574
#include "hal_i2c.h"
#include "hal_sensor.h"

#define PCF8574ADD 0x27

uint8 io8574Read(void)
{
  uint8 port;
  HalI2CInit(PCF8574ADD,i2cClock_123KHZ);
  HalSensorReadReg2(PCF8574ADD,&port,1);  
  return port;
}
void io8574Write(uint8 data)
{
   HalI2CInit(PCF8574ADD,i2cClock_123KHZ);
   HalSensorWriteReg2(PCF8574ADD,&data,1);      
}
