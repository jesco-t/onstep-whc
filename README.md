# Wireless Handcontroller for OnStep

This is a wireless handcontroller for the awesome OnStep telescope controller. Currently it only controls the focuser 1 module. It is based on the ESP8266 WiFi board.

# Development breadboard

This is a snapshot of my simples development breadboard. It only controls the focuser for now.

![Simple Breadboard](/support-files/Breadboard_Focuser-only.jpg)

Part list is
* WeMos D1 Mini Pro
* push buttons
* 10k resistors
* power supply board (to give 3.3V to the power rails)

# What already works, and what doesn't

Good
* Focuser 1 works
  * focus in and out
  * speed change slow/fast
  * set home position
  * go to home position

Bad
* No mount control yet

# Commands
* Focus Inward = Upper Button
* Focus Outward = Lower Button
* Set focus speed to fast = Upper + Middle Button
* Set focus speed to slow = Lower + Middle Button
* Set current position as home/middlle = Middle Button for longer than 1s
* Move focuser to home/middle position = Upper + Middle Button for longer 1s

