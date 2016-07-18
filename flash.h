
#ifndef INCLUDED_FLASH_H
#define INCLUDED_FLASH_H

// Constants

#define FLASH_MEMORY_FIRST            0
#define FLASH_MEMORY_VOLTAGE_ONOFF    1
#define FLASH_MEMORY_VSWR_ONOFF       2
// Some store floating point....be careful
// RDH 14 December 2003.  Remember to allocate 4 bytes here for each floating point value.
#define FLASH_MEMORY_POWER_LOW_DB     4
#define FLASH_MEMORY_POWER_LOW_LEVEL  8

#define FLASH_MEMORY_POWER_MID_DB     12
#define FLASH_MEMORY_POWER_MID_LEVEL  16

#define FLASH_MEMORY_POWER_HI_DB      20
#define FLASH_MEMORY_POWER_HI_LEVEL   24

#define FLASH_MEMORY_POWER_DIFF_DB    28
#define FLASH_MEMORY_POWER_DIFF_LEVEL 32

#define FLASH_MEMORY_TAP_DB           36
#define FLASH_MEMORY_UNUSED_003       40

#define FLASH_MEMORY_LAST             44

int   local_flash_VSWR_OnOff;
int   local_flash_Voltage_OnOff;

float local_flash_POWER_DB[5];
float local_flash_POWER_LEVEL[5];

// Index 0:  Lowest Point on Power Curve
// Index 1:  Mid    Point on Power Curve
// Index 2:  High   Point on Power Curve
// Index 3:  Differential Gain Calibration
// Index 4:  Tap Offset ("40.0 dB")

// Function Prototypes

void forceFlashStale();
long isFlashStale();
void FlashLoadAllLocal();
void StoreDefaultsToFlash();
void writeFloatExtEEPROM(long base_location, float data);
float readFloatExtEEPROM(long base_location);

#endif /* include */









