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

//includes
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

#include "EmonLib32.h"

static esp_adc_cal_characteristics_t *adc_chars; //pointer needed to characterize ADC

//Variables to share with all tasks
double realPower,apparentPower,powerFactor,Vrms,Irms;
double offsetV=1620;
double offsetI=1620;

//next two functions will check for calibration values burned into ESP32
static void check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void app_main ()
{

    //ADC Init

    check_efuse();        //Check if Two Point or Vref are burned into eFuse

    adc1_config_width(ADC_WIDTH_12Bit);                       //12 bit resolution (4096)
    adc1_config_channel_atten(inPinI,ADC_ATTEN_11db); //11db full range 3.9V or VDD, recommended range 150 to 2450mV
    adc1_config_channel_atten(inPinV,ADC_ATTEN_11db); //11db full range 3.9V or VDD, recommended range 150 to 2450mV

    //Characterize ADC                                      
    //Next three lines will create a curve to reduce VREF drift (see ESP32 documentation)
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);


    while(1){
        //call VI function
        calcVI(1480,2000,&offsetV,&offsetI,adc_chars,&realPower,&apparentPower,&powerFactor,&Vrms,&Irms); //samples,timeout

        //print VI function outputs
        printf("realPower: %.3f \t", realPower);
        printf("apparentPower: %.3f \t", apparentPower);
        printf("Vrms: %.3f \t", Vrms);
        printf("Irms: %.3f \t", Irms);
        printf("Vrms_pure: %f \t", Vrms/VCAL);
        printf("Irms_pure: %f \t", Irms/ICAL);
        printf("powerFactor: %.3f \n", powerFactor);

        vTaskDelay(1000 / portTICK_RATE_MS);    //wait 1 seconds.

        //call I function
        calcI(1480,&offsetI,adc_chars,&Irms);

        //print I funtion outputs
        printf(" Irms: %.3f \t", Irms);
        printf(" Irms_pure: %f \n", Irms/ICAL);
                
        vTaskDelay(1000 / portTICK_RATE_MS);    //wait 1 second ad repeat.
    }//end while(1)

}//end app_main