
/*
 * Title       onstep-fhc
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
 *   The FHC is a handcontroller to control *only* the focuser 1 module
 *   of the OnStep telescope controller by a wireless remote based on the
 *   ESP8266 WiFi board.
 *   
 *   The software is published here in the hope that it is helpful to someone.
 *
 * Author: Jesco Topp
 *   jesco.topp@gmail.com
 *
 * Revision history, and newer versions:
 *   See GitHub: https://github.com/jesco-t/onstep-fhc
 *
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

// Enable debugging messages via the serial monitor
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

// Network credentials (OnStep defaults are "ONSTEP" and "password"
const char* ssid     = "ONSTEP";
const char* password = "password";
IPAddress onstep(192,168,0,1);

WiFiClient cmdSvrClient;

// Focuser settings
int focus_smallstep = 10;
int focus_largestep = 250;
int focus_speed = focus_smallstep;

// processCommand with the OnStep server
String processCommand(String cmd){

  D("Command: ");
  D(cmd);
  D(" - ");
  // Connect to cmdSvr
  if (!cmdSvrClient.connect(onstep, 9999)) {
    DL("connection failed");
    delay(1000);
    return "NO_CONNECTION";
  }
  // Send command
  if (cmdSvrClient.connected()) {
    cmdSvrClient.println(cmd);
    DL("success - ");
  } 
  // wait for data return to be available
  unsigned long timeout = millis();
  while (cmdSvrClient.available() == 0) {
    if (millis() - timeout > 1000) {
      DL(">>> Client Timeout !");
      cmdSvrClient.stop();
      delay(10000);
      return "TIMEOUT";
    }
  }
  // Read return data
    while (cmdSvrClient.available()) {
    char ch = static_cast<char>(cmdSvrClient.read());
    D(ch);
  }
  // Close the connection
  cmdSvrClient.stop();
  DL();

  return "SUCCESS";
}

// Setup routine
void setup() {

  pinMode(D1, INPUT); // focus in
  pinMode(D2, INPUT); // focus out
  pinMode(D5, INPUT); // focus speed change

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

  // light up blue on-board LED when connected (only in debug mode)
#ifdef DEBUG
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
#endif

  DL("");
  DL("Connection established!");  
  D("IP address:\t");
  DL(WiFi.localIP());                    // Send the IP address of the ESP8266 to the computer

  // ToDo: validate that we're talking to OnStep
  // Check response from ":GVP#" - has to be "On-Step#" 
}

// main program loop
void loop() {
  // put your main code here, to run repeatedly:
  int D1_status = digitalRead(D1);
  int D2_status = digitalRead(D2);
  int D5_status = digitalRead(D5);

  //D("D1 Status: ");
  //DL(D1_status);

  String cmd_result;
  cmd_result = processCommand(":GVP#");

  delay(300);

}
