/*EmonLib32 - Library modified to work with ESP32

    Modified by: Dario Bocchino

esp_adc_cal_get_voltage will return voltage in mV, made of offset and ac voltage. 
Will not return a value between 0 and 4096 (12bit).

About ADC and precision:
Sensor accuracy is rated 1%
To reduce ADC error the device on startup will check for calibrated values (my device 
was not lucky), if they are not burned will use DEFAULT_VREF.
Then will calculate a characteristic curve to reduce Vref drift, and will  use 
function esp_adc_cal_raw_to_voltage to use it. 
Channel's attenuation was selected to stay between recommended voltages for adc
(150 to 2450mV for 11db).

Possible improvements:
Use adc2_vref_to_gpio() to obtain a better estimate

>>>WILL NOT WORK WITH ARDUINO

Original version:
  Emon.cpp - Library for openenergymonitor
  Created by Trystan Lea, April 27 2010
  GNU GPL
  modified to use up to 12 bits ADC resolution (ex. Arduino Due)
  by boredman@boredomprojects.net 26.12.2013
  Low Pass filter for offset removal replaces HP filter 1/1/2015 - RW
*/

// Proboscide99 10/08/2016 - Added ADMUX settings for ATmega1284 e 1284P (644 / 644P also, but not tested) in readVcc function

#include <stdio.h>
#include <stdlib.h>
#include "esp_system.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_adc_cal.h"

#include <driver/adc.h>
#include <math.h>

#define DEFAULT_VREF 1100

#define ADC_BITS    12              //resolution of ADC
#define ADC_COUNTS  (1<<ADC_BITS)   //4096 in a fashion way

#define inPinV ADC1_CHANNEL_6
#define inPinI ADC1_CHANNEL_7

#define VCAL 0.222
#define ICAL 0.03
#define PHASECAL 1

//function prototypes
void calcVI(unsigned int crossings, unsigned int timeout,double* offsetV, double* offsetI,esp_adc_cal_characteristics_t *adc_chars,double* realPower,double*apparentPower,double*powerFactor,double*Vrms,double*Irms);
void calcI(unsigned int samples,double* offsetI,esp_adc_cal_characteristics_t *adc_chars,double*Irms);