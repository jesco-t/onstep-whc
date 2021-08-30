
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
#define DEBUG
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
String pin_status = "00000"; // North East South West Special
String pin_prevstatus = "00000";
unsigned long lastReadout_time = 0;
unsigned long Readout_time = 0;

/* Operation Modes ("F" or "S") */
String OpMode = "F";

// * * * * * * * * * * * * * * * * * * * * *
// Command communication with OnStep server
// * * * * * * * * * * * * * * * * * * * * *
String processCommand(String cmd){
  // Debugging output
  //D("Command: "); D(cmd); D(" - ");
  
  // Connect to cmdSvr
  if (!cmdSvrClient.connect(onstep, 9999)) {
    //DL("connection failed");
    delay(1000);
    return "NO_CONNECTION";
  }
  // Send command
  if (cmdSvrClient.connected()) {
    cmdSvrClient.println(cmd);
    //D(cmd); DL(" successfully sent - ");
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
    //D(ch);
    data_return += ch;
  }
  // Close the connection
  cmdSvrClient.stop();
  //DL();

  return data_return;
}

// * * * * * * * * * * * * * * * * * * * * *
// Read button status
// * * * * * * * * * * * * * * * * * * * * *
void readInput() {
  DL("Input...");

  // save current time as helper to determine how long buttons are pushed 
  //(consider first run of this function when lastReadout_time is 0)
  Readout_time = millis();
  if (lastReadout_time == 0) {
    lastReadout_time = Readout_time;
  }

  // read buttons (alow change of state only if previous button input has been processed)
  pin_prevstatus = pin_status;
  if (InputIsProcessed == true) {
    pin_status = String(max(digitalRead(PIN_UP),0));
    pin_status += String(max(digitalRead(PIN_RIGHT),0));
    pin_status += String(max(digitalRead(PIN_DOWN),0));
    pin_status += String(max(digitalRead(PIN_LEFT),0));
#ifndef DISABLE_SPECIAL
    pin_status += String(max(digitalRead(PIN_SPECIAL),0));
#endif
  } else {
    pin_status = String(max(digitalRead(PIN_UP),atoi(&pin_prevstatus[0])));
    pin_status += String(max(digitalRead(PIN_RIGHT),atoi(&pin_prevstatus[1])));
    pin_status += String(max(digitalRead(PIN_DOWN),atoi(&pin_prevstatus[2])));
    pin_status += String(max(digitalRead(PIN_LEFT),atoi(&pin_prevstatus[3])));
#ifndef DISABLE_SPECIAL
    pin_status += String(max(digitalRead(PIN_SPECIAL),atoi(&pin_prevstatus[4])));
#endif
  }

  // Determine push time, if button was previously pushed
  if (String(pin_status[0]).toInt() == 1 && String(pin_prevstatus[0]).toInt() == 1) {
    pinUp_duration = pinUp_duration + (Readout_time - lastReadout_time);
  } else {
    pinUp_duration = 0;
  }
  if (String(pin_status[1]).toInt() == 1 && String(pin_prevstatus[1]).toInt() == 1) {
    pinRight_duration = pinRight_duration + (Readout_time - lastReadout_time);
  } else {
    pinRight_duration = 0;
  }
  if (String(pin_status[2]).toInt() == 1 && String(pin_prevstatus[2]).toInt() == 1) {
    pinDown_duration = pinDown_duration + (Readout_time - lastReadout_time);
  } else {
    pinDown_duration = 0;
  }
  if (String(pin_status[3]).toInt() == 1 && String(pin_prevstatus[3]).toInt() == 1) {
    pinLeft_duration = pinLeft_duration + (Readout_time - lastReadout_time);
  } else {
    pinLeft_duration = 0;
  }
 
  #ifndef DISABLE_SPECIAL
  if (String(pin_status[4]).toInt() == 1 && String(pin_prevstatus[4]) == 1) {
    pinSpecial_duration = pinSpecial_duration + (Readout_time - lastReadout_time);
  } else {
    pinSpecial_duration = 0;
  }
  #endif

  // clean up and save last button readout time
  lastReadout_time = Readout_time;
  InputIsProcessed = false;
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
  //display.print("Buttons "); display.print(pinUp_status); display.print(pinDown_status); display.print(pinLeft_status); display.print(pinRight_status);
  
  // display OpMode
  display.setCursor(96, 0);
  if (OpMode == "F") {
    display.print("FOCUS");
  }
  if (OpMode == "S") {
    display.print("SCOPE");
  }
  
  display.display();
}

