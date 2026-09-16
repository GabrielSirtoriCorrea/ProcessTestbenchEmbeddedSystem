
// ============================================================
// CONFIGURAÇÃO
// ============================================================

#define MODBUS_SLAVE_ID    1
#define MAX485_DE_RE       7

#define MODBUS_BAUD        9600

// Quantidade de Holding Registers
#define NUM_HOLDING_REGS   16
#define NUM_COILS       16

// Timeout entre bytes do frame
#define MODBUS_TIMEOUT_MS  5


// ============================================================
// HOLDING REGISTERS
// ============================================================

// Registros 0 ... 15
//
// Dependendo do software:
//
// Register 0 -> 40001
// Register 1 -> 40002
// Register 2 -> 40003
// ...
//
// Valores iniciais para teste.

uint16_t holdingRegisters[NUM_HOLDING_REGS] =
{
    123,
    456,
    789,
    1000,
    2000,
    3000,
    4000,
    5000,
    6000,
    7000,
    8000,
    9000,
    10000,
    11000,
    12000,
    13000
};


// ============================================================
// CRC-16 MODBUS
// ============================================================

uint16_t modbusCRC(uint8_t *buffer, uint8_t length)
{
    uint16_t crc = 0xFFFF;

    for (uint8_t i = 0; i < length; i++)
    {
        crc ^= buffer[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}


// ============================================================
// CONTROLE DO MAX485
// ============================================================

void rs485Receive()
{
    digitalWrite(MAX485_DE_RE, LOW);
}


void rs485Transmit()
{
    digitalWrite(MAX485_DE_RE, HIGH);
}


// ============================================================
// ENVIA FRAME MODBUS
// ============================================================

void modbusSend(uint8_t *frame, uint8_t length)
{
    rs485Transmit();

    delayMicroseconds(100);

    for (uint8_t i = 0; i < length; i++)
    {
        Serial.write(frame[i]);
    }

    // Espera a UART terminar completamente
    Serial.flush();

    delayMicroseconds(100);

    rs485Receive();
}


// ============================================================
// ENVIA RESPOSTA COMUM
// ============================================================

void sendResponse(
    uint8_t slave,
    uint8_t function,
    uint8_t *data,
    uint8_t dataLength
)
{
    uint8_t frame[64];

    uint8_t index = 0;

    frame[index++] = slave;
    frame[index++] = function;

    for (uint8_t i = 0; i < dataLength; i++)
    {
        frame[index++] = data[i];
    }

    uint16_t crc = modbusCRC(frame, index);

    // Modbus RTU:
    // CRC Low byte primeiro
    frame[index++] = lowByte(crc);
    frame[index++] = highByte(crc);

    modbusSend(frame, index);
}


// ============================================================
// RESPOSTA DE EXCEÇÃO
// ============================================================

void sendException(
    uint8_t slave,
    uint8_t function,
    uint8_t exceptionCode
)
{
    uint8_t frame[5];

    frame[0] = slave;

    // Bit 7 indica exceção
    frame[1] = function | 0x80;

    frame[2] = exceptionCode;

    uint16_t crc = modbusCRC(frame, 3);

    frame[3] = lowByte(crc);
    frame[4] = highByte(crc);

    modbusSend(frame, 5);
}


// ============================================================
// FUNCTION 03
//
// READ HOLDING REGISTERS
//
// Request:
//
// [ID]
// [03]
// [START HI]
// [START LO]
// [QTY HI]
// [QTY LO]
// [CRC LO]
// [CRC HI]
//
// Response:
//
// [ID]
// [03]
// [BYTE COUNT]
// [DATA...]
// [CRC LO]
// [CRC HI]
// ============================================================

void function03(uint8_t *request)
{
    uint16_t startAddress =
        ((uint16_t)request[2] << 8) |
        request[3];

    uint16_t quantity =
        ((uint16_t)request[4] << 8) |
        request[5];


    // Quantidade válida:
    // 1 até 125
    if (quantity < 1 || quantity > 125)
    {
        sendException(
            MODBUS_SLAVE_ID,
            0x03,
            0x03
        );

        return;
    }


    // Verifica se está dentro dos nossos registros
    if ((startAddress + quantity) > NUM_HOLDING_REGS)
    {
        sendException(
            MODBUS_SLAVE_ID,
            0x03,
            0x02
        );

        return;
    }


    uint8_t frame[64];

    uint8_t index = 0;

    frame[index++] = MODBUS_SLAVE_ID;
    frame[index++] = 0x03;

    // Número de bytes
    frame[index++] = quantity * 2;


    // Dados
    for (uint16_t i = 0; i < quantity; i++)
    {
        uint16_t value =
            holdingRegisters[startAddress + i];

        frame[index++] = highByte(value);
        frame[index++] = lowByte(value);
    }


    // CRC
    uint16_t crc = modbusCRC(frame, index);

    frame[index++] = lowByte(crc);
    frame[index++] = highByte(crc);


    modbusSend(frame, index);
}


// ============================================================
// FUNCTION 06
//
// WRITE SINGLE HOLDING REGISTER
//
// Request:
//
// [ID]
// [06]
// [ADDR HI]
// [ADDR LO]
// [VALUE HI]
// [VALUE LO]
// [CRC]
//
// Response:
//
// Ecoa exatamente a requisição.
// ============================================================

void function06(uint8_t *request)
{
    uint16_t address =
        ((uint16_t)request[2] << 8) |
        request[3];

    uint16_t value =
        ((uint16_t)request[4] << 8) |
        request[5];


    // Verifica endereço
    if (address >= NUM_HOLDING_REGS)
    {
        sendException(
            MODBUS_SLAVE_ID,
            0x06,
            0x02
        );

        return;
    }


    // Atualiza registro
    holdingRegisters[address] = value;


    // Resposta = mesma requisição
    uint8_t response[8];

    for (uint8_t i = 0; i < 6; i++)
    {
        response[i] = request[i];
    }

    uint16_t crc = modbusCRC(response, 6);

    response[6] = lowByte(crc);
    response[7] = highByte(crc);

    modbusSend(response, 8);
}


// ============================================================
// FUNCTION 16 / 0x10
//
// WRITE MULTIPLE HOLDING REGISTERS
//
// Request:
//
// [ID]
// [10]
// [START HI]
// [START LO]
// [QTY HI]
// [QTY LO]
// [BYTE COUNT]
// [DATA...]
// [CRC]
//
// Response:
//
// [ID]
// [10]
// [START]
// [QTY]
// [CRC]
// ============================================================

void function16(uint8_t *request, uint8_t length)
{
    uint16_t startAddress =
        ((uint16_t)request[2] << 8) |
        request[3];

    uint16_t quantity =
        ((uint16_t)request[4] << 8) |
        request[5];

    uint8_t byteCount = request[6];


    // Verifica quantidade
    if (quantity < 1 || quantity > 123)
    {
        sendException(
            MODBUS_SLAVE_ID,
            0x10,
            0x03
        );

        return;
    }


    // Byte count deveria ser 2 bytes por registro
    if (byteCount != quantity * 2)
    {
        sendException(
            MODBUS_SLAVE_ID,
            0x10,
            0x03
        );

        return;
    }


    // Verifica endereço
    if ((startAddress + quantity) > NUM_HOLDING_REGS)
    {
        sendException(
            MODBUS_SLAVE_ID,
            0x10,
            0x02
        );

        return;
    }


    // Verifica tamanho do frame
    if (length < (9 + byteCount))
    {
        return;
    }


    // Escreve os registros

    uint8_t index = 7;

    for (uint16_t i = 0; i < quantity; i++)
    {
        uint16_t value =
            ((uint16_t)request[index] << 8) |
            request[index + 1];

        holdingRegisters[startAddress + i] = value;

        index += 2;
    }


    // Monta resposta

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


// ============================================================
// PROCESSAMENTO DO FRAME
// ============================================================

void processModbusFrame(uint8_t *frame, uint8_t length)
{
    // Frame mínimo:
    //
    // ID + FUNCTION + DATA + CRC
    //
    // mínimo = 4 bytes

    if (length < 4)
        return;


    // Verifica Slave ID

    if (frame[0] != MODBUS_SLAVE_ID)
        return;


    // --------------------------------------------------------
    // Verifica CRC
    // --------------------------------------------------------

    uint16_t receivedCRC =
        ((uint16_t)frame[length - 1] << 8) |
        frame[length - 2];

    uint16_t calculatedCRC =
        modbusCRC(frame, length - 2);


    if (receivedCRC != calculatedCRC)
    {
        // CRC inválido
        return;
    }


    // --------------------------------------------------------
    // Function Code
    // --------------------------------------------------------

    uint8_t function = frame[1];


    switch (function)
    {
        case 0x03:

            if (length == 8)
            {
                function03(frame);
            }

            break;


        case 0x06:

            if (length == 8)
            {
                function06(frame);
            }

            break;


        case 0x10:

            function16(frame, length);

            break;


        default:

            // Function não suportada
            sendException(
                MODBUS_SLAVE_ID,
                function,
                0x01
            );

            break;
    }
}


// ============================================================
// RECEPÇÃO DO MODBUS RTU
// ============================================================

void modbusReceive()
{
    static uint8_t buffer[64];

    static uint8_t index = 0;

    static unsigned long lastByteTime = 0;


    while (Serial.available())
    {
        uint8_t byteReceived =
            Serial.read();


        // Evita overflow
        if (index < sizeof(buffer))
        {
            buffer[index++] = byteReceived;
        }


        lastByteTime = millis();
    }


    // --------------------------------------------------------
    // Detecta fim do frame
    // --------------------------------------------------------

    if (index > 0)
    {
        if ((millis() - lastByteTime) >= MODBUS_TIMEOUT_MS)
        {
            processModbusFrame(buffer, index);

            index = 0;
        }
    }
}

bool coils[NUM_COILS] =
{
    false,   // Coil 0
    false,   // Coil 1
    false,   // Coil 2
    false,   // Coil 3
    false,   // Coil 4
    false,   // Coil 5
    false,   // Coil 6
    false,   // Coil 7
    false,   // Coil 8
    false,   // Coil 9
    false,   // Coil 10
    false,   // Coil 11
    false,   // Coil 12
    false,   // Coil 13
    false,   // Coil 14
    false    // Coil 15
};

void readCoils(uint8_t *request)
{
    uint16_t startAddress =
        ((uint16_t)request[2] << 8) |
        request[3];

    uint16_t quantity =
        ((uint16_t)request[4] << 8) |
        request[5];


    // Quantidade válida: 1 até 2000
    if (quantity < 1 || quantity > 2000)
    {
        sendException(0x01, 0x03);
        return;
    }


    // Verifica endereço
    if ((startAddress + quantity) > NUM_COILS)
    {
        sendException(0x01, 0x02);
        return;
    }


    uint8_t byteCount = (quantity + 7) / 8;

    uint8_t response[64];

    uint8_t index = 0;

    response[index++] = SLAVE_ID;
    response[index++] = 0x01;
    response[index++] = byteCount;


    // Inicializa os bytes das coils
    for (uint8_t i = 0; i < byteCount; i++)
    {
        response[index++] = 0;
    }


    // Coloca as coils dentro dos bits
    for (uint16_t i = 0; i < quantity; i++)
    {
        if (coils[startAddress + i])
        {
            response[3 + (i / 8)] |=
                (1 << (i % 8));
        }
    }


    // CRC
    uint16_t crc = modbusCRC(response, index);

    response[index++] = crc & 0xFF;
    response[index++] = crc >> 8;


    modbusSend(response, index);
}

void writeSingleCoil(uint8_t *request)
{
    uint16_t address =
        ((uint16_t)request[2] << 8) |
        request[3];

    uint16_t value =
        ((uint16_t)request[4] << 8) |
        request[5];


    // Verifica endereço
    if (address >= NUM_COILS)
    {
        sendException(0x05, 0x02);
        return;
    }


    // 0xFF00 = ON
    // 0x0000 = OFF

    if (value == 0xFF00)
    {
        coils[address] = true;
    }
    else if (value == 0x0000)
    {
        coils[address] = false;
    }
    else
    {
        sendException(0x05, 0x03);
        return;
    }


    // Resposta da função 05 é um eco
    uint8_t response[8];

    for (uint8_t i = 0; i < 6; i++)
    {
        response[i] = request[i];
    }


    uint16_t crc = modbusCRC(response, 6);

    response[6] = crc & 0xFF;
    response[7] = crc >> 8;


    modbusSend(response, 8);
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    // MAX485
    pinMode(MAX485_DE_RE, OUTPUT);

    // Começa em recepção
    rs485Receive();


    // UART
    Serial.begin(MODBUS_BAUD);
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    modbusReceive();
}