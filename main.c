#include <18F452.h>
#DEVICE ADC=10
#fuses HS,NOWDT,NOPROTECT,NOLVP   // May need to be changed depending on the chip
#use delay(clock=20000000)  // Change if you use a different clock
#use rs232(baud=9600, xmit=PIN_B4, rcv=PIN_B5)

// V.2.0.0 Production release, Compiled with 4.033


#include <math.h>
#include <string.h>
#include "rlcd.h"
#include "flash.h"
#include "dac.h"

#define BUTTON_STATE_A  0x02
#define BUTTON_STATE_B  0x04
#define BUTTON_STATE_C  0x08
#define BUTTON_STATE_F  0x01

#define BUTTON_SHORT    0x10
#define BUTTON_LONG     0x20

#define MODE_0          0x00
#define MODE_1          0x01
#define MODE_1H         0x11
#define MODE_2          0x02
#define MODE_3          0x03
#define MODE_4          0x04
#define MODE_VIEW       0x12
#define MODE_CAL        0x22
#define MODE_CALV       0x25

#define RUNSTATE_NORMAL     0x0000
#define RUNSTATE_TAP        0x0001
#define RUNSTATE_ZERO       0x0002
#define RUNSTATE_MINHOLD    0x0004
#define RUNSTATE_MAXHOLD    0x0008
#define RUNSTATE_CALIBRATE  0x0100

long  runState;
long  runStateWatts;
long  saveStateWatts;   // Remembers what runStateWatts was while we're in zero mode.
long  d_value;      // ADC last value read.
float d_adc_voltage[2];
float d_zeroLoss_voltage;
float d_absoluteDB;
float d_minDB;
float d_maxDB;
float d_zeroReferenceAbsoluteDB;
long  interactive;  // Tells display to refresh immediately.
long  d_mode;
long  d_buttons;
int8  d_buttonState;
long  d_buttonStateCount;
char  lcdLine[18];
char  floatString[8];
int8  d_calibrateIndex;
int8  d_flash_blank;
int8  d_flashing;
float serialAbsPower = -100.0;
float lastZeroDB = 0.0;
float lastVswr   = 1.0;
int8  hyperZero  = 0;

#include "rlcd.c"
#include "flash.c"
#include "dac.c"

float capDB(float in) {
  float hicap;
  float locap;

  if(4 == d_calibrateIndex) {
    hicap = 50.05;
    locap = 0.00;
  } else {
    hicap = 20.05;
    locap = -80.05;
  }
  if(in > hicap) {
    return(hicap);
  }
  if(in < locap) {
    return(locap);
  }
  return(in);
}

float capVoltage(float in) {
  if(in > 4.99) {
    return(4.99);
  }
  if(in < 0.00) {
    return(0.00);
  }
  return(in);
}


void setCal() {
  runState |= RUNSTATE_CALIBRATE;
}

void cancelCal() {
  runState &= ~RUNSTATE_CALIBRATE;
  FlashLoadAllLocal();
}

void clearCal() {
  runState &= ~RUNSTATE_CALIBRATE;
}

void saveCal() {
  write_EEPROM(FLASH_MEMORY_VSWR_ONOFF,               local_flash_VSWR_OnOff);
  write_EEPROM(FLASH_MEMORY_VOLTAGE_ONOFF,            local_flash_Voltage_OnOff);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_LOW_DB,      local_flash_POWER_DB[0]);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_LOW_LEVEL,   local_flash_POWER_LEVEL[0]);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_MID_DB,      local_flash_POWER_DB[1]);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_MID_LEVEL,   local_flash_POWER_LEVEL[1]);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_HI_DB,       local_flash_POWER_DB[2]);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_HI_LEVEL,    local_flash_POWER_LEVEL[2]);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_DIFF_DB,     local_flash_POWER_DB[3]);
  writeFloatExtEEPROM(FLASH_MEMORY_POWER_DIFF_LEVEL,  local_flash_POWER_LEVEL[3]);
  writeFloatExtEEPROM(FLASH_MEMORY_TAP_DB,            local_flash_POWER_DB[4]);
}

void incrementCal(float dither) {
  local_flash_POWER_DB[d_calibrateIndex] = capDB(local_flash_POWER_DB[d_calibrateIndex] + dither);
}

void incrementCalv(float dither) {
  local_flash_POWER_LEVEL[d_calibrateIndex] = capVoltage(local_flash_POWER_LEVEL[d_calibrateIndex] + dither);
}

void toggleVolts() {
  if(0 == local_flash_Voltage_OnOff) {
    local_flash_VSWR_OnOff    = 0x00;
    local_flash_Voltage_OnOff = 0x01;
  } else {
    local_flash_Voltage_OnOff = 0x00;
  }
}

