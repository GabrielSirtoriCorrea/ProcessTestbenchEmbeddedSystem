import sys
import time
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
    QGridLayout, QLabel, QPushButton, QGroupBox, QLineEdit, QMessageBox
)
from PyQt5.QtCore import QThread, pyqtSignal, Qt
from PyQt5.QtGui import QFont
from pymodbus.client import ModbusSerialClient


# ==============================================================================
# THREAD DE COMUNICAÇÃO MODBUS (Evita que a interface congele durante as leituras)
# ==============================================================================
class WorkerModbus(QThread):
    # Sinais para atualizar a interface gráfica
    dados_recebidos = pyqtSignal(dict)
    erro_conexao = pyqtSignal(str)

    def __init__(self, porta, baudrate, slave_id):
        super().__init__()
        self.porta = porta
        self.baudrate = baudrate
        self.slave_id = slave_id
        self.rodando = True
        self.comando_coil = None  # Armazena solicitação de alteração da Coil 0

    def solicitar_alternancia_coil(self, novo_estado):
        """Define o comando para alterar o estado da Coil 0 no próximo ciclo."""
        self.comando_coil = novo_estado

    def run(self):
        client = ModbusSerialClient(
            port=self.porta,
            framer='rtu',
            baudrate=self.baudrate,
            bytesize=8,
            parity='N',
            stopbits=1,
            timeout=1
        )

        if not client.connect():
            self.erro_conexao.emit(f"Não foi possível conectar à porta {self.porta}.")
            return

        while self.rodando:
            try:
                # 1. Executa comando de escrita na Coil 0 (se houver solicitação pendente)
                if self.comando_coil is not None:
                    client.write_coil(address=0, value=self.comando_coil, device_id=self.slave_id)
                    self.comando_coil = None
                    time.sleep(0.1)

                # 2. Leitura do estado da Coil 0
                res_coil = client.read_coils(address=0, count=1, device_id=self.slave_id)
                bomba_ligada = False if res_coil.isError() else res_coil.bits[0]

                # 3. Leitura dos Holding Registers (0 a 4)
                res_regs = client.read_holding_registers(address=0, count=5, device_id=self.slave_id)
                
                if not res_regs.isError():
                    regs = res_regs.registers
                    dados = {
                        'status_bomba': bomba_ligada,
                        'vazao': regs[0] / 100.0,
                        'volume': regs[1] / 100.0,
                        'pulsos': regs[2],
                        'servo': regs[3],
                        'reg_extra': regs[4]
                    }
                    self.dados_recebidos.emit(dados)
                else:
                    self.erro_conexao.emit("Erro na leitura dos registradores Modbus.")

            except Exception as e:
                self.erro_conexao.emit(f"Exceção na comunicação: {str(e)}")

            time.sleep(0.5)  # Intervalo entre leituras (500 ms)

        client.close()

    def parar(self):
        self.rodando = False
        self.wait()


