#include <OneWire.h>
#include <DallasTemperature.h>
#include <Arduino.h>
#include <math.h>

#define DS18B20_OneWire 2
#define PIN_NTC A0

OneWire oneWire(DS18B20_OneWire);

DallasTemperature sensortemp(&oneWire);

int ndispositivos = 0;
float grausC;

// Configuracoes

const float R_FIXED = 10000.0;  // Resistor fixo (ohms)
const float R_NOMINAL = 10000.0; // NTC a 25 graus (ohms)
const float T_NOMINAL = 298.15;  // 25 graus em Kelvin

// Coeficiente Beta do termistor
// Exemplo: NTC 10k B3950
const float BETA = 3950.0;

const float ADC_MAX = 1023.0;

float readTemperature()
{
    // Leitura analogica
    int adc = analogRead(PIN_NTC);

    // Evita divisao por zero
    if (adc <= 0 || adc >= 1023)
        return NAN;

    // Calcula resistencia do NTC
    float resistance = R_FIXED *
                       ((ADC_MAX / adc) - 1.0);

    // Equacao Beta (Steinhart-Hart simplificada)
    float temperatureK = 1.0 /
        ((1.0 / T_NOMINAL) +
        (1.0 / BETA) * log(resistance / R_NOMINAL));

    // Kelvin para Celsius
    return temperatureK - 273.15;
}


void setup(){
  sensortemp.begin();
  Serial.begin(9600);

  Serial.println("Localizando Dispositivos ...");
  Serial.print("Encontrados ");
  ndispositivos = sensortemp.getDeviceCount();
  Serial.print(ndispositivos, DEC);
  Serial.println(" dispositivos.");
  Serial.println("");
}

void loop() {
  sensortemp.requestTemperatures();

  for (int i = 0; i < ndispositivos; i++) {
    Serial.println("Graus Celsius:");
    Serial.print("Sensor ");
    Serial.print(i+1);
    Serial.print(": ");
    grausC = sensortemp.getTempCByIndex(i);
    Serial.print(grausC);
    Serial.println("ºC");
  }

  Serial.println("");
  delay(1000);

  float temperature = readTemperature();

    if (isnan(temperature))
    {
        Serial.println("Erro na leitura do termistor");
    }
    else
    {
        Serial.print("Temperatura termistor: ");
        Serial.print(temperature, 2);
        Serial.println(" °C");
    }

    delay(500);
}