void toggleVSWR() {
  if(0 == local_flash_VSWR_OnOff) {
    local_flash_Voltage_OnOff = 0x00;
    local_flash_VSWR_OnOff    = 0x01;
  } else {
    local_flash_VSWR_OnOff = 0x00;
  }
}


void toggleTap() {
  if(0 == (runState & RUNSTATE_TAP)) {
    runState |= RUNSTATE_TAP;
  } else {
    runState &= ~RUNSTATE_TAP;
  }
}

long isTap() {
  return(runState & RUNSTATE_TAP ? 1 : 0);
}

void setMinHold() {
      d_minDB = d_absoluteDB;
      runState |= RUNSTATE_MINHOLD;
}

void clearMinHold() {
      runState &= ~RUNSTATE_MINHOLD;
}

long isMinHold() {
   return(runState & RUNSTATE_MINHOLD ? 1 : 0);
}

void toggleMinHold() {
   if(isMinHold()) {
      clearMinHold();
   } else {
      setMinHold();
   }
}



void setMaxHold() {
      d_maxDB = d_absoluteDB;
      runState |= RUNSTATE_MAXHOLD;
}

void clearMaxHold() {
      runState &= ~RUNSTATE_MAXHOLD;
}

long isMaxHold() {
   return(runState & RUNSTATE_MAXHOLD ? 1 : 0);
}

void toggleMaxHold() {
   if(isMaxHold()) {
      clearMaxHold();
   } else {
      setMaxHold();
   }
}

void setZero() {
  runState |= RUNSTATE_ZERO;
  clearMinHold();
  clearMaxHold();
  lcd_gotoxy(1,2);
  lcd_putc('*');
  d_zeroReferenceAbsoluteDB = d_absoluteDB;
  d_zeroLoss_voltage = output_dac_zero(2.5);
}

void clearZero() {
  runState &= ~RUNSTATE_ZERO;
  runStateWatts = saveStateWatts;
}


void toggleZero() {
  if(0 == (runState & RUNSTATE_ZERO)) {
   saveStateWatts = runStateWatts;
    runStateWatts = 0;
    setZero();
  } else {
    clearZero();
  }
}


long isZero() {
  return(runState & RUNSTATE_ZERO ? 1 : 0);
}


void toggleWatts() {
  if(0 == runStateWatts) {
    clearZero();
    runStateWatts = 1;
   saveStateWatts = 1;
  } else {
    runStateWatts = 0;
   saveStateWatts = 0;
  }
}


void sayMode(long mode) {

  lcd_gotoxy(1,2);
  switch(mode) {
    case MODE_1:
    case MODE_1H:
      strcpy(lcdLine, LCD_STRING_MENU_TIER_1);
      if(isZero()) {
        lcdLine[0] = '>';
      }
      if(runStateWatts) {
        lcdLine[7] = '>';
      }
      if(isTap()) {
        lcdLine[12] = '>';
      }
      break;
    case MODE_2:
      if(IsMinHold() || IsMaxHold()) {
        strcpy(lcdLine, LCD_STRING_MENU_TIER_2_CLEAR);
        if(IsMinHold()) {
          lcdLine[0] = '>';
        }
        if(IsMaxHold()) {
          lcdLine[6] = '>';
        }
      } else {
         strcpy(lcdline, LCD_STRING_MENU_TIER_2_SET);
      }
      break;
    case MODE_3:
      strcpy(lcdLine, LCD_STRING_MENU_TIER_3);
      break;
    case MODE_4:
      strcpy(lcdline, LCD_STRING_MENU_TIER_4);
      if(0 != local_flash_VSWR_OnOff) {
        lcdLine[5] = '>';
      }
      if(0 != local_flash_Voltage_OnOff) {
        lcdLine[10] = '>';
      }
      break;
    case MODE_VIEW:
      strcpy(lcdLine, LCD_STRING_MENU_CALVIEW);
      if(4 == d_calibrateIndex) {
        lcdLine[10] = ' ';
        lcdLine[11] = ' ';
        lcdLine[12] = ' ';
        lcdLine[13] = ' ';
        lcdLine[14] = ' ';
        lcdLine[15] = ' ';
      }
      break;
    case MODE_CAL:
    case MODE_CALV:
      strcpy(lcdLine, LCD_STRING_MENU_CALIBRATE);
      break;
    case MODE_0:
    default:
      strcpy(lcdLine, LCD_STRING_BLANK);
      break;
  }
  lcd_put_string(lcdLine);

}



