# CodeLess ESP32 RGBW ArtNet Neopixel Driver - THIS IS A VERY EARLY ALPHA RELEASE, THERE WILL BE BUGS, THERE WILL BE LOTS OF DEBUG MESSAGES
 # Prerequisites

-  An ESP32 board, with pin 23 VOltage Level Shifted to drive your Neopixels
-  Pin 34 is used to select config mode/normal runtime. Pin needs to be grounded and a pushbutton conected to 5/3.3v

# Firmware upload instructions

For Windows PC you can use the Espressif Firmware upload tool availbale here https://www.espressif.com/en/support/download/other-tools
Firmware files are located in the "Firmware" folder and need to be mapped as follows:

![Espressif Upload Tool](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Firmware/ESPTool.png)

For OSX or other platforms you can use the web based formware uplaod tool https://esp.huhn.me/ with the following address map:

![Web Upload Tool](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Firmware/ONLINETool.png)

-  Once the firmware is uplaoded, hold down the boot mode button connected to pin 34, and reset the ESP32. 
-  The device will boot up with its own WIFI Access point.
-  Use your phone/tablet/pc to connect to the following access point
SSID: WOW-AP
Key: wowaccesspoint
-  Open your browser and goto http://10.10.10.1
-  The configuration page will load 
![Web UI](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Firmware/ui.jpg)
-  Enter your SSID/KEY for your wifi network
-  Select DHCP or STATIC IP Mode
-  For DHCP the Hostname of your ESP32 will be "NODEXXXX" where XXXX are the last 2 bytes of its MAC Address
-  For Static mode, you can enter the ESP32 IP/Mask/Gateway and Hostname
-  Artnet config alows you to have as many Universes as you like (keep in mind that the more universes you set up the slower your render time will be, this is designed to drive 4 x 128 RGBW pixels with relatively good refresh rate)
-  Once done hit "SAVE & Reboot"
-  The ESP32 will try connecting to your wifi network, and after a few moments is ready to receive ArtNet Data

# Code - if you want to DIY or Alter the code
  # Prerequisites

  - Arduino IDE 
  - The CLockless Neopixel Driver https://github.com/hpwit/I2SClocklessLedDriver for fast Noepixel driving
  - ESP32 Upload tool https://github.com/me-no-dev/arduino-esp32fs-plugin to uplaod your SPIFS UI
  - ESP32 Async Web Server https://github.com/me-no-dev/ESPAsyncWebServer for websockets
  - ESP32 Async TCP Lib https://github.com/me-no-dev/AsyncTCP

Upload and compile the code, then use the ESP32 Upload tool to uplaod the UI page to SPIFS
