# LaCrosse-WS2315-external-sensors
Intercept and decode the RF signal of a LaCrosse WS-2315/3600 Weather station external sensors with ARDUINO UNO and a RF 433 MHz receiver

The only way to upload weather data to Weather Undeground from a LaCrosse WS-2315 console was through a PC connected to serial interface of the console. As serial ports are becoming obsolete to new PC, I decided to use an Arduino UNO with an RF_433 MHz receiver to intercept the external sensor's RF signal, decode the weather information and send the data through serial port to an ESP8266 for uploading the data to Weather Underground (WiFi). 

Because the external sensor does not have a Pressure sensor I used a BMP280 connected to the receiving ESP8266.

The format of the RF signal was obtained from the work of merbanan/rtl_433
