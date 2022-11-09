# SpoddyCoder Arduino LED Controller

A simple(-ish) Arduino LED PWM controller;

* 6 light outputs, each indivudally controllable;
    * 7 light patterns
    * 10 brightness levels
* 4 digit 7 segment LED display
    * For user feedback and messages
* 3 control buttons;
    * Select Light 
        * Short press to select next light
        * Long press to isolate light
        * Long press again to show all lights
    * Set Pattern
        * Short press to select next pattern
        * Long press to randomise the light pattern offsets
            * Becasue light displays dont look good when they're perfectly in sync
        * 7 preset patterns available;
            * slow breathing
            * fast breathing
            * slow flash
            * fast flash
            * slow random
            * fast random
            * always on
    * Set Brightness
        * 9 = max - 0 = off
* Stores / restores settings automatically to EEPROM


For my love, Anjou x.


## Requirements

* Any Arduino
* `SevSeg` library installed: https://github.com/DeanIsMe/SevSeg
* Some hardware (not detailed in this README)
    * 3 x input switches
    * 4 digit 7 segment display
    * Some output LED's / Lights
    * Output stage electronics for each light
        * Could be a simple transistor switching circuit for low power LED's
        * You'd probably want MOSFET switching circuit for high power use cases


## Build Notes

* Pin numbers etc can be edited in the defines at the top of the script
* This project uses every available IO pin on an Arduino Nano, including D0 + D1
    * This means serial (USB) connection wont work while the sketch is running
    * So you'll need to reset the Arduino while uploading
* Other config can also be adjusted in the defines in the script - have a read.


### Additional Notes

I'm using some cheapo Arduino Nano boards, that use the ATMega328P with the old bootloader, FQBN...

```
arduino:avr:nano:cpu=atmega328old
```

Compile, upload...

```
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old spoddy-led-controller
arduino-cli upload -p COM7 --fqbn arduino:avr:nano:cpu=atmega328old spoddy-led-controller
```
