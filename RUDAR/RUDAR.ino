#include <Servo.h> 
 
Servo myservo;  // create servo object to control a servo 
int pos = 0;    // variable to store the servo position
int sensorpin = 0;                 // analog pin used to connect the sharp sensor
int val = 0;                 // variable to store the values from sensor(initially zero)
 
void setup() 
{ 
  Serial.begin(9600);
  while (!Serial) {
    ; //wait for Leonardo serial to connect
  }
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object 
} 

void loop() 
{ 
  for(pos = 0; pos < 90; pos += 1)  // goes from 0 degrees to 90 degrees 
  {                                  // in steps of 1 degree 
    myservo.write(pos);              // tell servo to go to position in variable 'pos' 
    delay(15);    // waits 15ms for the servo to reach the position 
    val = analogRead(sensorpin);  //read sensor value
    Serial.println(val);  //print sensor value
    delay(25); //sensor be crazy
  } 
  for(pos = 90; pos>=1; pos-=1)     // goes from 90 degrees to 0 degrees 
  {                                
    myservo.write(pos);              // tell servo to go to position in variable 'pos' 
    delay(15);                       // waits 15ms for the servo to reach the position 
    val = analogRead(sensorpin);  //read sensor value
    Serial.println(val);  //print sensor value
    delay(25);  //sensory overload
  } 
}
