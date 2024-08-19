# Build Instructions
![Parts](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/Parts1of2.jpg)
![SBEC](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/sbec.jpg)
![JST Cables](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/jst.jpg)

Parts List:
-  1 x Lolin D32 - ESP32 Module
-  2 x 16pin header pins
-  1 x Green PCB Module
-  1 x Level Shifter IC - 74LS245N
-  1 x Push Button
-  1 x 100K Pushbutton Resistor
-  1 x Green Power Filter Capacitor
-  1 x Purple USBC Power Module
-  1 x 20A SBEC Power Module
-  2 x Pairs of (1 Male & 1 Female) 4PIN JST Clip Cables
-  1 x Pair of (1 Male & 1 Female) 3PIN JST Clip Cables

Sundry Parts:
-  Wide Heat Shrink Tube
-  Narrow Heat Shrink Tube

# Breakdown of Assembly

- Assemble the USBC Power module
- Assemble the SBEC Power Module
- Assemble the ESP32 Module
- Assemble the PCB: Add the Push Button and resistor. Add the Level shifter. Add the ESP32 Module, Add the Capacitor, finish with power cable and data cable

# Assemble the USBC Power module

- 1 x Purple USBC Power Module
- 1 x FEMALE 4 Pin JST Cable
- 2 x Small pieces of WIDE Heat Shrink
 
1. Remove the protective tape from the x3 dip switch on the USBC module and set the DIP switch to 011. So that SW1=DOWN SW2=UP SW3=UP

![USBC PD Module](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/usbcpd.jpg)
2. Cut the 4pin JST cable to a length of 5CM from the end of the BLACK PLUG
![USBC PD Module](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/usbcpd-1.jpg)
3. Split the Cable into 2 pairs.
   - RED & YELLOW : + Voltage
     - Strip 10mm of both RED & YELLOW Jackets and tie them together
   - GREEN & BLUE : - Voltage
     - Strip 10mm of both GREEN & BLUE Jackets and tie them together
![USBC PD Module](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/usbcpd-2.jpg)
![USBC PD Module](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/usbcpd-3.jpg)
4. Insert the RED & YELLOW tied pair into the VCC hole of the USBC Module, bend the excess to the SIDE
5. Insert the GREEN & BLUE tied pair into the GND hole of the USBC Module, bend the excess to the SIDE
![USBC PD Module](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/usbcpd-4.jpg)
6. Solder each connection
![USBC PD Module](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/usbcpd-5.jpg)
7. Slip 1 piece of WIDE Heat Shrink over the end where the cables were soldered and heat to shrink
![USBC PD Module](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/usbcpd-6.jpg)
8. Slip the 2nd piece of WIDE Heat Shrink over the end closest to the USBC Port and heat to shrink
![USBC PD Module](https://github.com/leonyuhanov/ESP32_RGBW_ArtNet/blob/main/Module%20Build%20Documentation/usbcpd-7.jpg)
9. Connect the Module to the PowerBanks 100W USBC Port using a 100W capable USBC Cable
10. Use a multimeter to verify you are receiving 20V on the end of the 4PIN Female JST Cable

# Assemble the 20A SBEC Power module
- 1 x SBEC Power Module
- 1 x MALE 4 Pin JST Cable
- 1 x FEMALE 4 Pin JST Cable
- 2 x Small pieces of NARROW Heat Shrink

1. Cut the plastic connectors from teh very ends of the OUTPUT cable, then carefully unwide the ferrite RING from the cables
2. SPlit the sables into 2 pairs
   - 2 x RED Cables are the +5V
   - 2 x BLACK Cables are the GND
4. Cut the OUTPUT cable so that you have 10cm from the module
5. Strip 10mm of the jacketing from both pairs of OUTPUT CABLES and join the SAME coloured pairs together
6. Cut the INPUT Cables so that you have 10cm from the modile
7. Strip 10mm of the jacketing from both OUTPUT CABLES
8. Cut the 4pin MALE JST cable to a length of 5CM from the end of the BLACK PLUG
9. Split the Cable into 2 pairs.
   - RED & YELLOW : + Voltage
     - Strip 10mm of both RED & YELLOW Jackets and tie them together
   - GREEN & BLUE : - Voltage
     - Strip 10mm of both GREEN & BLUE Jackets and tie them together
10. Solder the RED & YELLOW tie from the JST cable to the RED INPUT cable of the SBEC
11. Solder the GREEN & BLUE tie from the JST cable to the BLACK INPUT cable of the SBEC
12. Seal each solder joint with electrical tape and heatshrink over the top
13. Cut the 4pin FEMALE JST cable to a length of 5CM from the end of the BLACK PLUG
14. Split the Cable into 2 pairs.
   - RED & YELLOW : + Voltage
     - Strip 10mm of both RED & YELLOW Jackets and tie them together
   - GREEN & BLUE : - Voltage
     - Strip 10mm of both GREEN & BLUE Jackets and tie them together
15. Solder the RED & YELLOW tie from the JST cable to the PAIR of RED OUTPUT cables of the SBEC
16. Solder the GREEN & BLUE tie from the JST cable to the PAIR of BLACK OUTPUT cables of the SBEC
17. Seal each solder joint with electrical tape and heatshrink over the top
