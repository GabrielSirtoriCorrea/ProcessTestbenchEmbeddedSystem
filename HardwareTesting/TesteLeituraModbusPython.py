from pymodbus.client import ModbusSerialClient
import time

# ==========================================
# CONFIGURAÇÕES DA REDE MODBUS RTU
# ==========================================
PORTA_SERIAL = 'COM7'   # Altere para '/dev/ttyUSB0' se estiver no Linux
BAUDRATE = 9600         # Deve ser igual ao MODBUS_BAUD do Arduino
SLAVE_ID = 1            # MODBUS_SLAVE_ID definido no código Arduino

def monitorar_escravo_continuamente():
    # Inicializa o cliente serial Modbus RTU
    client = ModbusSerialClient(
        port=PORTA_SERIAL,
        framer='rtu',
        baudrate=BAUDRATE,
        bytesize=8,
        parity='N',
        stopbits=1,
        timeout=1
    )

    print(f"Conectando à porta {PORTA_SERIAL} a {BAUDRATE} bps...")

    if not client.connect():
        print(f"Erro crítico: Não foi possível conectar à porta {PORTA_SERIAL}.")
        return

    print("Conexão estabelecida com sucesso!")
    print("Pressione **Ctrl + C** a qualquer momento para interromper a leitura.\n")

    try:
        # ====================================================
        # ACIONAMENTO INICIAL DA COIL 0 (Opcional)
        # ====================================================
        print("--- Acionando a Coil 0 (Ligando a Bomba) ---")
        estado_desejado = True  # True para ligar, False para desligar
        
        resposta_escrita = client.write_coil(address=0, value=estado_desejado, device_id=1)
        if resposta_escrita.isError():
            print(f"Aviso: Erro ao escrever na Coil 0: {resposta_escrita}")
        else:
            print("Comando enviado com sucesso! Entrando em loop de leitura...\n")

        time.sleep(0.5)

        # ====================================================
        # LOOP INFINITO DE LEITURA CONTÍNUA
        # ====================================================
        while True:
            timestamp = time.strftime('%H:%M:%S')
            print(f"[{timestamp}] ----------------------------------------")

            # 1. Leitura do estado da Coil 0
            resposta_coil = client.read_coils(address=0, count=1, device_id=1)
            if resposta_coil.isError():
                print(f"Erro ao ler o Coil 0: {resposta_coil}")
            else:
                bomba_ligada = resposta_coil.bits[0]
                status_bomba = "Ligada (TRUE)" if bomba_ligada else "Desligada (FALSE)"
                print(f" > Coil 0 (Bomba): {status_bomba}")

            # 2. Leitura dos Holding Registers (0 a 4 / 40001 a 40005)
            resposta_regs = client.read_holding_registers(address=0, count=5, device_id=1)
            if resposta_regs.isError():
                print(f"Erro ao ler os Holding Registers: {resposta_regs}")
            else:
                regs = resposta_regs.registers
                
                vazao_lmin = regs[0] / 100.0    # Convertendo centésimos para L/min
                volume_litros = regs[1] / 100.0  # Convertendo centésimos para Litros
                pulsos_intervalo = regs[2]       # Pulsos no intervalo recente
                posicao_servo = regs[3]          # Posição do servo em graus
                reg_extra = regs[4]              # Registrador 4 (livre)

                print(f" > [Reg 0 / 40001] Vazão: {vazao_lmin:.2f} L/min")
                print(f" > [Reg 1 / 40002] Volume Acumulado: {volume_litros:.2f} Litros")
                print(f" > [Reg 2 / 40003] Pulsos no Intervalo: {pulsos_intervalo}")
                print(f" > [Reg 3 / 40004] Posição do Servo: {posicao_servo}°")
                print(f" > [Reg 4 / 40005] Registrador Adicional: {reg_extra}")

            # Intervalo de espera entre cada ciclo de leitura (ex: 1 segundo)
            time.sleep(1.0)

    except KeyboardInterrupt:
        print("\n\nLeitura contínua interrompida pelo usuário (Ctrl + C).")

    except Exception as e:
        print(f"\nOcorreu uma exceção inesperada durante a execução: {e}")

    finally:
        client.close()
        print("Conexão serial encerrada com segurança.")

if __name__ == "__main__":
    monitorar_escravo_continuamente()
