# ESP32-C5 with 5G WIfi Experimental build April 2025 RGBW

This build is purely experimental for now. It uses the following:
- [ESP32-C5 Dev Module](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c5/esp32-c5-devkitc-1/index.html)
- ESP IDF V 5.4 Stable
- PIN GPIO_NUM_6 is Grounded on BOOT. connect it to resistor and to 3.3v via a pushbutton to initiate config modode THEN push reset and release the pushbutton

# Build Firmware Instructions (Windows with ESP IDF 5.4 Stable)

- cd into the directory RGBWArtnetDriverC5Experimental
- set the Build target "idf.py --preview set-target esp32c5"
- to Build "idf.py build" this will build the code into the firmware
- To create firnedly binaries run "createFirmwarePackage.bat" this will place the BINARIES into the Firmware folder with the correct memory addresses on the end of the file names

# Build Instructions to go directly to the Dev Module (Windows with ESP IDF 5.4 Stable)

- connect the ESP32-C5 Module via the serial USBC port
- cd into the directory RGBWArtnetDriverC5Experimental
- set the Build target "idf.py --preview set-target esp32c5"
- to Build  flash and run "idf.py -p com4 flash monitor"

# Codeless Instructions 

For Windows PC you can use the Espressif Firmware upload tool availbale here https://www.espressif.com/en/support/download/other-tools
Firmware files are located in the "Firmware" folder and need to be mapped as follows:

![Espressif Upload Tool](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/RGBWArtnetDriverC5Experimental/Firmware/UpploadTool.jpg)

# Operation

-  Once the firmware is uplaoded, hold down the boot mode button connected to pin 6, and reset the ESP32. 
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

# Custom PCB

Gerbers are here [ESP32-C5 PCB Revision 1.1](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/Rev1.1.zip)
![Revision 1.1 PCB](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/revision1.1.jpg)
