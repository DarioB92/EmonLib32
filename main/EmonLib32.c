#include "EmonLib32.h"

//--------------------------------------------------------------------------------------
// emon_calc procedure
// Calculates realPower,apparentPower,powerFactor,Vrms,Irms,kWh increment
// From a sample window of the mains AC voltage and current.
// The Sample window length is defined by the number of half wavelengths or crossings we choose to measure.
//--------------------------------------------------------------------------------------
void calcVI(unsigned int crossings, unsigned int timeout,double* offsetV, double* offsetI,esp_adc_cal_characteristics_t *adc_chars,double* realPower,double*apparentPower,double*powerFactor,double*Vrms,double*Irms)
{
  unsigned int crossCount = 0;                             //Used to measure number of times threshold is crossed.
  unsigned int numberOfSamples = 0;                        //This is now incremented
  unsigned int startV=0;
  unsigned int sampleV;
  unsigned int sampleI;
  double lastFilteredV;
  double filteredV=0;
  double filteredI;
  double sqV;
  double sumV=0;
  double sqI;
  double sumI=0;
  double instP;
  double sumP=0;
  double phaseShiftedV;
  bool lastVCross, checkVCross=false;

  //-------------------------------------------------------------------------------------------------------------------------
  // 1) Waits for the waveform to be close to 'zero' (mid-scale adc) part in sin curve.
  //-------------------------------------------------------------------------------------------------------------------------
  bool st=false;                                  //an indicator to exit the while loop

  unsigned int start = esp_log_timestamp();    //esp_log_timestamp()-start makes sure it doesnt get stuck in the loop if there is an error. 

  while(st==false)                                   //the while loop...
  {
    //startV = esp_adc_cal_raw_to_voltage(adc1_get_raw(inPinV),adc_chars);
    esp_adc_cal_get_voltage(inPinV,adc_chars,&startV);
    if ((startV < (ADC_COUNTS*0.55)) && (startV > (ADC_COUNTS*0.45))) st=true;  //check its within range
    if ((esp_log_timestamp()-start)>timeout) st = true;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 2) Main measurement loop
  //-------------------------------------------------------------------------------------------------------------------------
  start = esp_log_timestamp();                                                                           

  while ((crossCount < crossings) && (esp_log_timestamp()-start)<timeout)
  {
    numberOfSamples++;                       //Count number of times looped.
    lastFilteredV = filteredV;               //Used for delay/phase compensation

    //-----------------------------------------------------------------------------
    // A) Read in raw voltage and current samples
    //-----------------------------------------------------------------------------
    
    esp_adc_cal_get_voltage(inPinV,adc_chars,&sampleV);  //Read in voltage signal in mV
    esp_adc_cal_get_voltage(inPinI,adc_chars,&sampleI);  //Read in voltage signal in mV
    //sampleV = esp_adc_cal_raw_to_voltage(adc1_get_raw(inPinV),adc_chars);
    //sampleI=esp_adc_cal_raw_to_voltage(adc1_get_raw(inPinI),adc_chars);
    //-----------------------------------------------------------------------------
    // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
    //     then subtract this - signal is now centred on 0 counts.
    //-----------------------------------------------------------------------------
    *offsetV = (*offsetV + (sampleV-*offsetV)/4096);
    filteredV = sampleV - *offsetV;
    *offsetI = (*offsetI + (sampleI-*offsetI)/4096);
    filteredI = sampleI - *offsetI;

    //-----------------------------------------------------------------------------
    // C) Root-mean-square method voltage
    //-----------------------------------------------------------------------------
    sqV= filteredV * filteredV;                 //1) square voltage values
    sumV += sqV;                                //2) sum

    //-----------------------------------------------------------------------------
    // D) Root-mean-square method current
    //-----------------------------------------------------------------------------
    sqI = filteredI * filteredI;                //1) square current values
    sumI += sqI;                                //2) sum

    //-----------------------------------------------------------------------------
    // E) Phase calibration
    //-----------------------------------------------------------------------------
    phaseShiftedV = lastFilteredV + PHASECAL * (filteredV - lastFilteredV);

    //-----------------------------------------------------------------------------
    // F) Instantaneous power calc
    //-----------------------------------------------------------------------------
    instP = phaseShiftedV * filteredI;          //Instantaneous Power
    sumP +=instP;                               //Sum

    //-----------------------------------------------------------------------------
    // G) Find the number of times the voltage has crossed the initial voltage
    //    - every 2 crosses we will have sampled 1 wavelength
    //    - so this method allows us to sample an integer number of half wavelengths which increases accuracy
    //-----------------------------------------------------------------------------
    lastVCross = checkVCross;
    if (sampleV > startV) checkVCross = true;
                     else checkVCross = false;
    if (numberOfSamples==1) lastVCross = checkVCross;

    if (lastVCross != checkVCross) crossCount++;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 3) Post loop calculations
  //-------------------------------------------------------------------------------------------------------------------------
  //Calculation of the root of the mean of the voltage and current squared (rms)
  //Calibration coefficients applied.

  //double V_RATIO = VCAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  *Vrms = VCAL * sqrt(sumV / numberOfSamples);

  //double I_RATIO = ICAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  *Irms = ICAL * sqrt(sumI / numberOfSamples);

  //Calculation power values
  *realPower = VCAL * ICAL * (sumP / numberOfSamples);
  *apparentPower = *Vrms * *Irms;
  *powerFactor= *realPower / *apparentPower;

  //Reset accumulators
  sumV = 0;
  sumI = 0;
  sumP = 0;
//--------------------------------------------------------------------------------------
 /* printf("realPower: %f \t", *realPower);
  //printf("apparentPower: %f \t", *apparentPower);
  printf("Vrms: %f \t", *Vrms);
  printf("Irms: %f \t", *Irms);
  printf("Vrms_pure: %f \t", *Vrms/VCAL);
  printf("Irms_pure: %f \n", *Irms/ICAL);
  //printf("powerFactor: %f \n", *powerFactor);*/
}