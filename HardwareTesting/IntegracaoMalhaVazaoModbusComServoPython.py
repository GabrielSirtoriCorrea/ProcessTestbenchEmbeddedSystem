import sys
import time
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
    QGridLayout, QLabel, QPushButton, QGroupBox, QLineEdit, QSlider, QSpinBox, QMessageBox
)
from PyQt5.QtCore import QThread, pyqtSignal, Qt
from PyQt5.QtGui import QFont
from pymodbus.client import ModbusSerialClient


# ==============================================================================
# THREAD DE COMUNICAÇÃO MODBUS
# ==============================================================================
class WorkerModbus(QThread):
    dados_recebidos = pyqtSignal(dict)
    erro_conexao = pyqtSignal(str)

    def __init__(self, porta, baudrate, slave_id):
        super().__init__()
        self.porta = porta
        self.baudrate = baudrate
        self.slave_id = slave_id
        self.rodando = True
        self.comando_coil = None      # Para acionamento da Bomba (Coil 0)
        self.comando_servo = None     # Para controle do Servo (Holding Register 4)

    def solicitar_alternancia_coil(self, novo_estado):
        self.comando_coil = novo_estado

    def definir_posicao_servo(self, angulo):
        self.comando_servo = angulo

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
                # 1. Envia comando para a Coil 0 se houver alteração pendente
                if self.comando_coil is not None:
                    client.write_coil(address=0, value=self.comando_coil, device_id=self.slave_id)
                    self.comando_coil = None
                    time.sleep(0.05)

                # 2. Envia comando para o Holding Register 4 (Setpoint do Servo) se houver alteração pendente
                if self.comando_servo is not None:
                    client.write_register(address=4, value=self.comando_servo, device_id=self.slave_id)
                    self.comando_servo = None
                    time.sleep(0.05)

                # 3. Leitura do estado da Coil 0 (Bomba)
                res_coil = client.read_coils(address=0, count=1, device_id=self.slave_id)
                bomba_ligada = False if res_coil.isError() else res_coil.bits[0]

                # 4. Leitura dos Holding Registers (0 a 4)
                res_regs = client.read_holding_registers(address=0, count=5, device_id=self.slave_id)
                
                if not res_regs.isError():
                    regs = res_regs.registers
                    dados = {
                        'status_bomba': bomba_ligada,
                        'vazao': regs[0] / 100.0,
                        'volume': regs[1] / 100.0,
                        'pulsos': regs[2],
                        'servo_atual': regs[3],
                        'servo_setpoint': regs[4]
                    }
                    self.dados_recebidos.emit(dados)
                else:
                    self.erro_conexao.emit("Erro na leitura dos registradores Modbus.")

            except Exception as e:
                self.erro_conexao.emit(f"Exceção na comunicação: {str(e)}")

            time.sleep(0.3)

        client.close()

    def parar(self):
        self.rodando = False
        self.wait()


