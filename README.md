# ATTiny84TTN
ATTiny84 TTN Node from Leo Korbee (https://www.iot-lab.org/blog/101/) + LED + Serial

## Overview

Send data to TheThingsNetwork. For details look at Leo's great site, in general you need to

* Build breadboard node following Leos cirquit diagram (or use his PCB)
    * ATTiny84 (DIP14 format)
    * RFM95W (solder on an ESP32 breakout board where you removed the 2 10K resistors, not the 0k resistor)
    * BME280 breakout board with 6 pins (4 pins is I2C only, we want SPI)
    * Somewhat optional: 100nF and 470µF condensators at the ATTiny +/Gnd pins
    * You dont need the GPIO outputs from Leos cirquit diagram. They are just nice to have on a PCB
    * Optionally connect a led with 470 Ohm resistor from ground to pin 1 (PB1).
        * Blinks fast at startup if BME280 not found (200ms interval)
        * Blinks short on wakeups (8 s interval)
        * Blinks long on aquiring sensor data and sending to TTN (5 min interval)
    * Optionally connect Gnd and pin 3 (PA7) to an USB2Serial Gnd and Rx to monitor at 9600 baud
* Register a TTN account for free
* Login to TTN
* Go to your account menu (upper left) and select Console
* Create an application: Give it a unique name (ID) and a description. Leave the rest at defaults
* Register your node as a device implementing the application
    * Give it a name and description
    * Select activation method ABP (or adapt the code for OTAA, can be changed in the device settings)
    * Leave rest at defaults
* Copy file src/secconfig_example.h to src/secconfig.h 
* Copy ID and keys from the device page (bottom) into the new file
* Connect the ATTiny84 to your UsbAsp programmer, as usual (or use your own flash method, adapting platformio.ini)
    * Gnd  <-> Gnd
    * MISO <-> MISO
    * MOSI <-> MOSI
    * SCLK <-> SCLK
    * Reset <-> Reset
    *  +   <-> +
* Connect the programmer to an USB port
* Call pio run --target upload (or click the right buttons in your IDE of choice...)
* Put the ATTiny in your breadboard, connect power and wait for 8 s for the first data to arrive at the TTN device page
    * ...if a gateway is in range
    * ...and I did no mistake - it is not working for me yet :P
    
Good Luck
    