# ==============================================================================
# JANELA PRINCIPAL DA APLICAÇÃO (PyQt5)
# ==============================================================================
class JanelaModbus(QMainWindow):
    def __init__(self):
        super().__init__()
        self.thread_modbus = None
        self.estado_atual_bomba = False
        self.initUI()

    def initUI(self):
        self.setWindowTitle("Monitoramento & Controle Modbus RTU (RS485)")
        self.setGeometry(100, 100, 500, 500)

        # Widget Central
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # -------------------------------------------------------------
        # PAINEL 1: Configuração da Porta Serial
        # -------------------------------------------------------------
        group_config = QGroupBox("Configurações de Conexão")
        layout_config = QHBoxLayout()

        layout_config.addWidget(QLabel("Porta:"))
        self.input_porta = QLineEdit("COM3")
        layout_config.addWidget(self.input_porta)

        layout_config.addWidget(QLabel("Baudrate:"))
        self.input_baud = QLineEdit("9600")
        layout_config.addWidget(self.input_baud)

        layout_config.addWidget(QLabel("Slave ID:"))
        self.input_slave = QLineEdit("1")
        self.input_slave.setFixedWidth(40)
        layout_config.addWidget(self.input_slave)

        self.btn_conectar = QPushButton("Conectar")
        self.btn_conectar.clicked.connect(self.alternar_conexao)
        layout_config.addWidget(self.btn_conectar)

        group_config.setLayout(layout_config)
        main_layout.addWidget(group_config)

        # -------------------------------------------------------------
        # PAINEL 2: Controle do Atuador (Coil 0)
        # -------------------------------------------------------------
        group_controle = QGroupBox("Controle do Atuador (Coil 0)")
        layout_controle = QVBoxLayout()

        self.btn_bomba = QPushButton("LIGAR BOMBA")
        self.btn_bomba.setFont(QFont("Arial", 12, QFont.Bold))
        self.btn_bomba.setFixedHeight(50)
        self.btn_bomba.setStyleSheet("background-color: #e0e0e0; color: black;")
        self.btn_bomba.setEnabled(False)
        self.btn_bomba.clicked.connect(self.acionar_bomba)

        layout_controle.addWidget(self.btn_bomba)
        group_controle.setLayout(layout_controle)
        main_layout.addWidget(group_controle)

        # -------------------------------------------------------------
        # PAINEL 3: Exibição dos Valores (Holding Registers)
        # -------------------------------------------------------------
        group_dados = QGroupBox("Leitura do Processo (Holding Registers)")
        layout_dados = QGridLayout()

        # Rótulos
        lbl_vazao_title = QLabel("Vazão Atual:")
        lbl_volume_title = QLabel("Volume Acumulado:")
        lbl_pulsos_title = QLabel("Pulsos (Último Intervalo):")
        lbl_servo_title = QLabel("Posição do Servo:")
        lbl_extra_title = QLabel("Registrador Adicional (40005):")

        # Valores
        self.val_vazao = QLabel("0.00 L/min")
        self.val_volume = QLabel("0.00 Litros")
        self.val_pulsos = QLabel("0")
        self.val_servo = QLabel("0°")
        self.val_extra = QLabel("0")

        # Formatação de destaque para os dados
        fonte_dados = QFont("Arial", 11, QFont.Bold)
        for val in [self.val_vazao, self.val_volume, self.val_pulsos, self.val_servo, self.val_extra]:
            val.setFont(fonte_dados)

        # Adicionando ao Grid
        layout_dados.addWidget(lbl_vazao_title, 0, 0)
        layout_dados.addWidget(self.val_vazao, 0, 1)

        layout_dados.addWidget(lbl_volume_title, 1, 0)
        layout_dados.addWidget(self.val_volume, 1, 1)

        layout_dados.addWidget(lbl_pulsos_title, 2, 0)
        layout_dados.addWidget(self.val_pulsos, 2, 1)

        layout_dados.addWidget(lbl_servo_title, 3, 0)
        layout_dados.addWidget(self.val_servo, 3, 1)

        layout_dados.addWidget(lbl_extra_title, 4, 0)
        layout_dados.addWidget(self.val_extra, 4, 1)

        group_dados.setLayout(layout_dados)
        main_layout.addWidget(group_dados)

        # Barra de Status
        self.statusBar().showMessage("Status: Desconectado")

    # -------------------------------------------------------------
    # LÓGICA DE INTERAÇÃO
    # -------------------------------------------------------------
    def alternar_conexao(self):
        if self.thread_modbus is None or not self.thread_modbus.isRunning():
            porta = self.input_porta.text().strip()
            try:
                baud = int(self.input_baud.text().strip())
                slave = int(self.input_slave.text().strip())
            except ValueError:
                QMessageBox.critical(self, "Erro de Entrada", "Baudrate e Slave ID devem ser números inteiros.")
                return

            # Inicia a thread de comunicação Modbus
            self.thread_modbus = WorkerModbus(porta, baud, slave)
            self.thread_modbus.dados_recebidos.connect(self.atualizar_interface)
            self.thread_modbus.erro_conexao.connect(self.tratar_erro)
            self.thread_modbus.start()

            self.btn_conectar.setText("Desconectar")
            self.btn_bomba.setEnabled(True)
            self.input_porta.setEnabled(False)
            self.input_baud.setEnabled(False)
            self.input_slave.setEnabled(False)
            self.statusBar().showMessage(f"Conectando em {porta}...")
        else:
            self.desconectar()

    def desconectar(self):
        if self.thread_modbus:
            self.thread_modbus.parar()
            self.thread_modbus = None

        self.btn_conectar.setText("Conectar")
        self.btn_bomba.setEnabled(False)
        self.btn_bomba.setText("LIGAR BOMBA")
        self.btn_bomba.setStyleSheet("background-color: #e0e0e0; color: black;")
        self.input_porta.setEnabled(True)
        self.input_baud.setEnabled(True)
        self.input_slave.setEnabled(True)
        self.statusBar().showMessage("Status: Desconectado")

    def acionar_bomba(self):
        """Solicita a alternância de estado da Coil 0."""
        novo_estado = not self.estado_atual_bomba
        if self.thread_modbus:
            self.thread_modbus.solicitar_alternancia_coil(novo_estado)

    def atualizar_interface(self, dados):
        # Atualiza o estado visual da Coil 0 (Bomba)
        self.estado_atual_bomba = dados['status_bomba']
        if self.estado_atual_bomba:
            self.btn_bomba.setText("DESLIGAR BOMBA")
            self.btn_bomba.setStyleSheet("background-color: #4CAF50; color: white;") # Verde quando ligada
        else:
            self.btn_bomba.setText("LIGAR BOMBA")
            self.btn_bomba.setStyleSheet("background-color: #f44336; color: white;") # Vermelho quando desligada

        # Atualiza as labels dos registradores
        self.val_vazao.setText(f"{dados['vazao']:.2f} L/min")
        self.val_volume.setText(f"{dados['volume']:.2f} Litros")
        self.val_pulsos.setText(str(dados['pulsos']))
        self.val_servo.setText(f"{dados['servo']}°")
        self.val_extra.setText(str(dados['reg_extra']))

        self.statusBar().showMessage("Status: Conectado e lendo dados...")

    def tratar_erro(self, mensagem):
        self.statusBar().showMessage(f"Erro: {mensagem}")

    def closeEvent(self, event):
        """Garante o encerramento correto da thread ao fechar a janela."""
        self.desconectar()
        event.accept()


# ==============================================================================
# EXECUÇÃO DA APLICAÇÃO
# ==============================================================================
if __name__ == "__main__":
    app = QApplication(sys.argv)
    janela = JanelaModbus()
    janela.show()
    sys.exit(app.exec_())
