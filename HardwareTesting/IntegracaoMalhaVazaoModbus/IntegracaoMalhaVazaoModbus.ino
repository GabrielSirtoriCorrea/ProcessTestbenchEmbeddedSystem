#include <Servo.h>
#include <avr/interrupt.h>

// ---------------- Configuracao ----------------
#define MODBUS_SLAVE_ID       1
#define MAX485_DE_RE          7
#define MODBUS_BAUD           9600
#define MODBUS_TIMEOUT_MS     5

const byte PINO_VAZAO = 11;
const byte PINO_SERVO = 3;
const byte PINO_BOMBA = 10;

const byte BOMBA_LIGADA_LEVEL = HIGH;
const float FATOR_PULSOS = 7.5f;  // Hz por L/min
const uint16_t NUM_HOLDING_REGS = 16;
const uint16_t NUM_COILS = 16;
const uint8_t RX_BUFFER_SIZE = 64;

Servo myservo;
int pos = 0;

volatile unsigned long contadorPulsos = 0;
volatile byte estadoAnterior = HIGH;

unsigned long tempoAnterior = 0;
unsigned long pulsosAcumulados = 0;
float vazaoLmin = 0.0f;
float volumeLitros = 0.0f;
unsigned long pulsosUltimoIntervalo = 0;

uint16_t holdingRegisters[NUM_HOLDING_REGS] = {0};
bool coils[NUM_COILS] = {false};

// ---------------- PCINT: D11 = PB3 = PCINT3 ----------------
ISR(PCINT0_vect) {
  byte estadoAtual = (PINB & _BV(PB3)) ? HIGH : LOW;

  // Conta apenas a borda de descida (HIGH -> LOW)
  if (estadoAnterior == HIGH && estadoAtual == LOW) {
    contadorPulsos++;
  }
  estadoAnterior = estadoAtual;
}

void configurarPCINT() {
  pinMode(PINO_VAZAO, INPUT_PULLUP);
  estadoAnterior = (PINB & _BV(PB3)) ? HIGH : LOW;

  cli();
  PCICR |= _BV(PCIE0);       // Habilita grupo PCINT0 (D8-D13)
  PCMSK0 |= _BV(PCINT3);     // Habilita especificamente D11
  PCIFR |= _BV(PCIF0);       // Limpa flag pendente
  sei();
}

// ---------------- Controle da bomba ----------------
void aplicarEstadoBomba(bool ligada) {
  coils[0] = ligada;
  digitalWrite(PINO_BOMBA,
               ligada ? BOMBA_LIGADA_LEVEL :
                        (BOMBA_LIGADA_LEVEL == HIGH ? LOW : HIGH));
}

