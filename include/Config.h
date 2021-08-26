/* PINMAP */
#define PINMAP_PCB_R2
#include "PINMAP.h"

/* Network credentials (OnStep defaults are "ONSTEP" and "password") */
const char* ssid     = "ONSTEP";
const char* password = "password";
IPAddress onstep(192,168,0,1);
WiFiClient cmdSvrClient;

/* Focuser speed settings */
int focus_slowspeed = 25;       // slow speed step size
int focus_fastspeed = 250;      // fast speed step size
int focus_switchtime = 3000;    // time after switch speed is switched to fast
int focus_delay = 100;
