
#ifndef INCLUDED_DAC_H
#define INCLUDED_DAC_H

// Constants

#define DAC_SER_CLK  PIN_B0
#define DAC_SER_DATA PIN_B1
#define DAC_SER_LD   PIN_B2


// Function Prototypes

void output_dac(float voltage);
float output_dac_zero(float target_voltage);
void refresh_adc(long index);

#endif /* include */









