#include <Servo.h> 

#define MAX_CHAR 4

Servo servo;                

char angle[MAX_CHAR];
char* p = angle;
int alen = 0;

int myatoi(const char *s, int *value)
{
    if ( s != NULL && *s != '\0' && value != NULL )
    {
        char *endptr = (char*)s;
        *value = (int)strtol(s, &endptr, 10);
        if ( *endptr == '\0' )
            return 1;
    }
    return 0; /* failed to convert string to integer */
}

void setup() 
{ 
  Serial.begin(115200);
  servo.attach(9);
  memset(angle, '\0', MAX_CHAR);
}

void loop() 
{ 
  if (Serial.available() > 0)
  {
    char c = NULL;
    
    while ((c = Serial.read()) >= 0) 
    {  
      if ( c == '\r' || c == '\n' )
      {
        int incr = 1;
        int newpos;
        int pos = servo.read();

        // Check to make sure the input is all ints      
        if (!myatoi(angle, &newpos))
          Serial.println("Error: Bad input");
        else if (newpos >= 0 && newpos <=180)
        {
          // Seal off the string
          *p = NULL;
          Serial.println("Input angle: " + String(angle));
          if ((newpos - pos) > 0)
            incr = 1;
          else if ((newpos - pos) < 0)
            incr = -1;
            
          for(int ipos = servo.read(); ipos != newpos; ipos += incr)
          {
            servo.write(ipos);
            delay(30);
          }
        }
        else
          Serial.println("Error: Bad angle");
      }
      else
      {          
        if (alen == MAX_CHAR-1)
          Serial.println("Error: Angle more than 3 bytes");
        else
        {
          *p++ = c;
          alen++;
          continue;
        }
      }
        
      // Reset our string
      p = angle;
      memset(angle, '\0', MAX_CHAR);
      alen = 0;
        
      // Chuck anything after a newline/cr
      Serial.flush();
      Serial.println("OK: Good to go");
    }
  }
}