long buttonPushed(long oldMode, long buttonState, long shortLong) {
  long mode;
  mode = oldMode;

  // If any button was pushed, refresh the state.
  if(buttonState != 0) {
    interactive = 1;
  }

  if(BUTTON_SHORT == shortLong) {
    switch(oldMode) {
      case MODE_0:
        if(buttonState == BUTTON_STATE_F) {
          mode = MODE_1;
        }
        break;
      case MODE_1:
        if(buttonState == BUTTON_STATE_A) {
          toggleZero();
          mode = MODE_1H;
        }
        if(buttonState == BUTTON_STATE_B) {
          toggleWatts();
          mode = MODE_1H;
        }
        if(buttonState == BUTTON_STATE_C) {
          toggleTap();
          mode = MODE_1H;
        }
        if(buttonState == BUTTON_STATE_F) {
          mode = MODE_2;
        }
        break;
      case MODE_2:
        if(buttonState == BUTTON_STATE_A) {
         // min toggle
         toggleMinHold();  // xor with other modes built in to function
         mode = MODE_0;
        }
        if(buttonState == BUTTON_STATE_B) {
         // max toggle
         toggleMaxHold();
         mode = MODE_0;
        }
        if(buttonState == BUTTON_STATE_C) {
          // if either min or max are presently enabled, then this is a clear button.
          if(IsMinHold() || IsMaxHold()) {
            clearMinHold();
            clearMaxHold();
            mode = MODE_0;
          } else {
            setMinHold();
            setMaxHold();
            mode = MODE_0;
          }
        }
        if(buttonState == BUTTON_STATE_F) {
         mode = MODE_3;
        }
        if(BUTTON_STATE_A == buttonState || BUTTON_STATE_B == buttonState) {
          lcd_gotoxy(1,2);
          if(IsMinHold()) {
            lcd_putc('>');
          } else {
            lcd_putc(' ');
          }
          lcd_gotoxy(7,2);
          if(IsMaxHold()) {
            lcd_putc('>');
          } else {
            lcd_putc(' ');
          }
          delay_ms(200);
          saveCal();
          mode = MODE_0;
        }


        break;
      case MODE_3:
        if(buttonState == BUTTON_STATE_A) {
          d_calibrateIndex = 0;
          mode = MODE_VIEW;
        }
        if(buttonState == BUTTON_STATE_B) {
          d_calibrateIndex = 1;
          mode = MODE_VIEW;
        }
        if(buttonState == BUTTON_STATE_C) {
          d_calibrateIndex = 2;
          mode = MODE_VIEW;
        }
        if(buttonState == BUTTON_STATE_F) {
          mode = MODE_4;
        }
        break;
      case MODE_4:
        if(buttonState == BUTTON_STATE_A) {
          d_calibrateIndex = 3;
          mode = MODE_VIEW;
        }
        if(BUTTON_STATE_B == buttonState) {
          toggleVSWR();
        }
        if(BUTTON_STATE_C == buttonState) {
          toggleVolts();
        }
        if(BUTTON_STATE_B == buttonState || BUTTON_STATE_C == buttonState) {
          lcd_gotoxy(6,2);
          if(0 != local_flash_VSWR_OnOff) {
            lcd_putc('>');
          } else {
            lcd_putc(' ');
          }
          lcd_gotoxy(11,2);
          if(0 != local_flash_Voltage_OnOff) {
            lcd_putc('>');
          } else {
            lcd_putc(' ');
          }
          delay_ms(200);
          saveCal();
          mode = MODE_0;
        }
        if(buttonState == BUTTON_STATE_F) {
          mode = MODE_0;
        }
        break;
      case MODE_VIEW:
        if(BUTTON_STATE_A == buttonState || BUTTON_STATE_F == buttonState) {
          mode = MODE_0;
        }
        if(buttonState == BUTTON_STATE_B) {
          setCal();
          mode = MODE_CAL;
        }
        if(buttonState == BUTTON_STATE_C) {
          if(4 == d_calibrateIndex) {  // Tapcal is only dB...
            mode = MODE_0;
          } else {
            setCal();
            mode = MODE_CALV;
          }
        }
        break;
      case MODE_CAL:
      case MODE_CALV:
        if(buttonState == BUTTON_STATE_A) {
          lcd_gotoxy(1,2);
          lcd_putc('*');
          delay_ms(200);
          SaveCal();
          ClearCal();
          cancelCal();
          mode = MODE_0;
        }
        if(buttonState == BUTTON_STATE_B && MODE_CAL == mode) {
          incrementCal(-0.1);
        }
        if(buttonState == BUTTON_STATE_B && MODE_CALV == mode) {
          incrementCalv(-0.01);
        }
        if(buttonState == BUTTON_STATE_C && MODE_CAL == mode) {
          incrementCal(0.1);
        }
        if(buttonSTate == BUTTON_STATE_C && MODE_CALV == mode) {
          incrementCalv(0.01);
        }
        if(buttonState == BUTTON_STATE_F) {
          CancelCal();
          mode = MODE_0;
        }
        break;

      // If the guy lifted his finger up.....
      case MODE_1H:
        if(0 == buttonState) {
          mode = MODE_0;
        }
        break;
      default:
        break;
    }
  }
  if(BUTTON_LONG == shortLong) {
    switch(oldMode) {
      case MODE_1H:
        if(buttonState == BUTTON_STATE_C) {
          d_calibrateIndex = 4;  // Tap Calibration....
          mode = MODE_VIEW;
        }
        break;
      case MODE_CAL:
      case MODE_CALV:
        if(buttonState == BUTTON_STATE_B && MODE_CAL == mode) {
          incrementCal(-1.0);
        }
        if(buttonState == BUTTON_STATE_B && MODE_CALV == mode) {
          incrementCalv(-0.1);
        }
        if(buttonState == BUTTON_STATE_C && MODE_CAL == mode) {
          incrementCal(1.0);
        }
        if(buttonSTate == BUTTON_STATE_C && MODE_CALV == mode) {
          incrementCalv(0.1);
        }
        break;
      default:
        break;
    }
  }

  return(mode);

}






