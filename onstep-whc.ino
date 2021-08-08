
/*
 * Title       onstep-whc
 * by          Jesco Topp
 *
 * Copyright (C) 2021 Jesco Topp
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Description:
 *   This is a wireless handcontroller for the awesome OnStep telescope 
 *   controller. Currently it only controls the focuser 1 module. It is 
 *   based on the ESP8266 WiFi board.
 *   
 *   The software is published here in the hope that it is helpful to someone.
 *
 * Author: Jesco Topp
 *   jesco.topp@gmail.com
 *
 * Revision history, and newer versions:
 *   See GitHub: https://github.com/jesco-t/onstep-whc
 *
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#define I2C_ADDRESS 0x3C

/*
 * 
 * https://funduino.de/nr-42-oled-display-ssd1306-128x64-128x32

/* Enable debugging messages via the serial monitor */
#define DEBUG
#define DebugSer Serial

#ifdef DEBUG
  #define D(x)       DebugSer.print(x)
  #define DF(x)      DebugSer.print(F(x))
  #define DL(x)      DebugSer.println(x)
  #define DLF(x)     DebugSer.println(F(x))
#else
  #define D(x)
  #define DF(x)
  #define DL(x)
  #define DLF(x)
#endif

// * * * * * * * * * * * * * * * * * * * * *
// Configuration
// * * * * * * * * * * * * * * * * * * * * *

/* PINMAP (PINMAP_BREADBOARD,...) */
#define PINMAP_PCB_R2

#ifdef PINMAP_PCB_R2
  #define PIN_UP      D1
  #define PIN_DOWN    D6
  #define PIN_LEFT    D7
  #define PIN_RIGHT   D0
  #define PIN_SPECIAL RX
  #define LED_RED     D4
#endif

/* Network credentials (OnStep defaults are "ONSTEP" and "password" */
const char* ssid     = "ONSTEP";
const char* password = "password";
IPAddress onstep(192,168,0,1);
WiFiClient cmdSvrClient;

/* Focuser speed settings */
int focus_smallstep = 25;
int focus_largestep = 100;
int focus_delay = 100;

// Button variables
int pinUp_status = 0;
int pinDown_status = 0;
int pinSpecial_status = 0;
int pinLeft_status = 0;
int pinRight_status = 0;
int pinUp_duration = 0;
int pinDown_duration = 0;
int pinSpecial_duration = 0;
int pinLeft_duration = 0;
int pinRight_duration = 0;
unsigned long lastReadout_time = 0;
unsigned long Readout_time = 0;

SSD1306AsciiWire oled;

// * * * * * * * * * * * * * * * * * * * * *
// Command communication with OnStep server
// * * * * * * * * * * * * * * * * * * * * *
String processCommand(String cmd){
  // Debugging output
  D("Command: "); D(cmd); D(" - ");
  
  // Connect to cmdSvr
  if (!cmdSvrClient.connect(onstep, 9999)) {
    DL("connection failed");
    delay(1000);
    return "NO_CONNECTION";
  }
  // Send command
  if (cmdSvrClient.connected()) {
    cmdSvrClient.println(cmd);
    D(cmd); DL(" successfully sent - ");
  } 
  // wait for data return to be available
  /*unsigned long timeout = millis();
  while (cmdSvrClient.available() == 0) {
    if (millis() - timeout > 50) {
      DL(">>> Client Timeout !");
      cmdSvrClient.stop();
      //delay(10000);
      return "TIMEOUT";
    }
  }*/
  delay(10);
  // Read return data
  String data_return = "";
    while (cmdSvrClient.available()) {
    char ch = static_cast<char>(cmdSvrClient.read());
    D(ch);
    data_return += ch;
  }
  // Close the connection
  cmdSvrClient.stop();
  DL();

  return data_return;
}

// * * * * * * * * * * * * * * * * * * * * *
// Read button status
// * * * * * * * * * * * * * * * * * * * * *
void readPins() {
  // Save button status from last call
  int pinUp_prevstatus = pinUp_status;
  int pinDown_prevstatus = pinDown_status;
  int pinSpecial_prevstatus = pinSpecial_status;
  int pinLeft_prevstatus = pinLeft_status;
  int pinRight_prevstatus = pinRight_status;

  // save current time as helper to determine how long buttons are pushed (consider first run of this function when lastReadout_time is 0)
  Readout_time = millis();
  if (lastReadout_time == 0) {
    lastReadout_time = Readout_time;
  }

  // read buttons
  pinUp_status = digitalRead(PIN_UP);
  pinDown_status = digitalRead(PIN_DOWN);
  pinSpecial_status = digitalRead(PIN_SPECIAL);
  pinLeft_status = digitalRead(PIN_LEFT);
  pinRight_status = digitalRead(PIN_RIGHT);

  // Determine push time, if button was previously pushed
  if (pinUp_status == 1) {
    // code for button previously pushed
    if (pinUp_prevstatus == 1) {
      pinUp_duration = pinUp_duration + (Readout_time - lastReadout_time);
    } else {
      pinUp_duration = 0;
    }
  }
  else {
    pinUp_duration = 0;
  }
  if (pinDown_status == 1) {
    // code for button previously pushed
    if (pinDown_prevstatus == 1) {
      pinDown_duration = pinDown_duration + (Readout_time - lastReadout_time);
    } else {
      pinDown_duration = 0;
    }
  }
  else {
    pinDown_duration = 0;
  }
  if (pinLeft_status == 1) {
    // code for button previously pushed
    if (pinLeft_prevstatus == 1) {
      pinLeft_duration = pinLeft_duration + (Readout_time - lastReadout_time);
    } else {
      pinLeft_duration = 0;
    }
  }
  else {
    pinLeft_duration = 0;
  }
  if (pinRight_status == 1) {
    // code for button previously pushed
    if (pinRight_prevstatus == 1) {
      pinRight_duration = pinRight_duration + (Readout_time - lastReadout_time);
    } else {
      pinRight_duration = 0;
    }
  }
  else {
    pinRight_duration = 0;
  } 
  if (pinSpecial_status == 1) {
    // code for button previously pushed
    if (pinSpecial_prevstatus == 1) {
      pinSpecial_duration = pinSpecial_duration + (Readout_time - lastReadout_time);
    } else {
      pinSpecial_duration = 0;
    }
  }
  else {
    pinSpecial_duration = 0;
  }  
   
  // debug output
  D("pinUp_status: "); D(pinUp_status); D(" / pressed time: "); DL(pinUp_duration);
  D("pinDown_status: "); D(pinDown_status); D(" / pressed time: "); DL(pinDown_duration);
  D("pinSpecial_status: "); D(pinSpecial_status); D(" / pressed time: "); DL(pinSpecial_duration);
  D("pinLeft_status: "); D(pinLeft_status);  D(" / pressed time: "); DL(pinLeft_duration);
  D("pinRight_status: "); D(pinRight_status); D(" / pressed time: "); DL(pinRight_duration);

  // clean up and save last button readout time
  lastReadout_time = Readout_time;
}