# ==============================================================================
# JANELA PRINCIPAL DA APLICAÇÃO
# ==============================================================================
class JanelaModbus(QMainWindow):
    def __init__(self):
        super().__init__()
        self.thread_modbus = None
        self.estado_atual_bomba = False
        self.initUI()

    def initUI(self):
        self.setWindowTitle("Monitoramento & Controle Modbus RTU - Malha de Vazão")
        self.setGeometry(100, 100, 520, 620)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # -------------------------------------------------------------
        # 1. Configuração da Porta Serial
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
        # 2. Controle do Atuador (Coil 0 - Bomba)
        # -------------------------------------------------------------
        group_controle = QGroupBox("Controle da Bomba (Coil 0)")
        layout_controle = QVBoxLayout()

        self.btn_bomba = QPushButton("LIGAR BOMBA")
        self.btn_bomba.setFont(QFont("Arial", 11, QFont.Bold))
        self.btn_bomba.setFixedHeight(45)
        self.btn_bomba.setStyleSheet("background-color: #e0e0e0; color: black;")
        self.btn_bomba.setEnabled(False)
        self.btn_bomba.clicked.connect(self.acionar_bomba)

        layout_controle.addWidget(self.btn_bomba)
        group_controle.setLayout(layout_controle)
        main_layout.addWidget(group_controle)

        # -------------------------------------------------------------
        # 3. Controle da Válvula / Servomotor (Holding Register 4)
        # -------------------------------------------------------------
        group_servo = QGroupBox("Controle da Válvula / Servomotor (Holding Register 4 - Setpoint)")
        layout_servo = QHBoxLayout()

        self.slider_servo = QSlider(Qt.Horizontal)
        self.slider_servo.setRange(0, 180)
        self.slider_servo.setValue(0)
        self.slider_servo.setEnabled(False)
        self.slider_servo.setTickPosition(QSlider.TicksBelow)
        self.slider_servo.setTickInterval(15)

        self.spin_servo = QSpinBox()
        self.spin_servo.setRange(0, 180)
        self.spin_servo.setValue(0)
        self.spin_servo.setEnabled(False)
        self.spin_servo.setFixedWidth(60)

        # Sincroniza slider e spinbox
        self.slider_servo.valueChanged.connect(self.spin_servo.setValue)
        self.spin_servo.valueChanged.connect(self.slider_servo.setValue)
        
        # Envia o comando ao soltar ou alterar o valor
        self.spin_servo.valueChanged.connect(self.enviar_posicao_servo)

        layout_servo.addWidget(QLabel("0°"))
        layout_servo.addWidget(self.slider_servo)
        layout_servo.addWidget(QLabel("180°"))
        layout_servo.addWidget(self.spin_servo)

        group_servo.setLayout(layout_servo)
        main_layout.addWidget(group_servo)

        # -------------------------------------------------------------
        # 4. Exibição dos Valores (Holding Registers 0 a 4)
        # -------------------------------------------------------------
        group_dados = QGroupBox("Leitura do Processo (Holding Registers)")
        layout_dados = QGridLayout()

        lbl_vazao_title = QLabel("Vazão Atual (Reg 0):")
        lbl_volume_title = QLabel("Volume Acumulado (Reg 1):")
        lbl_pulsos_title = QLabel("Pulsos (Reg 2):")
        lbl_servo_atual_title = QLabel("Posição Atual do Servo (Reg 3):")
        lbl_servo_set_title = QLabel("Setpoint Registrado (Reg 4):")

        self.val_vazao = QLabel("0.00 L/min")
        self.val_volume = QLabel("0.00 Litros")
        self.val_pulsos = QLabel("0")
        self.val_servo_atual = QLabel("0°")
        self.val_servo_set = QLabel("0°")

        fonte_dados = QFont("Arial", 10, QFont.Bold)
        for val in [self.val_vazao, self.val_volume, self.val_pulsos, self.val_servo_atual, self.val_servo_set]:
            val.setFont(fonte_dados)

        layout_dados.addWidget(lbl_vazao_title, 0, 0)
        layout_dados.addWidget(self.val_vazao, 0, 1)

        layout_dados.addWidget(lbl_volume_title, 1, 0)
        layout_dados.addWidget(self.val_volume, 1, 1)

        layout_dados.addWidget(lbl_pulsos_title, 2, 0)
        layout_dados.addWidget(self.val_pulsos, 2, 1)

        layout_dados.addWidget(lbl_servo_atual_title, 3, 0)
        layout_dados.addWidget(self.val_servo_atual, 3, 1)

        layout_dados.addWidget(lbl_servo_set_title, 4, 0)
        layout_dados.addWidget(self.val_servo_set, 4, 1)

        group_dados.setLayout(layout_dados)
        main_layout.addWidget(group_dados)

        self.statusBar().showMessage("Status: Desconectado")

    # -------------------------------------------------------------
    # Métodos de Controle e Conexão
    # -------------------------------------------------------------
    def alternar_conexao(self):
        if self.thread_modbus is None or not self.thread_modbus.isRunning():
            porta = self.input_porta.text().strip()
            try:
                baud = int(self.input_baud.text().strip())
                slave = int(self.input_slave.text().strip())
            except ValueError:
                QMessageBox.critical(self, "Erro", "Baudrate e Slave ID devem ser números inteiros.")
                return

            self.thread_modbus = WorkerModbus(porta, baud, slave)
            self.thread_modbus.dados_recebidos.connect(self.atualizar_interface)
            self.thread_modbus.erro_conexao.connect(self.tratar_erro)
            self.thread_modbus.start()

            self.btn_conectar.setText("Desconectar")
            self.btn_bomba.setEnabled(True)
            self.slider_servo.setEnabled(True)
            self.spin_servo.setEnabled(True)
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
        self.slider_servo.setEnabled(False)
        self.spin_servo.setEnabled(False)
        self.btn_bomba.setText("LIGAR BOMBA")
        self.btn_bomba.setStyleSheet("background-color: #e0e0e0; color: black;")
        self.input_porta.setEnabled(True)
        self.input_baud.setEnabled(True)
        self.input_slave.setEnabled(True)
        self.statusBar().showMessage("Status: Desconectado")

    def acionar_bomba(self):
        novo_estado = not self.estado_atual_bomba
        if self.thread_modbus:
            self.thread_modbus.solicitar_alternancia_coil(novo_estado)

    def enviar_posicao_servo(self, valor):
        if self.thread_modbus and self.thread_modbus.isRunning():
            self.thread_modbus.definir_posicao_servo(valor)

    def atualizar_interface(self, dados):
        self.estado_atual_bomba = dados['status_bomba']
        if self.estado_atual_bomba:
            self.btn_bomba.setText("DESLIGAR BOMBA")
            self.btn_bomba.setStyleSheet("background-color: #4CAF50; color: white;")
        else:
            self.btn_bomba.setText("LIGAR BOMBA")
            self.btn_bomba.setStyleSheet("background-color: #F44336; color: white;")

        self.val_vazao.setText(f"{dados['vazao']:.2f} L/min")
        self.val_volume.setText(f"{dados['volume']:.2f} Litros")
        self.val_pulsos.setText(str(dados['pulsos']))
        self.val_servo_atual.setText(f"{dados['servo_atual']}°")
        self.val_servo_set.setText(f"{dados['servo_setpoint']}°")

        self.statusBar().showMessage("Status: Conectado e sincronizado com sucesso.")

    def tratar_erro(self, mensagem):
        self.statusBar().showMessage(f"Erro: {mensagem}")

    def closeEvent(self, event):
        self.desconectar()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    janela = JanelaModbus()
    janela.show()
    sys.exit(app.exec_())