long FastTickUpdate(long i) {

  int8 buttons1;
  int8 buttons2;
  long newMode;

  /* Fast Tick Update Here */
  buttons1 = (input_c() >> 4);
  if(buttons1 != d_buttons) {
    delay_ms(50);
    buttons2 = (input_c() >> 4);
    if(buttons1 != buttons2) {
      // After waiting n mS above, the buttons are not consistent.  Bounce.  Toss.
      // April 3 2003.  This helped IMMENSELY.  Can probably toss the bounce caps on the button inputs now.
      return(0);
    }
  }

  if(buttons1 != d_buttons) {
    d_buttons = buttons1;
    d_buttonStateCount = 0;
    d_buttonState = 0;
    if(0 == (d_buttons & BUTTON_STATE_A)) {
      d_buttonState |= BUTTON_STATE_A;
    }
    if(0 == (d_buttons & BUTTON_STATE_B)) {
      d_buttonState |= BUTTON_STATE_B;
    }
    if(0 == (d_buttons & BUTTON_STATE_C)) {
      d_buttonState |= BUTTON_STATE_C;
    }
    if(0 == (d_buttons & BUTTON_STATE_F)) {
      d_buttonState |= BUTTON_STATE_F;
    }
  }
  if(d_buttonStateCount <= 25) {
    d_buttonStateCount++;
  }

  if(2 == d_buttonStateCount) {
    newMode = buttonPushed(d_mode, d_buttonState, BUTTON_SHORT);
    if(newMode != d_mode) {
      d_mode = newMode;
      interactive = 1;
      sayMode(d_mode);
    }
    if(interactive) {
      break;
    }
  }
  if(25 == d_buttonStateCount) {
    newMode = buttonPushed(d_mode, d_buttonState, BUTTON_LONG);
    if(newMode != d_mode) {
      d_mode = newMode;
      interactive = 1;
      sayMode(d_mode);
    } else {
      d_buttonStateCount = 5;  // Allows you to hold down the button for long durations...
    }
    if(interactive) {
      break;
    }
  }

  if(i % 2) {
    output_low(PIN_D3);
    output_high(PIN_D2);
  } else {
    //putc('5');
    output_high(PIN_D3);
    output_low(PIN_D2);
  }

  return(interactive);

}



