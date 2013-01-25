#include <SPI.h>
#include <WiFi.h>
#include <SD.h>
//#include <Time.h>
#include "DualMC33926MotorShield.h"
#include "keys.h"

// Every second we run something
#define CRON 1000

// Heartbeat
#define HEARTBEAT 250

// Disconnect client after our timeout
#define CLIENTDEAD 15000

// Buffer for our logging function
#define BUFLEN 64

// Restart server interval?
#define SERVER 30000

// Length of the client mode command
#define CMDLEN 10

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
const char* ramString = "Free SRAM: %d\r\n";
const char* hello = "Hello!\r\n";
const char* signalString = "Signal: %ld dBm\r\n";
const char* serverString = "Server status: %d\r\n";
const char* clientString = "CLIENT STATUS: %d\r\n";
const char* bye = "Bye!\r\n";

boolean alreadyConnected = false;
byte mac[6];
unsigned long lastc = millis();
unsigned long lastcron = millis();
unsigned long lastbeat = millis();
unsigned long lastserver = millis();
int m1MaxCurrent = 0;
int m2MaxCurrent = 0;
int speed = 100;
// Mode is (S)ocket or (C)lient
char mode = 'S';
// [+/-]123\0[+/-]321\n
char cmdC[CMDLEN] = {};
char cbuf[BUFLEN] = {};
int bAvail = 0;
int bRead = 0;

int status = WL_IDLE_STATUS;
// How many milliseconds do we drive for in between commands? 2/3 of a second naturally. This is a good balance between network latency, tcp/ip buffering (even with TCP_NODELAY), and natural controls.
int thrust_drivetime = 666;
int yaw_drivetime = 666;
int drivetime = 666;
//char beat = '0';


WiFiClient client;        
//WiFiClient client2;        
WiFiServer server(8888);

File logFile;

void logger(const char *fmt, ...) {
  char buffer[BUFLEN];
  char timebuffer[BUFLEN];
  memset(buffer, '\0', BUFLEN);
  memset(timebuffer, '\0', BUFLEN);
  va_list args;
  va_start (args, fmt);
  vsnprintf(buffer, BUFLEN-1, fmt, args);
  va_end (args);

  // timestamp
  snprintf(timebuffer, BUFLEN-1, "%ld - %s", millis(), buffer);

  Serial.print(timebuffer);

  if (logFile) {
    logFile.print(timebuffer);
    logFile.flush();
  } else {
    Serial.println("Log error!");
  }   
}

void setup() {
  // Heartbeat if a client is connected (serial doesn't need to receive anything during run)
//  pinMode(0, OUTPUT);
//  digitalWrite(0, LOW);
  
  //Initialize serial and wait for port to open:
  Serial.begin(115200); 
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

  // RAM free?
  logger(ramString, freeRam());
  
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
  
  // Sync the time - This won't work as the Arduino wifi shield doesn't
  // support making a client connection at the same time as running a server. One negates
  // the other for some reason.
/*
  if (client.connect("theendless.org", 80)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    client.println("GET /time.php HTTP/1.1");
    client.println("Host:theendless.org");
    client.println("Connection: close");
    client.println();
  }

  if (client.connected()) {
    client.flush();
    client.stop();
  }
  client = NULL;

  char response[BUFLEN];
  int pos = 0;
  time_t nowTime = 0;
  // Wait 5 seconds
  delay(5000);
  while (timeC.available()) {   
    char c = timeC.read();
    Serial.print(c);
    
    response[pos] = c;
    if ( c == '\n' ) {
      response[pos] = NULL;
      // Now we have a full line, let's look for our TIME
      if (strstr(response, "TIME:") != NULL) {
        nowTime = (time_t)atol(&response[5]);
        break;
      } else {
        pos = 0;
      }
    }
  }
  // Disconnect
  timeC.stop();
  
  Serial.println("nowTime: " + String(nowTime));
  
  // If we got a time, let's sync the Arduino
  if (nowTime != 0) {
    setTime(nowTime);
  }
*/

  // Start server
  server.begin();
}

