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
const int analogIn1Pin = A4;  // Analog input pin that the potentiometer is attached to
const int analogIn2Pin = A5;  // Analog input pin that the potentiometer is attached to

float sensor1Value = 0;  // value read from the pot
float sensor2Value = 0;  // value read from the pot
float output1Value = 0;  // value output to the PWM (analog out)
float output2Value = 0;  // value output to the PWM (analog out)
float temp1 = 0;
float temp2 = 0;

void setup() {
  // initialize serial communications at 9600 bps:
  Serial.begin(9600);
}

void loop() {
  // read the analog in value:
  sensor1Value = analogRead(analogIn1Pin);
  sensor2Value = analogRead(analogIn2Pin);
  // map it to the range of the analog out:
  output1Value = log(10000*((1023/sensor1Value)-1)/1000);
  temp1 = (1.0 / ((1.0 / 298.15) + (1.0 / 3950) * output1Value)) - 273.15;

  output2Value = log(10000*((1023/sensor2Value)-1)/1000);
  temp2 = (1.0 / ((1.0 / 298.15) + (1.0 / 3950) * output1Value)) - 273.15;

  //float rNtc = R_FIXO * ((1023.0 / leituraADC) - 1.0);

  // 3. Aplicar a Equação do Parâmetro Beta
  //float logNtc = log(rNtc / R0); // Calcula o logaritmo natural
  //float temperaturaKelvin = 1.0 / ((1.0 / T0) + (1.0 / BETA) * logNtc);
  // change the analog out value:
  Serial.print("Sensor 1 =");
  Serial.print(sensor1Value);
  Serial.print(" output 1 =");
  Serial.print(temp1);
  Serial.print(" Sensor 2 =");
  Serial.print(sensor2Value);
  Serial.print(" output 2 =");
  Serial.println(temp2);


  // wait 2 milliseconds before the next loop for the analog-to-digital
  // converter to settle after the last reading:
  delay(100);
}
