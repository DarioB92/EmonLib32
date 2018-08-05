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

#define VCAL 0.22
#define ICAL 0.03
#define PHASECAL 1

static esp_adc_cal_characteristics_t *adc_chars; //pointer needed to characterize ADC


//function prototypes
void calcVI(unsigned int crossings, unsigned int timeout,double* offsetV, double* offsetI,esp_adc_cal_characteristics_t *adc_chars,double* realPower,double*apparentPower,double*powerFactor,double*Vrms,double*Irms);