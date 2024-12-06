# LaCrosse-WS2315-external-sensors
Intercept and decode the RF signal of a LaCrosse WS-2315/3600 Weather station external sensors

The only way to upload weather data to Weather Undeground from a LaCrosse WS-2315 console was through a PC connected to serial interface of the console. As serial ports are becoming obsolete to new PC, I decided to use a D1 mini (ESP8266) to intercept the external sensor's RF signal, decode the weather information and send the data through serial port of D1 mini to an another ESP8266 for uploading the data to Weather Underground (WiFi). 

Because the external sensor does not have a Pressure sensor I used a BMP280 connected to the receiving ESP8266.

Instead of D1 mini you can use an Arduino (uno and others).

The format of the RF signal was obtained from the work of merbanan/rtl_433
