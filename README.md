This project aims to keep RC-flier's hands warm at winter. Nothing prevents
using this product with anything that needs constant heatpower.

### Author
Pertti Erkkilä (pertti.erkkila@gmail.com)

### License
GPLv3

### Story
In Finnish: http://www.kopterit.net/index.php/topic,17572.0.html

### Features
- constant heatpower stages (configurable) using PWM
- lipo-cell count auto-detection
- battery-monitor for cutoff

### Steps to get stuff working
- build hardware
- get Arduino 1.0.1 (1.0.2 won't work)
- get arduino-tiny -libraries (core + PinChangeInterrupt): http://code.google.com/p/arduino-tiny/
- suitable programmer (Arduino as ISP works well)
- for a new chip, burn bootloader
- burn firmware
