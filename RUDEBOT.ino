#include <SPI.h>
#include <WiFi.h>
#include <SD.h>
#include "DualMC33926MotorShield.h"
#include "keys.h"

// Every 60 seconds shoot across a keepalive
#define KEEPALIVE 60000

// Every second we run something
#define CRON 1000

// Buffer for our logging function
#define MAXLOG 64

// Restart server interval?
#define SERVER 30000

DualMC33926MotorShield md;

// Where's the SD card?
const int sdSelect = 4;

const char forwardk = 'k';
const char reversek = 'j';
const char leftk = 'h';
const char rightk = 'l';

const char forwardk2 = 'w';
const char reversek2 = 's';
const char leftk2 = 'a';
const char rightk2 = 'd';

const char stop = ' ';
const char disconnect = '\\';

// String table to save memory
const char* oneConn = "Sorry, one at a time\r\n";
const char* hello = "Hello!\r\n";
const char* signalString = "Signal: %ld dBm\r\n";
const char* serverString = "Server status: %d\r\n";
const char* clientString = "Client status: %d\r\n";
const char* bye = "Bye!\r\n";

boolean alreadyConnected = false;
byte mac[6];
WiFiClient organic = NULL;
unsigned long keepalive = millis();
unsigned long lastc = millis();
unsigned long lastcron = millis();
unsigned long lastserver = millis();
int speed = 100;
int status = WL_IDLE_STATUS;
// How many milliseconds do we drive for in between commands?
int thrust_drivetime = 666;
int yaw_drivetime = 666;
int drivetime = 666;
        
WiFiServer server(8888);
WiFiClient client = NULL;
File logFile;

void logger(const char *fmt, ...) {
  char buffer[MAXLOG];
  memset(buffer, '\0', MAXLOG);
  va_list args;
  va_start (args, fmt );
  vsnprintf(buffer, MAXLOG, fmt, args);
  va_end (args);

  Serial.print(buffer);

  if (logFile) {
    logFile.print(buffer);
    logFile.flush();
  } else {
    Serial.println("Log error!");
  }   
}

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600); 
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  // Initialize motors
  md.init();

  // Initialize SD card
  if (!SD.begin(sdSelect)) {
    Serial.print("Card failed or not present\r\n");
  } else {
    Serial.print("SD initialized\r\n");
  }
  logFile = SD.open("RUDEBOT.log", FILE_WRITE);
  
  if ((status = WiFi.status()) == WL_NO_SHIELD) {
    logger("WiFi shield not present\r\n"); 
    // don't continue:
    while(true);
  } 

  // Spit out MAC 
  /*
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  Serial.print(mac[5],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.println(mac[0],HEX);
  */
  
  // Connect
  connectWifi();
}

void loop() {
  // listen for incoming clients
  client = server.available();

  // Reset already connected if someone dropped
  if (((alreadyConnected == true) && !client) || 
      ((alreadyConnected == true) && client && client.status() != 4)) {
    alreadyConnected = false;
  }
    
  // Jobs to run every second
  if ( (millis() - lastcron) > CRON) {
    logger(signalString, WiFi.RSSI());
    logger(serverString, server.status());
    if (client) {
      logger(clientString, client.status());
    }
    lastcron = millis();
    
    // Server alive?
    if (server.status() != 1) {
      logger("Server dead: %d\r\n", server.status());      
      if (client) {
        killClient();
        return;
      }
      md.setSpeeds(0,0);
    } else {
      // Log it if it's been a while since we were alive
      if ( (millis()-lastserver) > 5000 ) {
        logger("Server dead for %d seconds\r\n", (millis()-lastserver)/1000);
      }
      lastserver = millis();
    }
  }

  int newstatus = WiFi.status();
  if (status != newstatus)
  {
    logger("WiFi status changed to: %d\r\n", newstatus);
    status = newstatus;
  }

  if (client) {
    // New clients
    if (!alreadyConnected) {
      logger(hello);
      server.write(hello);
      alreadyConnected = true;
      client.flush();
    }

    if (client.available()) {
      char c = client.read();
      
      switch (c) {
        case forwardk:
        case forwardk2:
          drivetime = thrust_drivetime;
          md.setSpeeds(-speed,-speed);          
          break;
        case reversek:
        case reversek2:
          drivetime = thrust_drivetime;
          md.setSpeeds(speed,speed);
          break;
        case leftk:
        case leftk2:
          drivetime = yaw_drivetime;
          md.setSpeeds(-speed,speed);
          break;
        case rightk:
        case rightk2:
          drivetime = yaw_drivetime;
          md.setSpeeds(speed,-speed);
          break;
        case stop:
          md.setSpeeds(0,0);
          break;
        case '1':
          speed = 100;
          thrust_drivetime = 600;
          yaw_drivetime = 300;
          break;
        case '2':
          speed = 133;
          thrust_drivetime = 533;
          yaw_drivetime = 266;
          break;
        case '3':
          speed = 166;
          thrust_drivetime = 466;
          yaw_drivetime = 233;
          break;
        case '4':
          speed = 200;
          thrust_drivetime = 400;
          yaw_drivetime = 200;
          break;
        case disconnect:
          killClient();
          break;
        
      } 
      // Time of last command
      lastc = millis();

      // If the server doesn't send feedback, the wifi library disconnects. WTF!?
      // I'd like to remove this, but it could be useful for the client to know when
      // an action has executed.
      server.write(c);
    } else { 
      if (millis() - lastc > drivetime) {
        md.setSpeeds(0,0);
      }
      // keepalive (annoying!!)
      if ( (millis() - keepalive) > KEEPALIVE) {
        if (client)
          client.flush();
        server.write(".\r\n");
        Serial.print("Ping!\r\n");
        keepalive = millis();
      }
    }
  } 
}

void killClient() {
  logger(bye);
  server.write(bye);
  client.flush();
  client.stop();
  alreadyConnected == false;
}
  
void connectWifi() {
  while (status != WL_CONNECTED) { 
    logger("Attempting to connect to SSID: %s\r\n", ssid);
    status = WiFi.begin(ssid, pass);
    if (status != WL_CONNECTED) { 
      logger("Couldn't connect via WiFi, retrying\r\n");
    } else {
      server.begin();
      printWifiStatus();
    }
  } 
}

void printWifiStatus() {
  // Firmware version?
  // Serial.println("Firmware Version: " + String(WiFi.firmwareVersion()));
  
  // print the SSID of the network you're attached to:
  logger("SSID: %s\r\n", WiFi.SSID());
  
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  logger("IP Address: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);

  // print the received signal strength:
  logger(signalString, WiFi.RSSI());
  logger(serverString, server.status());
}

void stopIfFault()
{
  if (md.getFault())
  {
    logger("Motor fault\r\n");
    while(1);
  }
}
