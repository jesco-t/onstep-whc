
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

// Enable debugging messages via the serial monitor
#define DEBUG ON
#define DebugSer Serial

#if DEBUG != OFF
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

void setup() {

#if DEBUG != OFF
  // initialize USB serial debugging early, so we can use DebugSer.print() for debugging, if needed
  DebugSer.begin(9600); 
  delay(5000); 
  DebugSer.flush(); 
  DF(""); 
  DF(""); 
  DL("Starting onstep-fhc...");
#endif

  // Connect to controller WiFi
  WiFi.begin(ssid, password);             
  DL("Connecting to "); 
  D(ssid); DL(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    D(++i); D(' ');
  }

  DL("");
  DL("Connection established!");  
  D("IP address:\t");
  DL(WiFi.localIP());                     // Send the IP address of the ESP8266 to the computer
}

void loop() {
  // put your main code here, to run repeatedly:

}