void SlowTickUpdate(long mode) {

  int8  flag;
  long  value;
  float deebee;
  float mw;
  int8  refresh;
  int8  barOn;
  int8  position;
  float adc_voltage1;
  char  extra;
  float g;
  float vswr;
  float  x1, y1, x2, y2;

  refresh = 0;

  set_adc_channel(0);
  delay_us(20);
  value = read_adc();

  // Make sure the display will update if we are in hyper-sensitive mode.
  if(isZero() && MODE_0 == mode) {
    adc_voltage1 = d_adc_voltage[1];
    refresh_adc(1);
    if(d_adc_voltage[1] != adc_voltage1) {
      refresh = 1;
    }
  }

  // This will cause the display to update when we aren't in hyper-sensitive mode.
  // (Let both of these checks happen no-matter what.  Non-destructive).
  // If you are in a flashing mode, it is important that you refresh continuously until you are not in a flashing mode.
  if(d_value != value || 1 == interactive || d_flashing) {
    refresh = 1;
  }

  if(refresh) {
    d_value = value;
    interactive = 0;
    refresh_adc(0);
    if(d_adc_voltage[0] <= local_flash_POWER_LEVEL[1]) {
      // Use Low to Mid slope
      x1 = local_flash_POWER_LEVEL[0];
      x2 = local_flash_POWER_LEVEL[1];
      y1 = local_flash_POWER_DB[0];
      y2 = local_flash_POWER_DB[1];
    } else {
      // Use Mid to High Slope
      x1 = local_flash_POWER_LEVEL[1];
      x2 = local_flash_POWER_LEVEL[2];
      y1 = local_flash_POWER_DB[1];
      y2 = local_flash_POWER_DB[2];
    }

    deebee = (y2 - y1) * ((d_adc_voltage[0] - x1) / (x2 - x1)) + y1;
    // Note that the flash mode is based upon the power applied to the LogAmp, NOT the overall power.
    // We are trying to protect the meter here.
    if(deebee > local_flash_POWER_DB[2]) {
      d_flashing = 1; // Flash the dBm portion of the display
    } else {
      d_flashing = 0;
    }
    // Determine the number of meter-bars to show, now.
    // It is important to do this before the tap offset is applied.

    barOn = 0;
    if(deebee >= -70.0) {         barOn =  1;    }
    if(deebee >= -60.0) {         barOn =  2;    }
    if(deebee >= -50.0) {         barOn =  3;    }
    if(deebee >= -45.0) {         barOn =  4;    }
    if(deebee >= -40.0) {         barOn =  5;    }
    if(deebee >= -35.0) {         barOn =  6;    }
    if(deebee >= -30.0) {         barOn =  7;    }
    if(deebee >= -25.0) {         barOn =  8;    }
    if(deebee >= -20.0) {         barOn =  9;    }
    if(deebee >= -15.0) {         barOn = 10;    }
    if(deebee >= -10.0) {         barOn = 11;    }
    if(deebee >=  -6.0) {         barOn = 12;    }
    if(deebee >=  -3.0) {         barOn = 13;    }
    if(deebee >=   0.0) {         barOn = 14;    }
    if(deebee >=   3.0) {         barOn = 15;    }
    if(deebee >=   6.0) {         barOn = 16;    }
    // (done).

    // now apply the tap offset to the absolute power reading.
    if(isTap()) {
      deeBee += local_flash_POWER_DB[4]; // TAPCAL
    }

    d_absoluteDB = deeBee;

    d_maxDB = (d_absoluteDB > d_maxDB) ? d_absoluteDB : d_maxDB;
    d_minDB = (d_absoluteDB < d_minDB) ? d_absoluteDB : d_minDB;

    if(mode == MODE_CAL) {
      // this magic is OK, but it is only valid while you are in ZERO mode.  Range-Checking happening as well...
      if(3 == d_calibrateIndex &&
        isZero() &&
        d_adc_voltage[1] < 4.8 && d_adc_voltage[1] > 2.50) {
        // Show the difference for the differential measurement.
        local_flash_POWER_LEVEL[d_calibrateIndex] = d_adc_voltage[1] - 2.5;
      } else {
        local_flash_POWER_LEVEL[d_calibrateIndex] = d_adc_voltage[0];
      }
    }

    // TOP LINE
    flag = 0;
    lcd_gotoxy(1,1);
    if(mode == MODE_CAL || mode == MODE_CALV || mode == MODE_VIEW) {
      x1 = local_flash_POWER_LEVEL[d_calibrateIndex];
      y1 = local_flash_POWER_DB[d_calibrateIndex];

      if(4 == d_calibrateIndex) { // Tapcal
         // Some sort of stack-related sprintf floating issue.  Just format
         // the two lines separately and concatenate.  Not sure when this
         // first occurred, or whether an update to the compiler would fix it.
        sprintf(floatString, "%04.1f", fabs(y1));
        sprintf(lcdLine, "tapOffset     dB");
        lcdLine[10] = floatString[0];
        lcdLine[11] = floatString[1];
        lcdLine[12] = floatString[2];
        lcdLine[13] = floatString[3];
      } else {
        sprintf(lcdLine, "%c: %04.1fdBm %04.2fV", d_calibrateIndex + 'A', fabs(y1), x1);
        if(3 == d_calibrateIndex) {
          lcdLine[9] = ' ';  // Don't say dBm if it is a relative measurement...
        }
        if(y1 >= 0.0) {
          lcdLine[2] = '+';
        } else {
          lcdLine[2] = '-';
        }
      }
    } else {
      serialAbsPower = deebee;
      sprintf(lcdLine, "  %04.1f dBm %04.2fV ", abs(deebee), d_adc_voltage[0]);
      if(deebee < 0.0) {
        lcdLine[1] = '-';
      } else {
        lcdLine[1] = '+';
      }
      if(isTap()) {
        lcdLine[0] = 'T';
      }
      if(0 == local_flash_Voltage_OnOff) {
        lcdLine[11] = ' ';
        lcdLine[12] = ' ';
        lcdLine[13] = ' ';
        lcdLine[14] = ' ';
        lcdLine[15] = ' ';
      }
      // flashing is the mode to say "yes, I'm over load, and need to flash"
      // flash_blank is the state from last time.
      if(0 == d_flash_blank && d_flashing) {
        d_flash_blank = 1;
        lcdLine[1]  = ' ';
        lcdLine[2]  = ' ';
        lcdLine[3]  = ' ';
        lcdLine[4]  = ' ';
        lcdLine[5]  = ' ';
      } else {
        d_flash_blank = 0;
      }
    }

    lcd_put_string(lcdLine);
    if(isZero() && MODE_0 == mode) {
      refresh_adc(1);
      // Here is where you compute the offset dB value.
      if(d_adc_voltage[1] < 4.8 && d_adc_voltage[1] > 0.2) {
        // Okay, if I am using the 1.0 voltage, I have to subtract 1.0 and look for a positive number.
        // If I am using the 4.0 voltage, do the opposite.  Hmmm, shouldn't I remember which one I'm doing?

        //deebee = (2.5 - d_adc_voltage[1]);
        // The dB offset is based upon how far away from "zero" we are.
        // The d_zeroLoss_voltage is the value of d_adc_voltage[1]
        // at the precise moment when we set up the experiment.
        deebee = (d_zeroLoss_voltage - d_adc_voltage[1]);

        // Apply the slope gain before displaying.
        //        ( 10.0 dB )              / ( 2.0 Volts )
        deebee *= (local_flash_POWER_DB[3] / local_flash_POWER_LEVEL[3]);

        lastZeroDB = deebee;
        hyperZero = 1;
        sprintf(lcdLine, "  %05.2fdB  %04.2fV", abs(deebee), d_adc_voltage[1]);
      } else {
        // Not-so Hyper Mode.  Just diff the absolute values Instead.
        deebee = d_absoluteDB - d_zeroReferenceAbsoluteDB;
        lastZeroDB = deebee;
        hyperZero = 0;
        // Note the change in resolution on the display (one less digit).
        sprintf(lcdLine, "  %04.1f dB  %04.2fV", abs(deebee), d_adc_voltage[1]);
      }
      if(0 == local_flash_Voltage_OnOff) {
        if(1 == local_flash_VSWR_OnOff && deebee < -0.35) {
          g = pow(10.0, deebee / 20.0);
          vswr = (1.0 + g) / (1.0 - g);
          lastVswr = vswr;
          if(vswr > 9.99) {
            sprintf(&lcdLine[9], "%5.1f:1", vswr);
          } else {
            sprintf(&lcdLine[9], "%5.2f:1", vswr);
          }
        } else {
          lcdLine[11] = ' ';
          lcdLine[12] = ' ';
          lcdLine[13] = ' ';
          lcdLine[14] = ' ';
          lcdLine[15] = ' ';
        }
      }

      if(deebee < 0.0) {
        lcdLine[1] = '-';
      } else {
        lcdLine[1] = '+';
      }
      lcd_gotoxy(1,2);
      lcd_put_string(lcdLine);
    } else if((MODE_0 == mode) && (isMinHold() || isMaxHold())) {
      if(isMinHold() && isMaxHold()) {
        sprintf(lcdLine, "  %04.1f <->  %04.1f", abs(d_minDB), abs(d_maxDB));
        if(d_minDB < 0.0) {    lcdLine[1] = '-';    } else {    lcdLine[1] = '+';  }
        if(d_maxDB < 0.0) {    lcdLine[11] = '-';    } else {   lcdLine[11] = '+'; }
      } else if(isMinHold()) {
        sprintf(lcdLine, "  %04.1f dBm (min)", abs(d_minDB));
        if(d_minDB < 0.0) {    lcdLine[1] = '-';    } else {    lcdLine[1] = '+';  }
      } else { // isMaxHold (only) implicit
        sprintf(lcdLine, "  %04.1f dBm (max)", abs(d_maxDB));
        if(d_maxDB < 0.0) {    lcdLine[1] = '-';    } else {    lcdLine[1] = '+';  }
      }

      lcd_gotoxy(1,2);
      lcd_put_string(lcdLine);
    } else if(runStateWatts && MODE_0 == mode) {
      // deebee is in dBm at this point.
      deebee /= 10.0;
      mw = pow(10.0, deebee) / 1000.0;  // Now in Watts.
                                     extra = ' ';
      if(mw < 1.0) {  mw *= 1000.0;  extra = 'm';  }
      if(mw < 1.0) {  mw *= 1000.0;  extra = 'u';  }
      if(mw < 1.0) {  mw *= 1000.0;  extra = 'n';  }
      if(mw < 1.0) {  mw *= 1000.0;  extra = 'p';  }

      sprintf(lcdLine, " %6.2f  W", mw);
      lcdLine[8] = extra;

      // These lines shut off just to free up some ROM space for now...
      lcdLine[16] = '\0';
      lcd_gotoxy(1,2);
      lcd_put_string(lcdLine);
    } else if(MODE_0 == mode) {
      // Power Bar Mode.  First, clear the line.
      // Then, turn positions in to black squares, as per the number of barOn values determined from above.
      sprintf(lcdLine,"                ");
      lcd_gotoxy(1,2);

      for(position = 0; position < barOn; position++) {
         lcdLine[position] = 0xFF;
      }
      lcd_put_string(lcdLine);
    }
  }
  sprintf(lcdLine, "-P-:         dBm");
  sprintf(floatString, " %04.1f",abs(serialAbsPower));
  if(serialAbsPower < 0.0) {
    floatString[0] = '-';
  } else {
    floatString[0] = '+';
  }
  lcdLine[7] = floatString[0];
  lcdLine[8] = floatString[1];
  lcdLine[9] = floatString[2];
  lcdLine[10] = floatString[3];
  lcdLine[11] = floatString[4];
  if(isTap()) {
    lcdLine[5] = 'T';
  }
  printf(lcdLine);

  if(runStateWatts) {
    mw = pow(10.0, (serialAbsPower / 10.0)) / 1000.0;  // Now in Watts.
                                   extra = ' ';
    if(mw < 1.0) {  mw *= 1000.0;  extra = 'm';  }
    if(mw < 1.0) {  mw *= 1000.0;  extra = 'u';  }
    if(mw < 1.0) {  mw *= 1000.0;  extra = 'n';  }
    if(mw < 1.0) {  mw *= 1000.0;  extra = 'p';  }

    sprintf(lcdLine, " %6.2f  W", mw);
    lcdLine[8] = extra;
    printf(lcdLine);
  }

  if(isZero()) {
    printf(" Delta: ");
    if(1 == hyperZero) {
      sprintf(floatString, " %05.2f",abs(lastZeroDB));
    } else {
      sprintf(floatString, " %04.1f",abs(lastZeroDB));
    }

    if(lastZeroDB < 0.0) {
      floatString[0] = '-';
    } else {
      floatString[0] = '+';
    }
    printf(floatString);
    printf(" dB");
  }

  // more here...VSWR here....
  if(isZero() && 1 == local_flash_VSWR_OnOff && lastZeroDB < -0.35) {
    printf(" VSWR: ");
    if(lastVswr > 9.99) {
      sprintf(floatString, "%5.1f:1", lastVswr);
    } else {
      sprintf(floatString, "%5.2f:1", lastVswr);
    }
    printf(floatString);
  }

  if(isMinHold()) {
    printf(" min: ");
    sprintf(lcdLine, "  %04.1f dBm", abs(d_minDB));
    if(d_minDB < 0.0) {    lcdLine[1] = '-';    } else {    lcdLine[1] = '+';  }
    printf(lcdLine);
  }
  if(isMaxHold()) {
    printf(" max: ");
    sprintf(lcdLine, "  %04.1f dBm", abs(d_maxDB));
    if(d_maxDB < 0.0) {    lcdLine[1] = '-';    } else {    lcdLine[1] = '+';  }
    printf(lcdLine);
  }

  printf("\n\r");

}



