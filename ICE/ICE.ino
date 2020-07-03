#include <EEPROM.h>
#include <TimerOne.h>
#include <SimpleTimer.h>

#define RED 4
#define YELLOW 5
#define GREEN 6

#define THERMISTORPIN A0         
// resistance at 25 degrees C
#define THERMISTORNOMINAL 10000      
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25   
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 50
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
// the value of the 'other' resistor
#define SERIESRESISTOR 10000    

// Debug mode: print algorithm status through the serial port
unsigned int debug = 1;
unsigned int repeatStatusHeader = 100;  // Repeat status header every ... (0-no)
unsigned int statusCount = 1;

// IO pins
const int output = 10;         // PWM output
const int tempPin = A0;        // temperature sensor input

// PWM parameters

// PWM cycle in us
//     us    Freq
//     40    25 KHz   - PWM-controlled fan
//    100    10 KHz
//   1000     1 KHz
//  10000   100 Hz
float cycle = 40;
float dutyMin = 0.0;        // Minimum duy cycle value (some PWM fans need this)
float dutyMax = 100.0;      // Maximum duty cycle value
float duty = dutyMin;       // Initial duty cycle in %
bool dutyInv = false;       // True - invert duty output
 
// Control parameters
float temp;                 // Filtered (pre-processed) temperature
float tempRaw;              // Temperature read from sensor
float temp0;                // Previous filtered temperature
const float tempMax = 100;  // Maximum allowed target temperature
const float tempMin = 10;   // Minimum allowed target temperature
float targetTemp = 30;    // Initial target temperature
float targetTemp0 = targetTemp;    // Target temperature in the previous cycle

// Sample PID control values
// Interval   kp   ki   kd   applications  stars (* good ** very good *** excelent)
// ---------------------------------------------
//   5000   25  1.2   20  -  small fan in a box (**)
//  10000    5  0.8   10
//  10000   10  0.8   40
//   5000   20  0.4   20  -  fridge
//   5000   10  0.4   10  -  fridge, box (**)
//   5000   15  0.3   20  -
const int controlInterval = 250; // Control loop time interval in ms
float kp = 25;            // Proportional factor power% / C    (5 - 40)     25
float ki = 1.5;           // Integral factor power%/(C * s)    (0.1 - 2)       1
float kd = 50;            // Derivative factor power% * s / C  (0 - 40)    50
float softTemp = 0.5;     // Teemperature softening factor 0<softTemp<=1 (1 no soft)
float delta;              // temp - targetTemp
float dutyInt = 0;        // Integral component
float dutyProp;           // Proportional component
float dutyDer;            // Derivative component
float propMax = 150.0;    // Maximum proportional control component
float intMax = 100.0;     // Maximum integral control component
float derMax = 100.0;     // Maximum derivative control component
float duty0 = 0;          // Duty value in the previous cycle
float dutyKick = 50;      // Minimum duty to start fan from stop
float dutyOff = 3;        // Minimum duty that makes the fan to stop moving
float dutyOn = 8;         // Minimum duty value to force the fan to run
unsigned int fanOff = 0;  // Guessed fan state 1 - fan is off

  ///////////////////////////
 ////  Initial objects  ////
///////////////////////////

// Timed functions timer
SimpleTimer timer;

  ////////////////////////////
 ////  Helper functions  ////
////////////////////////////

int samples[NUMSAMPLES];

float getTempLM35(int analogPin)
{
  uint8_t i;
  float average;
 
  // take N samples in a row, with a slight delay
  for (i=0; i< NUMSAMPLES; i++) {
   samples[i] = analogRead(THERMISTORPIN);
   delay(1);
  }
  
  // average all the samples out
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
     average += samples[i];
  }
  average /= NUMSAMPLES;
 
  //Serial.print("Average analog reading "); 
  //Serial.println(average);
  
  // convert the value to resistance
  average = 1023*4 / average - 1;
  average = SERIESRESISTOR / average;
  //Serial.print("Thermistor resistance "); 
  //Serial.println(average);
  
  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C
  
  Serial.print("Temperature "); 
  Serial.print(steinhart);
  Serial.print(" *C");
  return steinhart;
}

////  PID control loop  ////

