/*
  main.cpp
  Main code where the control takes place
  @author  Leo Korbee (c), Leo.Korbee@xs4all.nl
  @website iot-lab.org
  @license Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)
  Thanks to all the folks who contributed beforme me on this code.

  Hardware information at the end of this file.
  @version 2018-03-31

  @version 2018-06-21
  binary was broken on atmelavr platform 1.9.0 (7200 bytes hex file)
  modified platform.ini to force to platform 1.8.0 (7212 bytes hex file)

  @version 2019-12-13
  * @author Joachim Banzhaf (joachim.banzhaf@gmail.com)
  * optional led indicator
  * optional serial debug output
  * avoid floats by using original bosch driver
  * add pressure to transmitted data
  * send data as 16bit milliVolts and 32bit centiCelsius, milliPercent and Pascal
  */

// LED blinks fast on BME detect and slow on wakeup
#define LED 1
//#undef LED

// Serial debug output
#define TX  3
#undef TX

#define LORA
//#undef LORA

#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "tinySPI.h"
// lowercase is the original Bosch driver...
#include "bme280.h"
#include "LoRaWAN.h"
#include "secconfig.h" // remember to rename secconfig_example.h to secconfig.h and to modify this file


// all functions declared
ISR(WDT_vect);
void readData(bme280_data *data);
uint16_t vccVoltage();
void setUnusedPins();
void goToSleep();
void watchdogSetup();
void user_delay_ms(uint32_t period);
int8_t user_spi_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);
int8_t user_spi_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);


#ifdef TX
#include <SendOnlySoftwareSerial.h>
#define BAUD 9600
SendOnlySoftwareSerial debug(TX);
#endif

#ifdef LORA
// RFM95W
#define DIO0 10
#define NSS  9
RFM95 rfm(DIO0, NSS);

// define LoRaWAN layer
LoRaWAN lora = LoRaWAN(rfm);
#endif

// frame counter for lora
unsigned int Frame_Counter_Tx = 0x0000;

// BME280 SPI Chipselect pin
#define BME_CS 7
// Default : forced mode, standby time = 1000 ms
struct bme280_dev bme;
struct bme280_data bme_comp;

// set sleep time between broadcasts. The processor awakes after 8 seconds deep-sleep_mode,
// increase and checks the counter and sleep again until sleep_total is reached.
// 5min * 60s = 300/8 = 37,5 = 38.
// Clocks seem slow on the ATTiny84. 35 sleep intervals give more like 5:15 min delay already
const int sleep_total = 35; // 7 -> one broadcast every minute (for sporadic tests)

// sleep cycles that will be counted, start with more than sleep_total to start after 8 seconds with first broadcast.
volatile int sleep_count = sleep_total;


// Interface function required for the Bosch driver
void user_delay_ms(uint32_t period) {
  delay(period);
}

// Interface function required for the Bosch driver
int8_t user_spi_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
  digitalWrite(dev_id, LOW);
  SPI.transfer(reg_addr | 0x80);  // msb set -> read
  while( len-- )
    *(reg_data++) = SPI.transfer(0);
  digitalWrite(dev_id, HIGH);
  return 0;
}

// Interface function required for the Bosch driver
int8_t user_spi_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
  digitalWrite(dev_id, LOW);
  SPI.transfer(reg_addr & ~0x80); // msb clear -> write
  while( len-- )
    SPI.transfer(*(reg_data++));
  digitalWrite(dev_id, HIGH);
  return 0;
}


void setup()
{
#ifdef LED
  pinMode(LED, OUTPUT); // LED for testing...
  digitalWrite(LED, HIGH);
  delay(50);
  static uint8_t isOn = true;
#endif

#ifdef TX
  debug.begin(BAUD);
  debug.print("\n\nbme ");
#endif

  // Init SPI for RFM95 and BME280
  SPI.setDataMode(SPI_MODE0);
  SPI.begin();

#ifdef LORA
  //Initialize RFM module
  rfm.init();
  lora.setKeys(NwkSkey, AppSkey, DevAddr);
#endif

  // for BME sensor
  bme.dev_id = BME_CS;
  // bme.intf = BME280_SPI_INTF;
  bme.read = user_spi_read;
  bme.write = user_spi_write;
  bme.delay_ms = user_delay_ms;

  pinMode(bme.dev_id, OUTPUT);
  digitalWrite(bme.dev_id, HIGH);

  while( BME280_OK != bme280_init(&bme) ) {
#ifdef LED
    isOn = !isOn;
    digitalWrite(LED, isOn ? HIGH : LOW);
#endif
#ifdef TX
    debug.print("NO ");
#endif
    delay(200);
  }

#ifdef TX
  debug.print("setup ");
#endif
  /* Recommended mode of operation: Indoor navigation */
  bme.settings.osr_h = BME280_OVERSAMPLING_1X;
  bme.settings.osr_p = BME280_OVERSAMPLING_16X;
  bme.settings.osr_t = BME280_OVERSAMPLING_2X;
  bme.settings.filter = BME280_FILTER_COEFF_16;

  uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

  if( BME280_OK != bme280_set_sensor_settings(settings_sel, &bme) ) {
#ifdef TX
    debug.print("FAILED! ");
#endif
  }

#ifdef LED
  digitalWrite(LED, LOW);
  delay(200);
#endif
}


#ifdef TX
void printSInt( const char *label, int32_t value ) {
  debug.print(label);
  debug.print("=");
  debug.print(value);
  debug.print(", ");
}


void printUInt( const char *label, uint32_t value ) {
  debug.print(label);
  debug.print("=");
  debug.print(value);
  debug.print(", ");
}
#endif


uint8_t *serialize( uint8_t *data, uint32_t value, size_t size ) {
  while( size-- ) {
    *(data++) = value & 0xff; 
    value >>= 8;
  }
  return data;
}


