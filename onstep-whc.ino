
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

/*
 * OnStep extended LX200 protocol
 * 
Get date                                  :GC#  Reply: MM/DD/YY#
Get time (Local, 24hr format)             :GL#  Reply: HH:MM:SS#
Set target RA                             :SrHH:MM:SS# *  Reply: 0 or 1
Set target Dec                            :SdsDD:MM:SS# *   Reply: 0 or 1
Get telescope RA                          :GR#  Reply: HH:MM:SS# *
Get telescope Dec                         :GD#  Reply: sDD*MM'SS# *
Move telescope (to current Equ target)    :MS#  Reply: e *2
Stop telescope                            :Q#   Reply: [none]
Move telescope east (at current rate)     :Me#  Reply: [none]
Move telescope west (at current rate)     :Mw#  Reply: [none]
Move telescope north (at current rate)    :Mn#  Reply: [none]
Move telescope south (at current rate)    :Ms#  Reply: [none]
Set rate to Move                          :RM#  Reply: [none]
Set rate to Slew                          :RS#  Reply: [none]
Set rate to n (0-9)*3                     :Rn#  Reply: [none]
    *3 = Slew rates are as follows.
    All values are in multipules of the sidereal rate:
    R0=0.25X, R1=0.5X, R2(RG)=1X, R3=2X, R4(RC)=4X, R5=8X(RM), R6=16X, R7(RS)=24X, R8=40X, R9=60X
    (for the -Dev-Alpha branch of OnStep:
    R0=0.25X, R1=0.5X, R2(RG)=1X, R3=2X, R4=4X, R5(RC)=8X, R6(RM)=24X, R7=48X, R8(RS)=1/2 MaxRate, R9=MaxRate)
Tracking enable                           :Te#  Reply: 0 or 1
Tracking disable                          :Td#  Reply: 0 or 1
Sync. with current target RA/Dec          :CS#  Reply: [none]
Sync. with current target RA/Dec          :CM#  Reply: N/A
 */

/*
 * INCLUDES
 */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/*
 * Debug output functionality
 */
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

/* OLED screen definitions */
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* PINMAP */
#define PINMAP_PCB_R2

#ifdef PINMAP_PCB_R2
  #define PIN_UP      D1
  #define PIN_DOWN    D6
  #define PIN_LEFT    D7
  #define PIN_RIGHT   D0
  #define PIN_SPECIAL RX
  #define LED_RED     D4
#endif

/* Network credentials (OnStep defaults are "ONSTEP" and "password") */
const char* ssid     = "ONSTEP";
const char* password = "password";
IPAddress onstep(192,168,0,1);
WiFiClient cmdSvrClient;

/* Focuser speed settings */
int focus_smallstep = 25;
int focus_largestep = 100;
int focus_delay = 100;

/* Initialize states for buttons */
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
// main program loop
// * * * * * * * * * * * * * * * * * * * * *
void updatedisplay() {
  display.clearDisplay();       // Clear the buffer
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  String RA, DE, FO;

  RA = processCommand(":GR#");
  DE = processCommand(":GD#");
  FO = processCommand(":FG#");
  
  display.print("R "); display.print(RA); display.print("\n");
  display.print("D "); display.print(DE); display.print("\n");
  display.print("F "); display.print(FO);
  display.display();
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

  /* initialize OLED display */
  Wire.begin(D2, D5);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    DL("SSD1306 allocation failed");
  }
  display.clearDisplay();       // Clear the buffer
  display.display();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  //display.print("R 12:34:10\n");
  //display.print("D 65:45:21\n");
  //display.print("F 21350");
  //display.display();
  
  /* disable LED on at startup (LED_RED and BUILTIN_LED currently on D4 */
  digitalWrite(LED_RED, HIGH);
  //analogWrite(LED_RED, 1023);

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
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Connecting to "); display.print(ssid); display.print("\n");
    display.print(i);
    display.display();
  }
  DL("");
  DL("Connection established!");  
  D("IP address:\t");
  DL(WiFi.localIP());

  // Check response from ":GVP#" - has to be "On-Step#"
  if (processCommand(":GVP#") == "On-Step#") {
    DL("Successfully found OnStep command server.");
    //digitalWrite(LED_RED, HIGH);
    display.print(" - OnStep found.");
    display.display();
  }
  else {
    DL("Did not find OnStep command server.");
    // add led blink code in when PCB with leds is available
    display.print(" - OnStep NOT found.");
    display.display();
    for(;;); // Don't proceed, loop forever
  }

  // set first value for readout of buttons, otherwise it is undefined
  lastReadout_time = millis();

  // wait for 3s, then continue
  delay(3000);

  // clear display
  display.clearDisplay();
  display.display();
}

// * * * * * * * * * * * * * * * * * * * * *
// main program loop
// * * * * * * * * * * * * * * * * * * * * *
void loop() {
  String cmd;
  String cmd_result;

  // get start time for loop
  unsigned long start_time = millis();
  
  // poll state for all buttons
  readPins();

  /*
   * Action:  Move Focus Inward
   * Trigger: UP Button
   */
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
   * Action:  Move Focus Outward
   * Trigger: DOWN Button
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

  // HERE COME NOT IMPLEMENTED COMMANDS
  
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

  /*
   * update OLED display
   */
  updatedisplay();

  // ensure constant execution time (not needed now that button state has duration associated)
  /*unsigned long end_time = millis();
  unsigned long run_time = end_time - start_time;
  if ( run_time < 250) {
    D("Loop time: ");
    D(run_time);
    DL("ms");
    delay(250-run_time);
  }*/
}
