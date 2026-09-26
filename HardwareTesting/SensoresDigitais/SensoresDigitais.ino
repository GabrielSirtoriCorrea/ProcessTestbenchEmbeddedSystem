/*
  Button

  Turns on and off a light emitting diode(LED) connected to digital pin 13,
  when pressing a pushbutton attached to pin 2.

  The circuit:
  - LED attached from pin 13 to ground through 220 ohm resistor
  - pushbutton attached to pin 2 from +5V
  - 10K resistor attached to pin 2 from ground

  - Note: on most Arduinos there is already an LED on the board
    attached to pin 13.

  created 2005
  by DojoDave <http://www.0j0.org>
  modified 30 Aug 2011
  by Tom Igoe

  This example code is in the public domain.

  https://docs.arduino.cc/built-in-examples/digital/Button/
*/

// constants won't change. They're used here to set pin numbers:
#define button1 A0  // the number of the pushbutton pi
#define button2 A1
#define button3 A2
#define button4 A3
// variables will change:
int buttonState1 = 0;  // variable for reading the pushbutton status
int buttonState2 = 0;  // variable for reading the pushbutton status
int buttonState3 = 0;  // variable for reading the pushbutton status
int buttonState4 = 0;  // variable for reading the pushbutton status

void setup() {
  // initialize the LED pin as an output:
  Serial.begin(9600);
  // initialize the pushbutton pin as an input:
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
  pinMode(button4, INPUT_PULLUP);
}

void loop() {
  // read the state of the pushbutton value:
  buttonState1 = digitalRead(button1);
  buttonState2 = digitalRead(button2);
  buttonState3 = digitalRead(button3);
  buttonState4 = digitalRead(button4);

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState1 == HIGH) {
    // turn LED on:
    Serial.println("1 - HIGH");
  } else {
    // turn LED off:
    Serial.println("1 - LOW");
  }

  if (buttonState2 == HIGH) {
    // turn LED on:
    Serial.println("2 - HIGH");
  } else {
    // turn LED off:
    Serial.println("2 - LOW");
  }

  if (buttonState3 == HIGH) {
    // turn LED on:
    Serial.println("3 - HIGH");
  } else {
    // turn LED off:
    Serial.println("3 - LOW");
  }

  if (buttonState4 == HIGH) {
    // turn LED on:
    Serial.println("4 - HIGH");
  } else {
    // turn LED off:
    Serial.println("4 - LOW");
  }

  delay(100);
}
