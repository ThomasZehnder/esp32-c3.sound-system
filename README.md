# ESP32-C3 demo

## Used Hardware ESP32-C3

![ESP32-C3 development board](images/esp32-c3-0-42-inch-oled-serial-and-wire-issues-v0-vredkxdz87se1.webp)

### DFPlayer Mini Wiring

Current software configuration for the DFPlayer is defined in [src/dfplayer.cpp](src/dfplayer.cpp):

* ESP32-C3 GPIO4 -> DFPlayer RX
* ESP32-C3 GPIO3 <- DFPlayer TX
* ESP32-C3 GND -> DFPlayer GND
* ESP32-C3 5V -> DFPlayer VCC
* Speaker -> DFPlayer SPK_1 and SPK_2

Notes:

* OLED already uses GPIO5 and GPIO6.
* Onboard LED uses GPIO8.
* Use a common GND between ESP32-C3 and DFPlayer.
* Power the DFPlayer from 5V, not 3.3V.
* A 1 kOhm resistor between ESP32 TX and DFPlayer RX is recommended.
* Connect one speaker directly between SPK_1 and SPK_2.
* Do not connect SPK_1 or SPK_2 to GND.
* Do not use SPK_1 and SPK_2 as two separate stereo outputs.

Optional:

* Use DFPlayer DAC_R and DAC_L instead of SPK_1 and SPK_2 when connecting an external amplifier.
* BUSY is currently not connected in software.

### Ultrasound Output Note

Current software uses GPIO1 for the ultrasound PWM output.

Hardware note:

* Crackling on stop was traced to unstable VCC on the connected output stage.
* Adding supply buffering close to the load or driver helps, for example a 100 nF ceramic capacitor together with a larger bulk capacitor.
* Keep GND short and common between the ESP32-C3 and the ultrasound driver stage.

### DFPlayer SD Card Layout

The current software uses the recommended numbered folder structure:

* /01/001.mp3 -> sunrise
* /01/002.mp3 -> chime
* /01/003.mp3 -> ocean
* /01/004.mp3 -> alarm
* /sound?name=stop -> stop playback

If you want to add more groups later, use folders like /02, /03, ... with numbered files inside them.

## Use PlatformIO

In VSCode see elements in the buttom left corner to transfer projekt to arduino board.

### Sample Configuration
