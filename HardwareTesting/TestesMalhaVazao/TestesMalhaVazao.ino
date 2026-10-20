
#include <Servo.h>
#include <avr/interrupt.h>

Servo myservo;

int pos = 0;

const byte PINO_VAZAO = 11;
const byte PINO_SERVO = 3;
const byte PINO_BOMBA = 10;

const float FATOR_PULSOS = 7.5;

volatile unsigned long contadorPulsos = 0;
volatile byte estadoAnterior = HIGH;

unsigned long tempoAnterior = 0;
unsigned long pulsosAcumulados = 0;

float vazaoLmin = 0.0;
float volumeLitros = 0.0;

// Interrupcao por mudanca de pino do grupo PCINT0
ISR(PCINT0_vect) {
  byte estadoAtual = PINB & _BV(PB3);

  // Conta somente a borda de descida (HIGH -> LOW)
  if (estadoAnterior != 0 && estadoAtual == 0) {
    contadorPulsos++;
  }

  estadoAnterior = estadoAtual;
}

void configurarPCINT() {
  pinMode(PINO_VAZAO, INPUT_PULLUP);

  // D11 = PB3 = PCINT3
  estadoAnterior = (PINB & _BV(PB3)) ? HIGH : LOW;

  cli();

  // Habilita o grupo PCINT0: pinos D8 a D13
  PCICR |= _BV(PCIE0);

  // Habilita especificamente o PCINT3 (D11)
  PCMSK0 |= _BV(PCINT3);

  // Limpa eventual flag pendente do grupo
  PCIFR |= _BV(PCIF0);

  sei();
}

void setup() {
  myservo.attach(PINO_SERVO);

  Serial.begin(9600);

  configurarPCINT();

  pinMode(PINO_BOMBA, OUTPUT);
  digitalWrite(PINO_BOMBA, HIGH);

  tempoAnterior = millis();

  Serial.println("Leitura do sensor YF-S201 via PCINT");
  Serial.println("Vazao (L/min) | Volume (L)");
}

void loop() {
  unsigned long tempoAtual = millis();

  if (tempoAtual - tempoAnterior >= 1000) {

    unsigned long intervalo = tempoAtual - tempoAnterior;

    // Leitura atomica do contador de pulsos
    noInterrupts();
    unsigned long pulsos = contadorPulsos;
    contadorPulsos = 0;
    interrupts();

    // Vazao em litros por minuto
    vazaoLmin = (pulsos * 1000.0) /
                (intervalo * FATOR_PULSOS);

    // Volume acumulado
    pulsosAcumulados += pulsos;
    volumeLitros = pulsosAcumulados / 450.0;

    // Controle do servo
    pos += 5;

    if (pos > 110) {
      pos = 0;
    }

    myservo.write(pos);

    Serial.print("Vazao: ");
    Serial.print(vazaoLmin, 2);
    Serial.print(" L/min | Volume: ");
    Serial.print(volumeLitros, 3);
    Serial.print(" L | Pulsos: ");
    Serial.print(pulsos);
    Serial.print(" | Posicao servo: ");
    Serial.println(pos);

    tempoAnterior = tempoAtual;
  }
}
