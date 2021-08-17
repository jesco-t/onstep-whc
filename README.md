# Wireless Handcontroller for OnStep

This is a wireless handcontroller for the awesome OnStep telescope controller. Currently it only controls the focuser 1 module. It is based on the ESP8266 WiFi board.

# Current PCB

Revision 2 of the PCB is planned to support one focuser and mount movement. A simple 0.91" OLED display is used to show operation mode and simple coordinates.

![R2 PCB](/support-files/PCB_R2.jpg)

Part list is
* WeMos D1 Mini Pro
* 5x push buttons
* 5x 10k resistors
* 1x LED, red
* 1x 4.7k resistor
* 1x 0.91" 128x32 OLED (SSD1306 based)
* Female pin headers to mount WeMOS and OLED
* Male pin header to create jumper switch for on/off
* Battery holder for 4x AA batteries as power supply (mounted on backside)

# What already works, and what doesn't

Good
* Focuser 1 works
  * focus in and out
  * speed change slow/fast (switches to fast after 1s)

Bad
* No mount control yet
* No display output yet

# Commands
* Focus Inward = Upper Button
* Focus Outward = Lower Button