void loop()
{
  // goToSleep for all devices...
  // The watchdog timer will reset.
#ifdef TX
  debug.println("sleep");
#endif
#ifdef LED
  digitalWrite(LED, LOW);  // LED off while sleeping
#endif
  goToSleep();
#ifdef LED
  digitalWrite(LED, HIGH); // LED on while active
  delay(10);               // make LED blink long enough to be visible
#endif
#ifdef TX
  printUInt("awake", sleep_count);
#endif

  // use this for non-sleep testing:
  //delay(8000);
  //sleep_count++;

  // do action if sleep_total is reached
  if( sleep_count >= sleep_total )
  {
    uint16_t mvcc;

    // define bytebuffer
    uint8_t Data[sizeof(bme_comp) + sizeof(mvcc)];
    uint8_t *data = Data;

    // read data from sensor
    readData(&bme_comp);
    // read vcc voltage (mv)
    mvcc = vccVoltage();

    data = serialize(data, mvcc, sizeof(mvcc));
    data = serialize(data, (uint32_t)bme_comp.temperature, sizeof(bme_comp.temperature));
    data = serialize(data, bme_comp.humidity, sizeof(bme_comp.humidity));
    serialize(data, bme_comp.pressure, sizeof(bme_comp.pressure));

#ifdef TX
    printSInt("Temp", bme_comp.temperature);
    printUInt("Humi", bme_comp.humidity);
    printUInt("Pres", bme_comp.pressure);
    printUInt("mVcc", mvcc);
#endif

#ifdef LORA
    lora.Send_Data(Data, sizeof(Data), Frame_Counter_Tx);
#endif
    Frame_Counter_Tx++;
#ifdef TX
    printUInt("send", Frame_Counter_Tx);
#endif

    // reset sleep count
    sleep_count = 0;
  }
}


/*
  set unused pins so no undefined situation takes place 
  ...and rumor has it this saves power during sleep
*/
void setUnusedPins()
{
  // 0, 1, 2, 3, 8
  pinMode(0, INPUT_PULLUP);
#ifndef LED
  pinMode(1, INPUT_PULLUP);
#endif
  pinMode(2, INPUT_PULLUP);
#ifndef TX
  pinMode(3, INPUT_PULLUP);
#endif
  pinMode(8, INPUT_PULLUP);
}

/**
 read temperature from BME sensor
*/
void readData( bme280_data *data ) {
  if( BME280_OK == bme280_set_sensor_mode(BME280_FORCED_MODE, &bme) ) {
    /* Wait for the measurement to complete */
    bme.delay_ms(40);
    if( BME280_OK != bme280_get_sensor_data(/* BME280_ALL, */ data, &bme) ) {
      data->humidity = 0;
      data->pressure = 0;
      data->temperature = 0;
    }
  }
}

/**
  read voltage of the rail (Vcc)
  output mV (2 bytes)
*/
uint16_t vccVoltage()
{
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  // default ADMUX REFS1 and REFS0 = 0

  // #define _BV(bit) (1 << (bit))

  // 1.1V (I Ref)(2) 100001
  ADMUX = _BV(MUX5) | _BV(MUX0);

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  uint16_t result = (high<<8) | low;

  // result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  // number of steps = 1023??
  result = (1125300L / result) ; // Calculate Vcc (in mV);

  return result;
}


void watchdogSetup()
{
  // Table for clearing/setting bits
  //WDP3 - WDP2 - WPD1 - WDP0 - time
  // 0      0      0      0      16 ms
  // 0      0      0      1      32 ms
  // 0      0      1      0      64 ms
  // 0      0      1      1      0.125 s
  // 0      1      0      0      0.25 s
  // 0      1      0      1      0.5 s
  // 0      1      1      0      1.0 s
  // 0      1      1      1      2.0 s
  // 1      0      0      0      4.0 s
  // 1      0      0      1      8.0 s


  // Reset the watchdog reset flag
  bitClear(MCUSR, WDRF);
  // Start timed sequence
  bitSet(WDTCSR, WDCE); //Watchdog Change Enable to clear WD
  bitSet(WDTCSR, WDE); //Enable WD

  // Set new watchdog timeout value to 8 second
  bitSet(WDTCSR, WDP3);
  bitClear(WDTCSR, WDP2);
  bitClear(WDTCSR, WDP1);
  bitSet(WDTCSR, WDP0);
  // Enable interrupts instead of reset
  bitSet(WDTCSR, WDIE);
}

void goToSleep()
{
  //Disable ADC, saves ~230uA
  ADCSRA &= ~(1<<ADEN);
  watchdogSetup(); //enable watchDog
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  // sleep_mode does set and reset the SE bit. sleep_enable and sleep_disable are not usefull
  sleep_mode();
  //disable watchdog after sleep
  wdt_disable();
  // enable ADC
  ADCSRA |=  (1<<ADEN);
}


ISR(WDT_vect)
{
  sleep_count++; // keep track of how many sleep cycles have been completed.
}


/*
  Hardware setup
  Attiny84 using the Arduino pin numbers! PB0 = 0 etc.


  Atmel ATtiny84A PU
  RFM95W
  BME280

  Power: 3V3 - 470uF elco over power connectors, 100nF over power connector for interference suppression
  Connections:
  RFM95W   ATtiny84

  DIO0 -- PB0
  MISO -- MOSI
  MOSI -- MISO
  SCK  -- SCK
  NSS  -- PB1 (this is Chipselect)

  Bosch BME280 sensor used with tinySPI....
  BME280  ATtiny84
  SCL -- SCK
  SDA -- MISO
  SDO -- MOSI
  CSB -- PA7 (this is Chipselect)(Arduino pin 7)
*/