// * * * * * * * * * * * * * * * * * * * * *
// Setup routine
// * * * * * * * * * * * * * * * * * * * * *
void setup() {
  // set up input and output pins
  pinMode(PIN_UP, INPUT); // focus in
  pinMode(PIN_DOWN, INPUT); // focus out
  pinMode(PIN_LEFT, INPUT); // focus out
  pinMode(PIN_RIGHT, INPUT); // focus out
  pinMode(PIN_SPECIAL, INPUT); // focus speed change
  pinMode(LED_RED, OUTPUT);

  // initialize I2C
  /*Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x32, I2C_ADDRESS);*/

  /*oled.setFont(System5x7); // Auswahl der Schriftart
  oled.clear();
  oled.println("Viel");
  oled.print("Erfolg!!!");*/
  
  // set LED on at startup
  digitalWrite(LED_RED, LOW);

#ifdef DEBUG
  DebugSer.begin(115200); 
  delay(5000); 
  DebugSer.flush();
  DL("Starting Wireless Handcontroller...");
#endif

  // Connect to controller WiFi
  WiFi.begin(ssid, password);             
  DL("Connecting to "); 
  D(ssid); DL(" ..."); Serial.flush();
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    D(++i); D(' ');
  }
  DL("");
  DL("Connection established!");  
  D("IP address:\t");
  DL(WiFi.localIP());

  // Check response from ":GVP#" - has to be "On-Step#"
  if (processCommand(":GVP#") == "On-Step#") {
    DL("Successfully found OnStep command server.");
    digitalWrite(LED_RED, HIGH);
  }
  else {
    DL("Did not find OnStep command server. Continuing anyways");
    // add led blink code in when PCB with leds is available
  }

  // temp - start in focus mode
  //digitalWrite(LED_RED, HIGH);

  // set first value for readout of buttons, otherwise it is undefined
  lastReadout_time = millis();

  //DL(digitalRead(PIN_UP));
  //delay(10000);
}

// * * * * * * * * * * * * * * * * * * * * *
// main program loop
// * * * * * * * * * * * * * * * * * * * * *
void loop() {
  String cmd;
  String cmd_result;

  // get start time for loop
  unsigned long start_time = millis();
  
  // get status of all buttons
  readPins();
  //delay(1000);

  // Upper button = Move Focus Inward
  if (pinUp_status == 1 && pinDown_status == 0 && pinSpecial_status == 0){
    while (pinUp_status == 1 && pinDown_status == 0 && pinSpecial_status == 0){
      cmd = ":FR" + String(focus_smallstep);
      if (pinUp_duration > 1000 ) {
        cmd = ":FR" + String(focus_largestep);
      }
      cmd = cmd + "#";
      DL(cmd);
      cmd_result = processCommand(cmd);
      delay(focus_delay);
      readPins();
    }
  }
  /*
   * Lower button = Move Focus Outward
   */
  if (pinUp_status == 0 && pinDown_status == 1 && pinSpecial_status == 0){
    while (pinUp_status == 0 && pinDown_status == 1 && pinSpecial_status == 0){
      cmd = ":FR-" + String(focus_smallstep);
      if (pinDown_duration > 1000 ) {
        cmd = ":FR-" + String(focus_largestep);
      }
      cmd = cmd + "#";
      DL(cmd);
      cmd_result = processCommand(cmd);
      delay(focus_delay);
      readPins();
    }
  }
  /*
   * set current focuser position as new home
   */
  if (pinUp_status == 0 && pinDown_status == 0 && pinSpecial_status == 1){
    delay(1000); // only issue command if button is pressed for more than one second
      readPins();
    if (pinUp_status == 0 && pinDown_status == 0 && pinSpecial_status == 1){
      cmd = ":FH#";
      cmd_result = processCommand(cmd);
    }
  }
  /*
   * move focuser to home position
   */
  if (pinUp_status == 1 && pinDown_status == 1 && pinSpecial_status == 0){
    delay(1000); // only issue command if button is pressed for more than one second
      readPins();
    if (pinUp_status == 1 && pinDown_status == 1 && pinSpecial_status == 0){
      cmd = ":Fh#";
      cmd_result = processCommand(cmd);
    }
  }

  // ensure constant execution time
  unsigned long end_time = millis();
  unsigned long run_time = end_time - start_time;
  if ( run_time < 250) {
    D("Loop time: ");
    D(run_time);
    DL("ms");
    delay(250-run_time);
  }
}
