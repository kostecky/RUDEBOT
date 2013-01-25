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
  Serial.begin(115200);
  neck.attach(9);
}

void loop() 
{ 
  int pos = neck.read();
  char cmd = NULL;
  int newpos = 0;

  if ((bAvail = Serial.available()) && (bAvail >= CMDLEN))
  {
    Serial.println("pos: " + String(pos));
    Serial.println("Bytes avail: " + String(bAvail));
    bRead = Serial.readBytes(sbuf, bAvail);
    if (bRead > 0) {
      cmd = sbuf[bRead-1];
    } else {
      return;
    }

    switch(cmd) {
      case 'a':
        // Go left
        dir = -1;
        newpos = pos + (dir * ANGLE);
        break;
      case 'd':
        // Go right
        dir = 1;
        newpos = pos + (dir * ANGLE);
        break;
      case 's':
        // Go 90
        newpos = 90;
        if (pos < 90) {
          dir = 1;
        } else if ( pos > 90) {
          dir = -1;
        }
        break;
      default:
        return;
    }
    
    if (newpos < MAXLEFT) {
      newpos = MAXLEFT;
    }
    if (newpos > MAXRIGHT) {
      newpos = MAXRIGHT;
    }
    
    // Send command to servo
    Serial.println("newpos: " + String(newpos));
    for (int i = pos; (i != newpos) && (i >= MAXLEFT) && (i <= MAXRIGHT); i += dir) {
      Serial.println("neck: " + String(i));
      neck.write(i);
      delay(30);
    }
  }
}
