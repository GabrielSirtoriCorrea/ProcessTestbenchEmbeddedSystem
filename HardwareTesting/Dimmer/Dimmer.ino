#define PINO_ZC 2
#define PINO_DIM 9
volatile long luminosidade = 0;  // 0 a 100
void zeroCross()  {
  if (luminosidade > 100) luminosidade = 100;
  if (luminosidade < 0) luminosidade = 0;
  long t1 = 8200L * (100L - luminosidade) / 100L;
  delayMicroseconds(t1);
  digitalWrite(PINO_DIM, HIGH);
  delayMicroseconds(6);
  // t2
  digitalWrite(PINO_DIM, LOW);
}
void setup() {
  pinMode(PINO_DIM, OUTPUT);
  attachInterrupt(0, zeroCross, RISING);
}
void loop() {
  for (byte i = 20; i < 90; i++) { //Aumenta a intensidade da lâmpada
    luminosidade = i;
    delay(20);
  }
  for (byte i = 90; i > 20; i--) { //Diminui a intensidade da lâmpada
    luminosidade = i;
    delay(20);
  }
}