void loop() {
  // Any clients?
  client = server.available();
    
  // Reset already connected if someone dropped
  if (((alreadyConnected == true) && !client) || 
      ((alreadyConnected == true) && client && client.status() != 4)) {
    alreadyConnected = false;
  }

  // Heartbeat (Every quarter second)  
/*
  if ( (millis() - lastbeat) > HEARTBEAT ) {
    if (alreadyConnected == true) {
      switch (beat) {
        case '0':
          digitalWrite(0, HIGH);
          beat = '1';
          break;
        case '1':
          digitalWrite(0, LOW);
          beat = '0';
          break;
      }
    }
  }
*/  
  // Jobs to run every second
  if ( (millis() - lastcron) > CRON) {
    logger(ramString, freeRam());
    printWifiStatus();
    int m1 = md.getM1CurrentMilliamps();
    int m2 = md.getM2CurrentMilliamps();
    if ( m1 > m1MaxCurrent) {
      m1MaxCurrent = m1;
    }
    if (m2 > m2MaxCurrent) {
      m2MaxCurrent = m2;
    }
    logger("m1 current (mA)/max: %d / %d\r\n", m1, m1MaxCurrent);
    logger("m2 current (mA)/max: %d / %d\r\n", m2, m2MaxCurrent);

    if (client) {
      logger(clientString, client.status());
    }
    lastcron = millis();
    
    // Server alive?
    if (server.status() != 1) {
      logger("Server dead: %d\r\n", server.status());      
      killClient();
      return;
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
      lastc = millis();
      server.write(hello);
      // reset everything
      md.setSpeeds(0,0);
      speed = 100;
      memset(cmdC, NULL, 10);
      alreadyConnected = true;
      mode = 'S';
      client.flush();
      logger(hello);
    }

    // We can switch to "C"lient mode from "S"ocket mode, but not back, unless you reconnect.
    if ((mode == 'S') && (client.available())) {
      char c = client.read();

      // Debugging - not needed, maybe for client mode.
      /*
      if ( c == NULL ) {
        logger("'c: NULL'\r\n", c);
      } else if ( c == '\n' ) {
        logger("'c: \\n'\r\n", c);        
      } else {
        logger("'c: %c'\r\n", c);
      }
      */

      switch(c) {
        case 'C':
          mode = 'C';
          logger("Changed to client mode\r\n");
          server.write("C\r\n");
          drivetime = 666;
          return;
          break;
        case disconnect:
          killClient();
          return;        
          break;
        case forwardk:
        case forwardk2:
          drivetime = thrust_drivetime;
          md.setSpeeds(-speed,-speed);          
          break;
        case reversek:
        case reversek2:
          drivetime = thrust_drivetime;
          // Speed going in reverse is always 100 - Prevent wheelies
          md.setSpeeds(100,100);
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
      }
      // Time of last command
      lastc = millis();
      // It's useful for the client to know when
      // an action has executed.
      server.write(c);
      logger("'%c'\r\n", c);
    }

    // Client mode - Independent control of motors
    if ((mode == 'C') && (bAvail = client.available())) {      
      // WifiShield libs/firmware only ever return 1 byte if any data is available.
      // This is very frustrating and shortsighted. Issue has been submitted. How do
      // we compensate?    

      // Debugging
      logger("Bytes Available: %d\r\n", bAvail);
      // OK, we probably have at least a command, we only care about the last full command.
      // Defaults to 1000ms timeout waiting on read buffer
      client.setTimeout(10);
      while (bAvail = client.available()) {
        bRead = client.readBytesUntil('\n', cmdC, 10);
        logger("Bytes Read: %d %s %s\r\n", bRead, cmdC, cmdC+5);
        if ((bRead == 9)) {
          logger("Good Command\r\n");
          // Most likely a good command
          cmdC[9] = NULL;
          // Disconnect?
          if (cmdC[0] == disconnect) {
            killClient();
            return;
          }
          if ((millis() - lastc) > 5) {
            break;
          }
        } else {
          // else, command is screwed or not all there, read again if there is at least CMDLen left.
          // We're not super-concerned about partial reads. Best be fast and dirty, rather than picky
          // as so many commands are coming in when in motion.
          memset(cmdC, NULL, 10);
        }
        //bAvail -= bRead;
      }
            
      if (bRead) {        
        int m1speed = atoi(cmdC);
        int m2speed = atoi(cmdC+5);
        if (m1speed >= 200) {
          m1speed = 200;
        }
        if (m1speed <= -200) {
          m1speed = -200;
        }
        if (m2speed >= 200) {
          m2speed = 200;
        }
        if (m2speed <= -200) {
          m2speed = -200;
        }
          
        // Motors wired in reverse (compensate here)
        md.setSpeeds(-m1speed, -m2speed);
        client.flush();
        // reset command
        memset(cmdC, NULL, 10);
  
        // Time of last command
        lastc = millis();
        server.write("C\r\n");
        logger("cmd: %d|%d\r\n", m1speed, m2speed);
      }
    }    

    // Stop the cart after no command received for drivetime
    if (millis() - lastc > drivetime) {
      md.setSpeeds(0,0);
    }
      
    // Disconnect client if no data comes across in CLIENTDEAD ms
    if (millis() - lastc > CLIENTDEAD) {
      killClient();
    }          
  } else { 
    // Safety - client disappears, robot stops. This prevents the robot from trying to kill Anarosa.
    killClient();
  }
}

void killClient() {
  md.setSpeeds(0,0);
  if (client) { 
    logger(bye);
    server.write(bye);
    client.flush();
    client.stop();
  }
  alreadyConnected == false;
  // Heart stopped
  //digiitalWrite(0, LOW);
}
  
void connectWifi() {
  while (status != WL_CONNECTED) { 
    logger("Attempting to connect to SSID: %s\r\n", ssid);
    status = WiFi.begin(ssid, pass);
    if (status != WL_CONNECTED) { 
      logger("Couldn't connect via WiFi, retrying\r\n");
    } else {
      printWifiStatus();
    }
  } 
}

void printWifiStatus() {
  // Firmware version?
  // Serial.println("Firmware Version: " + String(WiFi.firmwareVersion()));
  
  // How's our power source doing?
  logger("Battery voltage: %ld\r\n", readVcc());
  
  // print the SSID of the network you're attached to:
  logger("SSID: %s\r\n", WiFi.SSID());
  
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  logger("IP Address: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);

  // print the received signal strength:
  logger(signalString, WiFi.RSSI());
  logger(serverString, server.status());
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void stopIfFault()
{
  if (md.getFault())
  {
    logger("Motor fault\r\n");
    while(1);
  }
}

// Very roughly check on the battery =D
long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  
 
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