void controlLoopPID()
{
  // Read sensor temperature
  tempRaw = getTempLM35(tempPin);
  // Pre-process raw temperature. Average with previous temp using a
  // softening factor
  temp = tempRaw * softTemp + temp0 * (1 - softTemp);
  if ((tempRaw - temp >= 1.0) || (temp - tempRaw >= 1.0)) {
    temp = tempRaw;
  }
  Serial.print("    temp: ");
  Serial.print(temp);

  //
  delta = temp - targetTemp;

  // Calculate proportional component
  dutyProp = constrain(kp * delta, -propMax, propMax);
  // Experiment: cuadratic proportional component: faster response for
  // high deltas
  // dutyProp = kp/2.0 * delta * (1 + abs(delta));
  // dutyProp = constrain(dutyProp, -propMax, propMax);

  // Integral component
  dutyInt = dutyInt + ki * (controlInterval/1000.0) * delta;
  dutyInt = constrain(dutyInt, 40.0, intMax);

  // Derivative component
  dutyDer = kd * (temp - temp0) / controlInterval * 1000;
  dutyDer = constrain(dutyDer, -derMax, derMax);

  // Combine all control components
  duty = dutyProp + dutyInt + dutyDer;

  Serial.print("    P: ");
  Serial.print(dutyProp);
  Serial.print("    I: ");
  Serial.print(dutyInt);
  Serial.print("    D: ");
  Serial.print(dutyDer);

  // Experiment: make derivative comp. be stored in integral part
  //dutyInt = dutyInt + dutyDer;
  //duty = dutyProp + dutyInt;

  // Duty limits
  duty = constrain(duty, dutyMin, dutyMax);

  // Experiment: trim integral component when applying duty limits
  //  if (duty > dutyMax) {
  //    dutyInt = dutyInt - (duty - dutyMax);
  //    duty = dutyMax;
  //  } else if (duty < dutyMin) {
  //    dutyInt = dutyInt - duty;
  //    duty = dutyMin;
  //  }

  // Force fan start if we guess fan was stopped
  if (fanOff == 1 && duty > dutyOn) {
    duty = dutyKick;
    fanOff = 0;
  }
  Serial.print("     Duty: ");
  Serial.println(duty);
  // Guess if fan have stopped
  if (duty < dutyOff)
    fanOff = 1;

  // Save current algorithm values for next loop
  duty0 = duty;
  temp0 = temp;
  targetTemp0 = targetTemp;

  // Invert duty cycle if necessary
  float dutyOut = dutyInv ? 100.0 - duty : duty;

  // Update PWM output timer
  Timer1.setPwmDuty(output, dutyOut / 100.0 * 1023.0);
  if (duty > 66) {
    digitalWrite(RED, LOW);
    digitalWrite(YELLOW, HIGH);
    digitalWrite(GREEN, HIGH); 
  } else if (duty > 33) {
    digitalWrite(RED, HIGH);
    digitalWrite(YELLOW, LOW);
    digitalWrite(GREEN, HIGH);
  } else {
    
    digitalWrite(RED, HIGH);
    digitalWrite(YELLOW, HIGH);
    digitalWrite(GREEN, LOW);
  }
}

////  Sample simple control loop  ////
// Increase duty cycle (fan speed) when temp > targetTemp,
// decrease duty cycle (fan speed) when temp < targetTemp.
void controlLoopSimple()
{
  temp = getTempLM35(tempPin);
  if (temp > targetTemp)
    duty = duty < dutyMax ? duty + 1 : duty;
  else
    duty = duty > 0 ? duty - 1 : duty;

  Timer1.setPwmDuty(output, duty / 100.0 * 1023.0);
}

////  Sample simple control loop imitating a thermostat  ////
void controlLoopThermostatic()
{
  temp = getTempLM35(tempPin);
  if (temp > targetTemp + 0.5)
    duty = dutyMax;
  else if (temp < targetTemp - 0.5)
    duty = 0.0;

  Timer1.setPwmDuty(output, duty / 100.0 * 1023.0);
}

  /////////////////
 ////  Setup  ////
/////////////////

void setup()
{
  pinMode(RED, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);
  digitalWrite(RED, HIGH);
  digitalWrite(YELLOW, HIGH);
  digitalWrite(GREEN, HIGH);

  // Serial configuration
  Serial.begin(9600);
  Serial.println("Starting...");


  // Set analog reference to internal 1.1V
  //analogReference(EXTERNAL);

  pinMode(output, OUTPUT);
  // Initial trigger: max speed for 2 s
  digitalWrite(output, HIGH);
  delay(500);

  // Set timer-based pwm
  Timer1.initialize(cycle);
  Timer1.pwm(output, duty / 100.0 * 1023.0);

  // Set callback
  //Timer1.attachInterrupt(callback)

  // Control setup
  temp0 = getTempLM35(tempPin);
  temp = temp0;
  Serial.print(temp);
  timer.setInterval(controlInterval, controlLoopPID);
  // timer.setInterval(controlInterval, controlLoopThermostatic);
}

  /////////////////////
 ////  Main loop  ////
/////////////////////

void loop()
{
  timer.run();
}

