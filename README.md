# ATTiny84TTN
ATTiny84 TTN Node from Leo Korbee (https://www.iot-lab.org/blog/101/) 
+ optional LED, Serial or Pressure by me (JoBa-1)

## Overview

Send data to TheThingsNetwork. For details look at Leo's great site, in general you need to

* Build breadboard node following Leos cirquit diagram (or use his PCB)
    * ATTiny84 (DIP14 format)
    * RFM95W (solder on an ESP32 breakout board where you removed the 2 10K resistors, not the 0k resistor)
    * BME280 breakout board with 6 pins (4 pins is I2C only, we want SPI)
    * Somewhat optional: 100nF and 470µF condensators at the ATTiny +/Gnd pins
    * You dont need the GPIO outputs from Leos cirquit diagram. They are just nice to have on a PCB
    * Optionally connect a led with 470 Ohm resistor from ground to io pin 1 (PB1).
        * Blinks once at startup (50ms)
        * Blinks fast during startup if BME280 is not found (200ms interval)
        * Blinks short on wakeups (8 s interval)
        * Blinks long on aquiring sensor data and sending to TTN (5 min interval)
    * Optionally connect Gnd and pin 3 (PA7) to an USB2Serial Gnd and Rx to monitor at 9600 baud
        * You cant have serial and lora at the same time though. Not enough flash space :(
* Register a TTN account for free
* Login to TTN
* Go to your account menu (upper left) and select Console
* Create an application: Give it a unique name (ID) and a description. Leave the rest at defaults
* Register your node as a device implementing the application
    * Give it a name and description
    * Select activation method ABP (or adapt the code for OTAA, can be changed later in the TTN device settings)
    * Leave rest at defaults
    * Change device settings: Disable frame checks (since the device resets framecounter on restart)
* Copy file src/secconfig_example.h to src/secconfig.h 
* Copy ID and keys from the device page into the new file (use C-Style)
* Connect the ATTiny84 to your UsbAsp programmer, as usual (or use your own flash method, adapting platformio.ini)
    * you can do these connections on the breadboard, but make sure the programmer only uses 3.3V or your BME280 will not be happy
    * Gnd  <-> Gnd
    * MISO <-> MISO
    * MOSI <-> MOSI
    * SCLK <-> SCLK
    * Reset <-> Reset
    *  +   <-> + (if not powered already)
* Connect the programmer to an USB port
* Call pio run --target upload (or click the right buttons in your IDE of choice...)
* Put the ATTiny in your breadboard, connect power and wait for 8s for the first data to arrive at the TTN device page
    * ...if a gateway is in range
    * ...and you did no mistake - it works for me :P
    
Good Luck
    
## Options

Define your payload format on the TTN application page to use more common units. I use this:

### Decoder
```
function Decoder(bytes, port) {
  var decoded = {};
  
  if (port === 1) {
    decoded.mvcc = bytes[0] | bytes[1] << 8;
    decoded.temp = bytes[2] | bytes[3] << 8 | bytes[4] << 16 | bytes[5] << 24;
    decoded.humi = bytes[6] | bytes[7] << 8 | bytes[8] << 16 | bytes[9] << 24;
    decoded.pres = bytes[10]| bytes[11]<< 8 | bytes[12]<< 16 | bytes[13]<< 24;
  }
  else {
    decoded.humi = -1;
  }

  return decoded;
}
```
### Converter
```
function Converter(decoded, port) {
  var converted = {};

  if (port === 1) {
    converted.vcc_V        = decoded.mvcc / 1000;
    converted.temp_degC    = decoded.temp /  100;
    converted.humi_Percent = decoded.humi / 1000;
    converted.pres_hPa     = decoded.pres /  100;
    if (decoded.humi >= 0 && decoded.humi <= 100000) {
      converted.valid = true;
    }
    else {
      converted.valid = false;
    }
  }

  return converted;
}
```
### Validator
```
function Validator(converted, port) {
  if (port === 1) {
    return converted.valid;
  }
  
  return true;
}
```
