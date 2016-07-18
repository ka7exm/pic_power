
////////////////////////////////////////////////////////////////////////////
//// dac.c                                                              ////
////////////////////////////////////////////////////////////////////////////

void output_dac(float voltage) {

  long  i;

  int16 hexValue;
  float remedy;

  remedy = voltage;
  remedy *= 1024;
  remedy /= 5.0;

  hexValue = (int16) (remedy);
  // Anything above full scale should get full scale....
  if(hexValue > 0x3FF) {
    hexValue = 0x3FF;
  }

  output_high(DAC_SER_CLK);
  output_high(DAC_SER_LD);

  for(i = 1; i <= 10; i++) {
    output_low(DAC_SER_CLK);
    if(1 == bit_test(hexValue, (10 - i))) {
      output_high(DAC_SER_DATA);
    } else {
      output_low(DAC_SER_DATA);
    }
    output_high(DAC_SER_CLK);  
  }
  output_low(DAC_SER_CLK);
  output_low(DAC_SER_LD);

}


// Okay, so this doesn't have much to do with the DAC.
// It has more to do with the application.

// What we're trying to do here is set the DAC such that
// the differential mode ADC value will read 2.5 volts.
// This sets up the hyper-precise measurement mode.
// Note that the return value will be the initial value
// of the ADC.  This allows us to remember, PRECISELY, where
// we were when we started things.

// Note that it isn't super-important that we get to exactly
// 2.5 volts.  Something close is fine.  The remaining slop is
// subtracted out by remembering where we are when we start
// (hence the need to return adc[1] from this function).

float output_dac_zero(float target_voltage) {
  float dacVoltage;
  float dither = 0.0;
  float value;
  long  i;

  for(i = 0; i < 50; i++) {
    dacVoltage = d_adc_voltage[0] + (target_voltage / 4.7) + dither;
    output_dac(dacVoltage);
    delay_ms(50);
    refresh_adc(1);

    if((d_adc_voltage[1] - target_voltage) > 0.002) {
      dither -= 0.001;
    } else if((d_adc_voltage[1] - target_voltage) < -0.002) {
      dither += 0.001;
    } else {
      return(d_adc_voltage[1]);
    }
  }
  return(d_adc_voltage[1]);

}



void refresh_adc(long index) {
  long value;

  set_adc_channel(index);
  delay_us(20);
  value = read_adc();
  d_adc_voltage[index] = (float) value;
  d_adc_voltage[index] *= 0.004882813;  // Convert to Volts:  5V reference / 2^10
}