// ---------------- CRC-16 Modbus ----------------
uint16_t modbusCRC(const uint8_t *buffer, uint8_t length) {
  uint16_t crc = 0xFFFF;

  for (uint8_t i = 0; i < length; i++) {
    crc ^= buffer[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

// ---------------- Direcao RS-485 ----------------
void rs485Receive() {
  digitalWrite(MAX485_DE_RE, LOW);
}

void rs485Transmit() {
  digitalWrite(MAX485_DE_RE, HIGH);
}

void modbusSend(uint8_t *frame, uint8_t length) {
  rs485Transmit();
  delayMicroseconds(100);

  for (uint8_t i = 0; i < length; i++) {
    Serial.write(frame[i]);
  }

  Serial.flush();
  delayMicroseconds(100);
  rs485Receive();
}

void sendException(uint8_t slave, uint8_t function, uint8_t exceptionCode) {
  uint8_t frame[5];
  frame[0] = slave;
  frame[1] = function | 0x80;
  frame[2] = exceptionCode;

  uint16_t crc = modbusCRC(frame, 3);
  frame[3] = lowByte(crc);
  frame[4] = highByte(crc);
  modbusSend(frame, 5);
}

// ---------------- Sincroniza dados do processo com registradores ----------------
void atualizarRegistradoresProcesso() {
  // Registro 0: vazao em centesimos de L/min
  float vazaoEscalada = vazaoLmin * 100.0f;
  if (vazaoEscalada < 0.0f) vazaoEscalada = 0.0f;
  if (vazaoEscalada > 65535.0f) vazaoEscalada = 65535.0f;
  holdingRegisters[0] = (uint16_t)(vazaoEscalada + 0.5f);

  // Registro 1: volume em centesimos de litro; satura em 655,35 L
  float volumeEscalado = volumeLitros * 100.0f;
  if (volumeEscalado < 0.0f) volumeEscalado = 0.0f;
  if (volumeEscalado > 65535.0f) volumeEscalado = 65535.0f;
  holdingRegisters[1] = (uint16_t)(volumeEscalado + 0.5f);

  // Registro 2: pulsos contados no intervalo mais recente
  holdingRegisters[2] = (pulsosUltimoIntervalo > 65535UL)
                        ? 65535
                        : (uint16_t)pulsosUltimoIntervalo;

  // Registro 3: posicao do servo em graus
  holdingRegisters[3] = (uint16_t)pos;
}

// ---------------- Funcao 03: Read Holding Registers ----------------
void function03(uint8_t *request) {
  uint16_t startAddress = ((uint16_t)request[2] << 8) | request[3];
  uint16_t quantity = ((uint16_t)request[4] << 8) | request[5];

  if (quantity < 1 || quantity > 125) {
    sendException(MODBUS_SLAVE_ID, 0x03, 0x03);
    return;
  }
  if ((startAddress + quantity) > NUM_HOLDING_REGS) {
    sendException(MODBUS_SLAVE_ID, 0x03, 0x02);
    return;
  }

  // Resposta: ID + funcao + byte count + dados + CRC
  uint8_t frame[64];
  uint8_t index = 0;
  frame[index++] = MODBUS_SLAVE_ID;
  frame[index++] = 0x03;
  frame[index++] = (uint8_t)(quantity * 2);

  for (uint16_t i = 0; i < quantity; i++) {
    uint16_t value = holdingRegisters[startAddress + i];
    frame[index++] = highByte(value);
    frame[index++] = lowByte(value);
  }

  uint16_t crc = modbusCRC(frame, index);
  frame[index++] = lowByte(crc);
  frame[index++] = highByte(crc);
  modbusSend(frame, index);
}

// ---------------- Funcao 06: Write Single Holding Register ----------------
void function06(uint8_t *request) {
  uint16_t address = ((uint16_t)request[2] << 8) | request[3];
  uint16_t value = ((uint16_t)request[4] << 8) | request[5];

  if (address >= NUM_HOLDING_REGS) {
    sendException(MODBUS_SLAVE_ID, 0x06, 0x02);
    return;
  }

  // Os quatro primeiros registradores sao dados de processo somente leitura.
  if (address <= 3) {
    sendException(MODBUS_SLAVE_ID, 0x06, 0x02);
    return;
  }

  holdingRegisters[address] = value;

  uint8_t response[8];
  for (uint8_t i = 0; i < 6; i++) response[i] = request[i];

  uint16_t crc = modbusCRC(response, 6);
  response[6] = lowByte(crc);
  response[7] = highByte(crc);
  modbusSend(response, 8);
}

// ---------------- Funcao 16 / 0x10: Write Multiple Holding Registers ----------------
void function16(uint8_t *request, uint8_t length) {
  uint16_t startAddress = ((uint16_t)request[2] << 8) | request[3];
  uint16_t quantity = ((uint16_t)request[4] << 8) | request[5];
  uint8_t byteCount = request[6];

  if (quantity < 1 || quantity > 123 || byteCount != quantity * 2) {
    sendException(MODBUS_SLAVE_ID, 0x10, 0x03);
    return;
  }
  if ((startAddress + quantity) > NUM_HOLDING_REGS) {
    sendException(MODBUS_SLAVE_ID, 0x10, 0x02);
    return;
  }
  if (length != (uint8_t)(9 + byteCount)) return;

  // Nao permite escrever nos registradores de processo 0..3.
  if (startAddress <= 3) {
    sendException(MODBUS_SLAVE_ID, 0x10, 0x02);
    return;
  }

  uint8_t index = 7;
  for (uint16_t i = 0; i < quantity; i++) {
    holdingRegisters[startAddress + i] =
      ((uint16_t)request[index] << 8) | request[index + 1];
    index += 2;
  }

  uint8_t response[8];
  response[0] = MODBUS_SLAVE_ID;
  response[1] = 0x10;
  response[2] = request[2];
  response[3] = request[3];
  response[4] = request[4];
  response[5] = request[5];

  uint16_t crc = modbusCRC(response, 6);
  response[6] = lowByte(crc);
  response[7] = highByte(crc);
  modbusSend(response, 8);
}

// ---------------- Funcao 01: Read Coils ----------------
void readCoils(uint8_t *request) {
  uint16_t startAddress = ((uint16_t)request[2] << 8) | request[3];
  uint16_t quantity = ((uint16_t)request[4] << 8) | request[5];

  if (quantity < 1 || quantity > 2000) {
    sendException(MODBUS_SLAVE_ID, 0x01, 0x03);
    return;
  }
  if ((startAddress + quantity) > NUM_COILS) {
    sendException(MODBUS_SLAVE_ID, 0x01, 0x02);
    return;
  }

  uint8_t byteCount = (quantity + 7) / 8;
  uint8_t response[64];
  uint8_t index = 0;

  response[index++] = MODBUS_SLAVE_ID;
  response[index++] = 0x01;
  response[index++] = byteCount;

  for (uint8_t i = 0; i < byteCount; i++) response[index++] = 0;

  for (uint16_t i = 0; i < quantity; i++) {
    if (coils[startAddress + i]) {
      response[3 + (i / 8)] |= (uint8_t)(1U << (i % 8));
    }
  }

  uint16_t crc = modbusCRC(response, index);
  response[index++] = lowByte(crc);
  response[index++] = highByte(crc);
  modbusSend(response, index);
}

// ---------------- Funcao 05: Write Single Coil ----------------
void writeSingleCoil(uint8_t *request) {
  uint16_t address = ((uint16_t)request[2] << 8) | request[3];
  uint16_t value = ((uint16_t)request[4] << 8) | request[5];

  if (address >= NUM_COILS) {
    sendException(MODBUS_SLAVE_ID, 0x05, 0x02);
    return;
  }
  if (value != 0xFF00 && value != 0x0000) {
    sendException(MODBUS_SLAVE_ID, 0x05, 0x03);
    return;
  }

  coils[address] = (value == 0xFF00);

  // Coil 0 e o comando da bomba; demais coils ficam como bits auxiliares.
  if (address == 0) {
    aplicarEstadoBomba(coils[0]);
  }

  uint8_t response[8];
  for (uint8_t i = 0; i < 6; i++) response[i] = request[i];

  uint16_t crc = modbusCRC(response, 6);
  response[6] = lowByte(crc);
  response[7] = highByte(crc);
  modbusSend(response, 8);
}

// ---------------- Processamento de frame Modbus ----------------
void processModbusFrame(uint8_t *frame, uint8_t length) {
  if (length < 4) return;
  if (frame[0] != MODBUS_SLAVE_ID) return;

  uint16_t receivedCRC =
    ((uint16_t)frame[length - 1] << 8) | frame[length - 2];
  uint16_t calculatedCRC = modbusCRC(frame, length - 2);
  if (receivedCRC != calculatedCRC) return;

  uint8_t function = frame[1];

  switch (function) {
    case 0x01:
      if (length == 8) readCoils(frame);
      break;
    case 0x03:
      if (length == 8) function03(frame);
      break;
    case 0x05:
      if (length == 8) writeSingleCoil(frame);
      break;
    case 0x06:
      if (length == 8) function06(frame);
      break;
    case 0x10:
      function16(frame, length);
      break;
    default:
      sendException(MODBUS_SLAVE_ID, function, 0x01);
      break;
  }
}

// ---------------- Recepcao Modbus RTU ----------------
void modbusReceive() {
  static uint8_t buffer[RX_BUFFER_SIZE];
  static uint8_t index = 0;
  static unsigned long lastByteTime = 0;
  static bool overflow = false;

  while (Serial.available()) {
    uint8_t byteReceived = (uint8_t)Serial.read();

    if (index < sizeof(buffer)) {
      buffer[index++] = byteReceived;
    } else {
      overflow = true;
    }
    lastByteTime = millis();
  }

  if (index > 0 && (millis() - lastByteTime) >= MODBUS_TIMEOUT_MS) {
    if (!overflow) processModbusFrame(buffer, index);
    index = 0;
    overflow = false;
  }
}

// ---------------- Setup / Loop ----------------
void setup() {
  pinMode(MAX485_DE_RE, OUTPUT);
  rs485Receive();

  pinMode(PINO_BOMBA, OUTPUT);
  aplicarEstadoBomba(true);  // Mantem comportamento do codigo original: D10 em HIGH

  myservo.attach(PINO_SERVO);
  myservo.write(pos);

  Serial.begin(MODBUS_BAUD);

  configurarPCINT();

  tempoAnterior = millis();
  atualizarRegistradoresProcesso();
}

void loop() {
  // Mantem a comunicacao Modbus responsiva entre atualizacoes de vazao.
  modbusReceive();

  unsigned long tempoAtual = millis();
  unsigned long intervalo = tempoAtual - tempoAnterior;

  if (intervalo >= 1000UL) {
    unsigned long pulsos;

    noInterrupts();
    pulsos = contadorPulsos;
    contadorPulsos = 0;
    interrupts();

    pulsosUltimoIntervalo = pulsos;
    vazaoLmin = (pulsos * 1000.0f) / (intervalo * FATOR_PULSOS);

    pulsosAcumulados += pulsos;
    volumeLitros = pulsosAcumulados / 450.0f;

    // Mantem o movimento de teste do servo do arquivo original.
    pos += 5;
    if (pos > 110) pos = 0;
    myservo.write(pos);

    atualizarRegistradoresProcesso();
    tempoAnterior = tempoAtual;
  }
}
