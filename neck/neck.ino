#include <Servo.h> 

#define MAX_CHAR 4
#define BUFLEN 64
#define CMDLEN 1
#define MAXLEFT 30
#define MAXRIGHT 150
#define ANGLE 5

Servo neck;                
int bAvail = 0;
int bRead = 0;
char sbuf[BUFLEN] = {};
int dir = 0;

void setup() 
{ 
  Serial.begin(9600);
  neck.attach(9);
}

void loop() 
{ 
  int pos = neck.read();
  char cmd = NULL;

  if ((bAvail = Serial.available()) && (bAvail >= CMDLEN))
  {
    bRead = Serial.readBytes(sbuf, bAvail);
    if (bRead > 0) {
      cmd = sbuf[bRead-1];
    }

    switch(cmd) {
      case 'a':
        // Go left
        dir = -1;
        break;
      case 'd':
        // Go right
        dir = 1;
        break;
      default:
        return;
    }
    
    // Send command
    int newpos = pos + (dir * ANGLE);
    for (int i = pos; (pos != newpos && (i > MAXLEFT) && (i < MAXRIGHT)); i += dir) {
      neck.write(i);
      delay(30);
    }
  }
}
