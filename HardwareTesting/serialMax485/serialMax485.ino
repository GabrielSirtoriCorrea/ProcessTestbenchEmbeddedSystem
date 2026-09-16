#define MAX485_DE_RE 7

char c = 'A';

void rs485Transmit() {
  digitalWrite(MAX485_DE_RE, HIGH);
}

void rs485Receive() {
  digitalWrite(MAX485_DE_RE, LOW);
}

void setup() {
  pinMode(MAX485_DE_RE, OUTPUT);

  // Inicialmente em modo recepção
  rs485Receive();

  Serial.begin(9600);
}

void loop() {

  // -------------------------
  // TRANSMISSÃO
  // -------------------------

  rs485Transmit();
  Serial.println("Hello RS485");
  Serial.println(c);

  // Aguarda a transmissão terminar
  Serial.flush();

  // Volta para recepção
  rs485Receive();

  // -------------------------
  // RECEPÇÃO
  // -------------------------
  while (Serial.available()) {
    c = Serial.read();

    // Mostra o dado recebido
  }

  delay(100);
}