void processInput(){
  DL("Processing...");
  String cmd;
  String cmd_result;

  // temporary conversion of button state to old format
  pinUp_status = String(pin_status[0]).toInt();
  pinRight_status = String(pin_status[1]).toInt();
  pinDown_status = String(pin_status[2]).toInt();
  pinLeft_status = String(pin_status[3]).toInt();

  /*
   * Focuser Mode
   */
  if( OpMode == "F") {
    /*
     * Action:  Move Focus Inward
     * Trigger: UP Button
     */
    if (pinUp_status == 1 && pinDown_status == 0){
    
      if (pinUp_duration > focus_switchtime ) {
        cmd = ":FR" + String(focus_fastspeed) + "#";
        cmd_result = processCommand(cmd);
      } else {
        cmd = ":FR" + String(focus_slowspeed) + "#";
        cmd_result = processCommand(cmd);
      }
    }
  
    /*
     * Action:  Move Focus Outward
     * Trigger: DOWN Button
     */
    if (pinUp_status == 0 && pinDown_status == 1){
      if (pinDown_duration > focus_switchtime ) {
        cmd = ":FR-" + String(focus_fastspeed) + "#";
        cmd_result = processCommand(cmd);
      } else {
        cmd = ":FR-" + String(focus_slowspeed) + "#";
        cmd_result = processCommand(cmd);
      }    
    }

    /*
     * ACTION: set current focuser position as new home
     * TRIGGER: UP and DOWN Buttons pressed for more than 1s
     */
    if (pinUp_status == 1 && pinDown_status == 1){
      if (pinUp_duration > 1000 && pinDown_duration > 1000){
        cmd_result = processCommand(":FH#");
      }
    }

    /*
     * ACTION: move focuser to home position
     * TRIGGER: LEFT and RIGHT Buttons pressed for more than 1s
     */
    if (pinLeft_status == 1 && pinRight_status == 1){
      if (pinLeft_duration > 1000 && pinRight_duration > 1000){
        cmd_result = processCommand(":Fh#");
      }
    }

    /*
     * ACTION: change to scope control mode
     * TRIGGER: UP and RIGHT Buttons pressed for more than 1s
     */
    if (pinUp_status == 1 && pinRight_status == 1){
      if (pinUp_duration > 500 && pinRight_duration > 500){
        OpMode = "S";
      }
    }
  }
  /*
   * Scope Mode
   */
  if( OpMode == "S" && InputIsProcessed == false) {
    /*
     * ACTION: change to focus control mode
     * TRIGGER: DOWN and LEFT Buttons pressed for more than 1s
     */
    if (pinDown_status == 1 && pinLeft_status == 1){
      if (pinDown_duration > 500 && pinLeft_duration > 500){
        OpMode = "F";
      }
    }
    /*
     * ACTION: move scope north/south/west/east
     * TRIGGER: UP, DOWN, LEFT, RIGHT Buttons
     */
    if (pinUp_status == 1){
      cmd_result = processCommand(":Mn#");
    }
    if (pinDown_status == 1){
      cmd_result = processCommand(":Ms#");
    }
    if (pinLeft_status == 1){
      cmd_result = processCommand(":Mw#");
    }
    if (pinRight_status == 1){
      cmd_result = processCommand(":Me#");
    }
  }
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

  // enable tracking
  processCommand(":Te#");
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
