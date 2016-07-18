
////////////////////////////////////////////////////////////////////////////
//// flash.c                                                            ////
////////////////////////////////////////////////////////////////////////////


void forceFlashStale() {
  // Used for test purposes.  This will cause all Flash Memory routines to start fresh upon next boot.
  write_EEPROM(FLASH_MEMORY_FIRST, 0x00);
}


long isFlashStale() {
  if(0x5A != read_EEPROM(FLASH_MEMORY_FIRST) || 0x5A != read_EEPROM(FLASH_MEMORY_LAST)) {
    return(1);
  } else {
    return(0);
  }
}

void FlashLoadAllLocal() {

  long memPtr;
  long i;

  local_flash_Voltage_OnOff = read_EEPROM(FLASH_MEMORY_VOLTAGE_ONOFF);
  local_flash_VSWR_OnOff    = read_EEPROM(FLASH_MEMORY_VSWR_ONOFF);

  memPtr = FLASH_MEMORY_POWER_LOW_DB;  // Base Address, see flash.h and be very careful

  for(i = 0; i < 5; i++) {
    local_flash_POWER_DB[i]     = readFloatExtEEPROM(memPtr);  memPtr += 4;
    local_flash_POWER_LEVEL[i]  = readFloatExtEEPROM(memPtr);  memPtr += 4;
  }

}


void StoreDefaultsToFlash() {
  write_EEPROM(FLASH_MEMORY_FIRST,                        0x5A);
  write_EEPROM(FLASH_MEMORY_VOLTAGE_ONOFF,                0x00);  // Off by default
  write_EEPROM(FLASH_MEMORY_VSWR_ONOFF,                   0x01);  // On by default, note mutex with Voltage setting req'd
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_LOW_DB,        -70.01);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_MID_DB,          0.01);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_HI_DB,           7.01);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_DIFF_DB,         3.60);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_LOW_LEVEL,       0.65);  // Things get muddy below -70.....
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_MID_LEVEL,       3.50);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_HI_LEVEL,        3.79);  // Cal points based upon HP 8640B @ 100 MHz.
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_DIFF_LEVEL,      0.67);
  writeFloatExtEEPROM(FLASH_MEMORY_TAP_DB,               40.05);
  writeFloatExtEEPROM(FLASH_MEMORY_UNUSED_003,             0.0);
  write_EEPROM(FLASH_MEMORY_LAST,                         0x5A);
}


void writeFloatExtEEPROM(long base_location, float data) {
  signed int16 data16;
  unsigned int8 oneByte;
  unsigned int8 twoByte;
  
  data16 = (signed int16) (100 * data);
  oneByte = (0x00FF & data16);
  twoByte = ((0xFF00 & data16) >> 8);
  write_EEPROM(base_location, oneByte);
  write_EEPROM(base_location + 1, twoByte);

}


float readFloatExtEEPROM(long base_location) {
  float         data;
  signed int16  data16;
  unsigned int8 oneByte;
  unsigned int8 twoByte;
  
  oneByte = read_EEPROM(base_location);
  twoByte = read_EEPROM(base_location + 1);
  data16 = twoByte;
  data16 <<= 8;
  data16 |= oneByte;
  data = (float) data16;
  data /= 100.0;
  return(data);

}