void main() {

  int8   i;
  int8   limit;

  interactive = 0;

  runState        = RUNSTATE_NORMAL;  // Tells the program what mode it is running in (not a flash parameter)
  runStateWatts   = 0;
  saveStateWatts  = 0;

  set_tris_b(0xC0); // Serial Control to hardware, shared w/ICD as well
  set_tris_c(0x0F);
  d_buttons       = 0x00;
  d_value         = 0;
  d_buttonState   = 0;
  d_flashing      = 0;  // The thing that says you are, or were, over power (therfore you must flash the display)
  d_flash_blank   = 0;  // Given the above line, this is the thing that tells me whether I was a flash or not last time.

  setup_adc_ports(ALL_ANALOG);
  setup_adc(ADC_CLOCK_INTERNAL);

  lcd_init();
  delay_ms(100);

  d_buttons = (input_c() >> 4);
  delay_ms(250);
  d_buttons = (input_c() >> 4);

  lcd_gotoxy(1,1);

#ifdef UNUSED_CURRENTLY
  if(0 == (d_buttons & BUTTON_STATE_A)) {
    // Do "Button A Pressed on Powerup" here
    // sprintf(lcdLine, "button A");
    // lcd_put_string(lcdLine);
    // delay_ms(1000);
  }
  if(0 == (d_buttons & BUTTON_STATE_B)) {
    // Do "Button B Pressed on Powerup" here
    // sprintf(lcdLine, "button B");
    // lcd_put_string(lcdLine);
    // delay_ms(1000);
  }
#endif

  if((0 == (d_buttons & BUTTON_STATE_C)) &&
     (0 == (d_buttons & BUTTON_STATE_B))) {
    // Do "Button C Pressed on Powerup" here
    forceFlashStale();
    // sprintf(lcdLine, "button C");
    // lcd_put_string(lcdLine);
    // delay_ms(1000);
  }

#ifdef UNUSED_CURRENTLY
  if(0 == (d_buttons & BUTTON_STATE_F)) {
    // Do "Button F Pressed on Powerup" here
    // sprintf(lcdLine, "button F");
    // lcd_put_string(lcdLine);
    // delay_ms(1000);
  }
#endif

  lcd_gotoxy(1,2);
  strcpy(lcdLine, LCD_STRING_MEMORY);
  lcd_put_string(lcdLine);
  printf("\n\r");

  if(isFlashStale()) {
    lcd_gotoxy(1,1);
    strcpy(lcdLine, LCD_STRING_RESETTING_FLASH);
    lcd_put_string(lcdLine);
    printf("-I-:  Resetting Flash Memory\n\r");
    StoreDefaultsToFlash();
    delay_ms(500);
  }

  lcd_gotoxy(1,1);
  strcpy(lcdLine, LCD_STRING_RETRIEVING_FLASH);
  lcd_put_string(lcdLine);
  printf("-I-:  Retrieving Flash Memory Settings\n\r");
  FlashLoadAllLocal();
  delay_ms(300);

  lcd_gotoxy(1,1);
  strcpy(lcdLine, LCD_STRING_PIC_POWER_METER);
  lcd_put_string(lcdLine);
  printf("-I-:  %s\n\r",LCD_STRING_PIC_POWER_METER);
  lcd_gotoxy(1,2);
  strcpy(lcdLine, LCD_STRING_CALLSIGN);
  lcd_put_string(lcdLine);
  printf("-I-:  %s\n\r",LCD_STRING_CALLSIGN);
  delay_ms(2000);

  lcd_gotoxy(1,1);
  strcpy(lcdLine, LCD_STRING_BLANK);
  lcd_put_string(lcdLine);
  lcd_gotoxy(1,2);
  lcd_put_string(lcdLine);

  printf("-I-:  Calibration Settings:\n\r");
  printf("-I-:    Voltage Display:      ");
  if(0 != local_flash_Voltage_OnOff) {
    printf("ON\n\r");
  } else {
    printf("OFF\n\r");
  }
  printf("-I-:    VSWR Display:         ");
  if(0 != local_flash_VSWR_OnOff) {
    printf("ON\n\r");
  } else {
    printf("OFF\n\r");
  }
  printf("-I-:    Power Curve Point A:  ");
  sprintf(floatString, "%04.1f", local_flash_POWER_DB[0]);
  if(local_flash_POWER_DB[0] >= 0.0) {    floatString[0] = '+';  }
  printf(floatString);
  printf(" dBm :: ");
  sprintf(floatString, " %04.2f", local_flash_POWER_LEVEL[0]);
  printf(floatString);
  printf(" V\n\r");

  printf("-I-:    Power Curve Point B:  ");
  sprintf(floatString, "%04.1f", local_flash_POWER_DB[1]);
  if(local_flash_POWER_DB[1] >= 0.0) {    floatString[0] = '+';  }
  printf(floatString);
  printf(" dBm :: ");
  sprintf(floatString, " %04.2f", local_flash_POWER_LEVEL[1]);
  printf(floatString);
  printf(" V\n\r");

  printf("-I-:    Power Curve Point C:  ");
  sprintf(floatString, "%04.1f", local_flash_POWER_DB[2]);
  if(local_flash_POWER_DB[2] >= 0.0) {    floatString[0] = '+';  }
  printf(floatString);
  printf(" dBm :: ");
  sprintf(floatString, " %04.2f", local_flash_POWER_LEVEL[2]);
  printf(floatString);
  printf(" V\n\r");

  printf("-I-:    Differential Mode Calibration:  ");
  sprintf(floatString, "%04.1f", local_flash_POWER_DB[3]);
  if(local_flash_POWER_DB[3] >= 0.0) {    floatString[0] = '+';  }
  printf(floatString);
  printf(" dB :: ");
  sprintf(floatString, " %04.2f", local_flash_POWER_LEVEL[3]);
  printf(floatString);
  printf(" V\n\r");

  printf("-I-:    Tap Offset:  ");
  sprintf(floatString, " %04.1f", local_flash_POWER_DB[4]);
  floatString[0] = '+';
  printf(floatString);
  printf(" dB\n\r");

  /* Import Flash State, set State here */
  d_mode = MODE_0;
  sayMode(d_mode);

  while(TRUE) {

    if(d_flashing) {
      limit = 5;
    } else {
      limit = 20;
    }
    for(i = 0; i < limit; i++) {
      if(1 == FastTickUpdate(i)) {
        break;
      }
      /* End Fast Tick Update */
      delay_ms(25);
    }

    /* Slow Tick Update Here */

    SlowTickUpdate(d_mode);

  }

}


