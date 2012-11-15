#include <SPI.h>
#include <Ethernet.h>
//#include <SD.h>
//#include <Time.h>
#include "DualMC33926MotorShield.h"

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
const char* ramString = "\nFree SRAM: %d\r\n";
const char* hello = "Hello!\r\n";
const char* clientString = "CLIENT CONNECTED\r\n";
const char* bye = "Bye!\r\n";

unsigned long lastc = millis();
unsigned long lastcron = millis();
unsigned long lastbeat = millis();
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

byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,20,128);
IPAddress gateway(192,168,20,1);
IPAddress subnet(255, 255, 255, 0);

// How many milliseconds do we drive for in between commands? 2/3 of a second naturally. This is a good balance between network latency, tcp/ip buffering (even with TCP_NODELAY), and natural controls.
int thrust_drivetime = 666;
int yaw_drivetime = 666;
int drivetime = 666;
//char beat = '0';

EthernetServer server(8888);
EthernetClient client;

//File logFile;

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

/*  if (logFile) {
    logFile.print(timebuffer);
    logFile.flush();
  } 
*/
  /*
  else {
    Serial.println("Log error!");
  } 
  */  
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
//  if (!SD.begin(sdSelect)) {
//    Serial.print("SD card failed or not present\r\n");
//  } else {
//    Serial.print("SD initialized\r\n");
//  }
//  logFile = SD.open("RUDEBOT.log", FILE_WRITE);

  // RAM free?
  logger(ramString, freeRam());
    
  // Connect
  connectEthernet();
  
  // Start server
  server.begin();
}

void loop() {
  // If we have a client, don't request a new one.
  // We only want one connection at a time

  if (!client.connected()) {
    //client.flush();
    //client.stop();
    //logger("Grabbing a new connection!\r\n");
    if (client = server.available()) {
      lastc = millis();
      server.write(hello);
      logger(hello);
      // reset everything
      md.setSpeeds(0,0);
      speed = 100;
      memset(cmdC, NULL, CMDLEN);
      mode = 'S';
    }
  } else {
    // Disconnect client if no data comes across in CLIENTDEAD ms
    if ((millis() - lastc) > CLIENTDEAD) {
      killClient();
    }
  }

  // Jobs to run every second
  if ((millis() - lastcron) > CRON) {
    //printStatus();
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

    logger("CLIENT STATUS: 0x%X %d\r\n", client.status(), client.connected());
    lastcron = millis();    
  }

  if (client) {
    // We can switch to "C"lient mode from "S"ocket mode, but not back, unless you reconnect.
    if ((mode == 'S') && (client.available() > 0)) {
      char c = client.read();
  
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
      // It's useful for the client to know when an action has executed.
      server.write(c);
      logger("'%c'\r\n", c);
    }

    // Client mode - Independent control of motors
    if ((mode == 'C') && (client.available() >= CMDLEN)) {      
      bAvail = client.available();
      logger("Bytes Available: %d\r\n", bAvail);
      // OK, we probably have at least a command, we only care about the last full command.
      // Defaults to 1000ms timeout waiting on read buffer
      client.setTimeout(10);
      while (bAvail >= CMDLEN) {
        bRead = client.readBytesUntil('\n', cmdC, CMDLEN);
        logger("Bytes Read/Avail: %d/%d %s %s\r\n", bRead, bAvail, cmdC, cmdC+5);
        if ((bRead == CMDLEN-1)) {
          logger("Good Command\r\n");
          // Most likely a good command
          cmdC[CMDLEN-1] = NULL;
          // Disconnect?
          if (cmdC[0] == disconnect) {
            killClient();
            return;
          }
          // We don't want to stay in this loop longer than 50% of drivetime!
          if ((millis() - lastc) > (0.1*drivetime)) {
            logger("Parsing commands for too long: %d ms\r\n", millis() - lastc);
            // I profiled the flush and it takes on the order of 400ms!!! with a full buffer.
            // It's quicker to read the commands in as they come and just chuck the ones first
            // in the queue.
            client.flush();
            logger("Flush complete\r\n");
            break;
          }
        } else {
          // else, command is screwed or not all there, read again if there is at least CMDLen left.
          // We're not super-concerned about partial reads. Best be fast and dirty, rather than picky
          memset(cmdC, NULL, CMDLEN);
          bAvail = client.available();
        }
        bAvail -= bRead;
      }
            
      if (bRead == CMDLEN-1) {        
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
        // reset command
        memset(cmdC, NULL, CMDLEN);
  
        // Time of last command
        lastc = millis();
        server.write("C\r\n");
        logger("cmd: %d|%d\r\n", m1speed, m2speed);
      }
    }     
  } 

  // Stop the cart after no command received for drivetime
  if ((millis() - lastc) > drivetime) {
    md.setSpeeds(0,0);
  }
}

void killClient() {
  md.setSpeeds(0,0);
  logger(bye);
  if (client.connected()) {
    server.write(bye);
    client.flush();
    logger("Flush complete\r\n");
    client.stop();
  }
  // Heart stopped
  //digiitalWrite(0, LOW);
}
  
void connectEthernet() {
  Ethernet.begin(mac, ip, gateway, subnet);
}

void printStatus() {
//  logger(ramString, freeRam());
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
