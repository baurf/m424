/* Arduino pin 6 -> CLK (Green on the 6-pin cable)
  5 -> LAT (Blue)
  7 -> SER on the IN side (Yellow)
  5V -> 5V (Orange)
  Power Arduino with 12V and connect to Vin -> 12V (Red)
  GND -> GND (Black)

  There are two connectors on the Large Digit Driver. 'IN' is the input side that should be connected to
  your microcontroller (the Arduino). 'OUT' is the output side that should be connected to the 'IN' of addtional
  digits.

  Each display will use about 150mA with all segments and decimal point on.

*/
#include <EEPROM.h>


//GPIO declarations
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

byte segmentClock = 6;
byte segmentLatch = 5;
byte segmentData = 7;
byte stopButton =  9;


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
int hour = 0;
float hourf = 0;
float hourold = 0;
int bighour = 0;
int smallhour = 0;
float mil = millis();
void setup()
{
  Serial.begin(9600);
  Serial.println("Large Digit Driver Example");


  pinMode(segmentClock, OUTPUT);
  pinMode(segmentData, OUTPUT);
  pinMode(segmentLatch, OUTPUT);
  pinMode(stopButton, INPUT);
  pinMode(2, INPUT);

  digitalWrite(segmentClock, LOW);
  digitalWrite(segmentData, LOW);
  digitalWrite(segmentLatch, LOW);
  digitalWrite(stopButton, HIGH);
  hour = EEPROM.read(5);
  hour += EEPROM.read(6) * 256;
  hourold = hour;
  delay(200);
}

int addr = 0;
int readpin = 1;
int readpinold;


void loop()
{
  readpinold = readpin;
  readpin = digitalRead(stopButton);

  
  if (readpin == 0 && readpinold == 1) {        //displays a number  (on button pressed for the first time)
    showNumber(420);
  } else if (readpin == 0 && readpinold == 0) { //delays so the number from befor remains on display (after button gets pressed until it's released)
    delay(500);
  } else if (readpin == 1 && readpinold == 0) { //resets the hours and pins (on release)
    hour = 0;
    readpin = 0;
    EEPROM.write(7, 0); //0 for pressing the button
    EEPROM.write(8, 0);
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
  } else {                                      //counts upwards from 0 (usually in hours) and handles overflows (until button gets pressed again)
    mil = millis();
    hourf = hour;
    if (mil > ((hourf - hourold + 1) * 2 * 1000))  { //EDIT SO IT IS HOURS
      hour++;
      
      smallhour = hour % 256;
      if (hour > 255){bighour = hour / 256;}
      
    EEPROM.write(7, smallhour); //smallhour einfügen
    EEPROM.write(8, bighour); //bighour
    }
    if (hour >= 1000) {
      EEPROM.write(7, 0);
      EEPROM.write(8, 0);
      pinMode(2, OUTPUT);
      digitalWrite(2, LOW);
    }
    showNumber(hour); //Test pattern

    
  }
}

//Takes a number and displays 3 numbers. Displays absolute value (no negatives)
void showNumber(float value)
{
  int number = abs(value); //Remove negative signs and any decimals

  //Serial.print("number: ");
  //Serial.println(number);

  for (byte x = 0 ; x < 3 ; x++)
  {
    int remainder = number % 10;

    postNumber(remainder, false);

    number /= 10;
  }

  //Latch the current segment data
  digitalWrite(segmentLatch, LOW);
  digitalWrite(segmentLatch, HIGH); //Register moves storage register on the rising edge of RCK
}

//Given a number, or '-', shifts it out to the display
void postNumber(byte number, boolean decimal)
{
  //    -  A
  //   / / F/B
  //    -  G
  //   / / E/C
  //    -. D/DP

#define a  1<<0
#define b  1<<6
#define c  1<<5
#define d  1<<4
#define e  1<<3
#define f  1<<1
#define g  1<<2
#define dp 1<<7

  byte segments;

  switch (number)
  {
    case 1: segments = b | c; break;
    case 2: segments = a | b | d | e | g; break;
    case 3: segments = a | b | c | d | g; break;
    case 4: segments = f | g | b | c; break;
    case 5: segments = a | f | g | c | d; break;
    case 6: segments = a | f | g | e | c | d; break;
    case 7: segments = a | b | c; break;
    case 8: segments = a | b | c | d | e | f | g; break;
    case 9: segments = a | b | c | d | f | g; break;
    case 0: segments = a | b | c | d | e | f; break;
    case ' ': segments = 0; break;
    case 'c': segments = g | e | d; break;
    case '-': segments = g; break;
  }

  if (decimal) segments |= dp;

  //Clock these bits out to the drivers
  for (byte x = 0 ; x < 8 ; x++)
  {
    digitalWrite(segmentClock, LOW);
    digitalWrite(segmentData, segments & 1 << (7 - x));
    digitalWrite(segmentClock, HIGH); //Data transfers to the register on the rising edge of SRCK
  }
}
