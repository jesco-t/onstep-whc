
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
void char2RA(char* txt, unsigned int& hour, unsigned int& minute, unsigned int& second) {
  char* pEnd;
  hour = (int)strtol(&txt[0], &pEnd, 10);
  minute = (int)strtol(&txt[3], &pEnd, 10);
  second = (int)strtol(&txt[6], &pEnd, 10);
}

void char2DEC(char* txt, int& deg, unsigned int& min, unsigned int& sec) {
  char* pEnd;
  deg = (int)strtol(&txt[0], &pEnd, 10);
  min = (int)strtol(&txt[4], &pEnd, 10);
  sec = (int)strtol(&txt[7], &pEnd, 10);
}
*/

/*
 * INCLUDE COMMON HEADERS AND DEFINITIONS
 */
//#define DEBUG
#include "Common.h"
#include "Config.h"

/* TASK SCHEDULER */
Scheduler ts;
bool InputIsProcessed = false;

void updateUI();
Task tsDisplay ( 500 * TASK_MILLISECOND, TASK_FOREVER, &updateUI, &ts, true );
void readInput();
Task tsInput ( 100 * TASK_MILLISECOND, TASK_FOREVER, &readInput, &ts, true );
void processInput();
Task tsProcess ( 333 * TASK_MILLISECOND, TASK_FOREVER, &processInput, &ts, true );

/* OLED screen definitions */
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* Initialize states for buttons */
int pinUp_status = 0;
int pinDown_status = 0;
int pinLeft_status = 0;
int pinRight_status = 0;
int pinUp_duration = 0;
int pinDown_duration = 0;
#ifndef DISABLE_SPECIAL 
int pinSpecial_duration = 0; 
int pinSpecial_status = 0; 
#endif
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
void readInput() {
  DL("Input...");
  // Save button status from last call
  int pinUp_prevstatus = pinUp_status;
  int pinDown_prevstatus = pinDown_status;
#ifndef DISABLE_SPECIAL 
  int pinSpecial_prevstatus = pinSpecial_status; 
#endif
  int pinLeft_prevstatus = pinLeft_status;
  int pinRight_prevstatus = pinRight_status;

  // save current time as helper to determine how long buttons are pushed 
  //(consider first run of this function when lastReadout_time is 0)
  Readout_time = millis();
  if (lastReadout_time == 0) {
    lastReadout_time = Readout_time;
  }

  // read buttons
  pinUp_status = digitalRead(PIN_UP);
  pinDown_status = digitalRead(PIN_DOWN);
#ifndef DISABLE_SPECIAL 
  pinSpecial_status = digitalRead(PIN_SPECIAL); 
#endif
  pinLeft_status = digitalRead(PIN_LEFT);
  pinRight_status = digitalRead(PIN_RIGHT);

  // Determine push time, if button was previously pushed
  if (pinUp_status == 0 && pinUp_prevstatus == 1 && InputIsProcessed == false){
    pinUp_status = 1;
  }
  if (pinUp_status == 1) {
    display.print("U");
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

  if (pinDown_status == 0 && pinDown_prevstatus == 1 && InputIsProcessed == false){
    pinDown_status = 1;
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

  if (pinLeft_status == 0 && pinLeft_prevstatus == 1 && InputIsProcessed == false){
    pinLeft_status = 1;
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

  if (pinRight_status == 0 && pinRight_prevstatus == 1 && InputIsProcessed == false){
    pinRight_status = 1;
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

  #ifndef DISABLE_SPECIAL
  if (pinSpecial_status == 0 && pinSpecial_prevstatus == 1 && InputIsProcessed == false){
    pinSpecial_status = 1;
  }
  if (pinSpecial_status == 1) {
    display.print("S");
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
  #endif
   
  // debug output
  /*D("pinUp_status: "); D(pinUp_status); D(" / pressed time: "); DL(pinUp_duration);
  D("pinDown_status: "); D(pinDown_status); D(" / pressed time: "); DL(pinDown_duration);
  D("pinSpecial_status: "); D(pinSpecial_status); D(" / pressed time: "); DL(pinSpecial_duration);
  D("pinLeft_status: "); D(pinLeft_status);  D(" / pressed time: "); DL(pinLeft_duration);
  D("pinRight_status: "); D(pinRight_status); D(" / pressed time: "); DL(pinRight_duration);*/

  // clean up and save last button readout time
  lastReadout_time = Readout_time;
  if (InputIsProcessed == true){
    InputIsProcessed = false;
  }
}

// * * * * * * * * * * * * * * * * * * * * *
// display routines
// * * * * * * * * * * * * * * * * * * * * *
void updateUI() {
  DL("User Interface...");
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
  //display.print("Buttons "); display.print(pinUp_status); display.print(pinDown_status); display.print(pinLeft_status); display.print(pinRight_status); display.print(pinSpecial_status);
  display.display();
}

void processInput(){
  String cmd;
  String cmd_result;

  /*
   * Action:  Move Focus Inward
   * Trigger: UP Button
   */
  if (pinUp_status == 1 && pinDown_status == 0){
    cmd = ":FR" + String(focus_slowspeed);
    if (pinUp_duration > focus_switchtime ) {
      cmd = ":FR" + String(focus_fastspeed);
    }
    cmd = cmd + "#";
    DL(cmd);
    cmd_result = processCommand(cmd);
  }
  
  /*
   * Action:  Move Focus Outward
   * Trigger: DOWN Button
   */
  if (pinUp_status == 0 && pinDown_status == 1){
    cmd = ":FR-" + String(focus_slowspeed);
    if (pinDown_duration > focus_switchtime ) {
      cmd = ":FR-" + String(focus_fastspeed);
    }
    cmd = cmd + "#";
    DL(cmd);
    cmd_result = processCommand(cmd);
  }

  // HERE COME NOT IMPLEMENTED COMMANDS
  
  /*
   * set current focuser position as new home
   */
  /*if (pinUp_status == 0 && pinDown_status == 0){
    delay(1000); // only issue command if button is pressed for more than one second
    readInput();
    if (pinUp_status == 0 && pinDown_status == 0){
      cmd = ":FH#";
      cmd_result = processCommand(cmd);
    }
  }*/
  /*
   * move focuser to home position
   */
  /*if (pinUp_status == 1 && pinDown_status == 1){
    delay(1000); // only issue command if button is pressed for more than one second
      readInput();
    if (pinUp_status == 1 && pinDown_status == 1){
      cmd = ":Fh#";
      cmd_result = processCommand(cmd);
    }
  }*/
  InputIsProcessed = true;
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
  display.dim(true);
  
  /* disable LED on at startup (LED_RED and BUILTIN_LED currently on D4 */
  digitalWrite(LED_RED, HIGH);
  //analogWrite(LED_RED, 1023);

#ifdef DEBUG
  DebugSer.begin(9600); 
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
    yield();
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
    for(;;)
        yield(); // Don't proceed, loop forever
  }

  // set first value for readout of buttons, otherwise it is undefined
  lastReadout_time = millis();

  // wait, then continue
  delay(1000);

  // clear display
  display.clearDisplay();
}

// * * * * * * * * * * * * * * * * * * * * *
// main program loop
// * * * * * * * * * * * * * * * * * * * * *
void loop() {
  /*
   * run scheduler
   */
  DL("Running scheduler...");
  ts.execute();
}
