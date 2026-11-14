
#include <Arduino.h>
#include <math.h>

#define PIN_NTC A0

const float R_FIXED = 10000.0;
const float R_NOMINAL = 10000.0;
const float T_NOMINAL = 298.15;
const float BETA = 3950.0;

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    int adc = analogRead(PIN_NTC);

    if (adc <= 0 || adc >= 1023)
    {
        Serial.println("Erro na leitura");
        delay(500);
        return;
    }

    // Para montagem: 5V -> resistor -> A0 -> NTC -> GND
    float resistance = R_FIXED *
                       ((1023.0 / adc) - 1.0);

    float tempK = 1.0 /
        ((1.0 / T_NOMINAL) +
        (1.0 / BETA) *
        log(resistance / R_NOMINAL));

    float tempC = tempK - 273.15;

    Serial.print("ADC: ");
    Serial.print(adc);

    Serial.print(" | Resistencia: ");
    Serial.print(resistance, 1);

    Serial.print(" ohms | Temperatura: ");
    Serial.print(tempC, 2);

    Serial.println(" C");

    delay(500);
}
