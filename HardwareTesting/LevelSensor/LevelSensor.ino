/*
  Analog input, analog output, serial output

  Reads an analog input pin, maps the result to a range from 0 to 255 and uses
  the result to set the pulse width modulation (PWM) of an output pin.
  Also prints the results to the Serial Monitor.

  The circuit:
  - potentiometer connected to analog pin 0.
    Center pin of the potentiometer goes to the analog pin.
    side pins of the potentiometer go to +5V and ground
  - LED connected from digital pin 9 to ground through 220 ohm resistor

  created 29 Dec. 2008
  modified 9 Apr 2012
  by Tom Igoe

  This example code is in the public domain.

  https://docs.arduino.cc/built-in-examples/analog/AnalogInOutSerial/
*/

// These constants won't change. They're used to give names to the pins used:
#define analogInPin A0// Analog input pin that the potentiometer is attached to
//const int analogOutPin = 9;  // Analog output pin that the LED is attached to

int sensorValue = 0;  // value read from the pot
int outputValue = 0;  // value output to the PWM (analog out)
int rawValues[50];
unsigned long sum = 0;

void setup() {
  // initialize serial communications at 9600 bps:
  pinMode(10, OUTPUT);
  Serial.begin(9600);
  digitalWrite(10, HIGH);
}

void loop() {
  // read the analog in value:
  /*sum = 0;
  for(int i = 0; i<=50; i++){
    rawValues[i] = analogRead(analogInPin);
    Serial.print(rawValues[i]);
    Serial.print(" | ");
    sum+= rawValues[i];
    delay(10);
  } 
*/

  //sensorValue = (int) sum/100;
  sensorValue = analogRead(analogInPin);
  // map it to the range of the analog out:
  outputValue = map(sensorValue, 0, 1023, 0, 100);
  // change the analog out value:
  //analogWrite(analogOutPin, outputValue);

  // print the results to the Serial Monitor:
  Serial.println();
  Serial.print("sensor = ");
  Serial.print(sensorValue);
  Serial.print("\t output = ");
  Serial.println(outputValue);

  // wait 2 milliseconds before the next loop for the analog-to-digital
  // converter to settle after the last reading:
  delay(500);

}
