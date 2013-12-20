//hal_Si4703

#include "hal_i2c.h"
#include "hal_sensor.h"
#include "hal_lcd.h"

#define SI4703 0x10
#define SEEK_DOWN 0
#define SEEK_UP   1

#define SIDEVICEID   0x00
#define SICHIPID     0x01
#define POWERCFG     0x02
#define CHANNEL      0x03
#define SYSCONFIG1   0x04
#define SYSCONFIG2   0x05
#define STATUSRSSI   0x0A
#define READCHAN     0x0B
#define RDSA         0x0C
#define RDSB         0x0D
#define RDSC         0x0E
#define RDSD         0x0F

#define SMUTE      15
#define DMUTE      14
#define SKMODE     10
#define SEEKUP     9
#define SEEK       8

#define TUNE       15
#define RDS        12
#define DE         11

#define SPACE1     5
#define SPACE0     4

#define RDSR       15
#define STC        14
#define SFBL       13
#define AFCRL      12
#define RDSS       11
#define STEREO     8

#define DELAY(x)     {for(long int i=0;i<x;i++)  {asm("NOP");asm("NOP");asm("NOP");}}

void si4703_readRegisters(void);
void si4703_updateRegisters(void);
void si4703_setVolume(uint8 volume);
void si4703_setChannel(int channel);

uint8 si4703_store[32];
uint16 si4703_registers[16];
uint8 si4703_out[6];

void si4703_readRegisters(void)
{
  uint8 i=0;
  HalI2CInit(SI4703,i2cClock_123KHZ);
  HalSensorReadReg2(SI4703,si4703_store,32);
  for(int x = 0x0A ; ; x++) { //Read in these 32 bytes
    if(x == 0x10) x = 0; //Loop back to zero
    si4703_registers[x]  = si4703_store[i++] << 8;
    si4703_registers[x] |= si4703_store[i++];
    if(x == 0x09) break; //We're done!
  }
}

void si4703_updateRegisters(void)
{
  uint8 i=0;
  for(int regSpot = 0x02 ; regSpot < 0x08 ; regSpot++) {//6 Word, 12 Byte
    si4703_out[i++] = si4703_registers[regSpot] >> 8;
    si4703_out[i++] = si4703_registers[regSpot] & 0x00FF;
  } 
  HalI2CInit(SI4703,i2cClock_123KHZ);
  HalSensorWriteReg2(SI4703,si4703_out,12);
}

void si4703_init(void)
{
  P1SEL |= 0xE0;
  P1SEL &= ~0x0E; 
  P1 |= 0x0E; 
  P1_3 = 0; 
  P1DIR |= 0x0E; 
  DELAY(1000);
  P1_3 = 1;
  DELAY(1000);  
  HalI2CInit(SI4703,i2cClock_123KHZ);
  si4703_readRegisters();
  
  si4703_registers[0x07] = 0x8100; //Enable the oscillator, from AN230 page 9, rev 0.61 (works)
  si4703_updateRegisters(); //Update

  DELAY(500000); //Wait for clock to settle - from AN230 page 9

  si4703_readRegisters(); //Read the current register set
  si4703_registers[POWERCFG] = 0x4001; //Enable the IC
  //  si4703_registers[POWERCFG] |= (1<<SMUTE) | (1<<DMUTE); //Disable Mute, disable softmute
  si4703_registers[SYSCONFIG1] |= (1<<RDS); //Enable RDS

  si4703_registers[SYSCONFIG1] |= (1<<DE); //50kHz Europe setup
  si4703_registers[SYSCONFIG2] |= (1<<SPACE0); //100kHz channel spacing for Europe

  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= 0x0001; //Set volume to lowest
  si4703_updateRegisters(); //Update

  DELAY(110000); //Max powerup time, from datasheet page 13  
  si4703_setVolume(0);
  DELAY(5000);
  si4703_setChannel(917);  
  DELAY(5000);
  si4703_setVolume(3);
}

void si4703_setVolume(uint8 volume)
{
  si4703_readRegisters(); //Read the current register set
  if(volume < 0) volume = 0;
  if (volume > 15) volume = 15;
  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= volume; //Set new volume
  si4703_updateRegisters(); //Update
}

void si4703_setChannel(int mychannel)
{
  //Freq(MHz) = 0.200(in USA) * Channel + 87.5MHz
  //97.3 = 0.2 * Chan + 87.5
  //9.8 / 0.2 = 49
  int newChannel = mychannel * 10; //973 * 10 = 9730
  newChannel -= 8750; //9730 - 8750 = 980
  newChannel /= 10; //980 / 10 = 98

  //These steps come from AN230 page 20 rev 0.5
  si4703_readRegisters();
  si4703_registers[CHANNEL] &= 0xFE00; //Clear out the channel bits
  si4703_registers[CHANNEL] |= newChannel; //Mask in the new channel
  si4703_registers[CHANNEL] |= (1<<TUNE); //Set the TUNE bit to start
  si4703_updateRegisters();

  //delay(60); //Wait 60ms - you can use or skip this delay

  //Poll to see if STC is set
  while(1) {
    si4703_readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) != 0) break; //Tuning complete!
  }

  si4703_readRegisters();
  si4703_registers[CHANNEL] &= ~(1<<TUNE); //Clear the tune after a tune has completed
  si4703_updateRegisters();

  //Wait for the si4703 to clear the STC as well
  while(1) {
    si4703_readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) == 0) break; //Tuning complete!
  }
}
int si4703_getChannel() {
  si4703_readRegisters();
  int channel = si4703_registers[READCHAN] & 0x03FF; //Mask out everything but the lower 10 bits
  //Freq(MHz) = 0.100(in Europe) * Channel + 87.5MHz
  //X = 0.1 * Chan + 87.5
  channel += 875; //98 + 875 = 973
  return(channel);
}

int si4703_seek(uint8 seekDirection)
{
  si4703_readRegisters();
  //Set seek mode wrap bit
  si4703_registers[POWERCFG] |= (1<<SKMODE); //Allow wrap
  //si4703_registers[POWERCFG] &= ~(1<<SKMODE); //Disallow wrap - if you disallow wrap, you may want to tune to 87.5 first
  if(seekDirection == SEEK_DOWN) si4703_registers[POWERCFG] &= ~(1<<SEEKUP); //Seek down is the default upon reset
  else si4703_registers[POWERCFG] |= 1<<SEEKUP; //Set the bit to seek up

  si4703_registers[POWERCFG] |= (1<<SEEK); //Start seek
  si4703_updateRegisters(); //Seeking will now start

  //Poll to see if STC is set
  while(1) {
    si4703_readRegisters();
    if((si4703_registers[STATUSRSSI] & (1<<STC)) != 0) break; //Tuning complete!
  }

  si4703_readRegisters();
  int valueSFBL = si4703_registers[STATUSRSSI] & (1<<SFBL); //Store the value of SFBL
  si4703_registers[POWERCFG] &= ~(1<<SEEK); //Clear the seek bit after seek has completed
  si4703_updateRegisters();

  //Wait for the si4703 to clear the STC as well
  while(1) {
    si4703_readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) == 0) break; //Tuning complete!
  }

  if(valueSFBL) { //The bit was set indicating we hit a band limit or failed to find a station
    return(0);
  }
return si4703_getChannel();
}
