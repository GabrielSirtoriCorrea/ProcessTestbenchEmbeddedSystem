import sys
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
    QGridLayout, QLabel, QPushButton, QGroupBox, QLineEdit, QStatusBar
)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QFont

class JanelaTesteLayout(QMainWindow):
    def __init__(self):
        super().__init__()
        self.estado_bomba_simulado = False
        self.initUI()

    def initUI(self):
        self.setWindowTitle("Teste de Layout - Monitoramento & Controle Modbus")
        self.setGeometry(100, 100, 520, 520)

        # Widget e Layout Principal
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # -------------------------------------------------------------
        # 1. Painel de Configurações de Conexão
        # -------------------------------------------------------------
        group_config = QGroupBox("Configurações de Conexão (Simulação)")
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
        self.btn_conectar.clicked.connect(self.simular_conexao)
        layout_config.addWidget(self.btn_conectar)

        group_config.setLayout(layout_config)
        main_layout.addWidget(group_config)

        # -------------------------------------------------------------
        # 2. Painel de Controle da Coil 0 (Bomba)
        # -------------------------------------------------------------
        group_controle = QGroupBox("Controle do Atuador (Coil 0)")
        layout_controle = QVBoxLayout()

        self.btn_bomba = QPushButton("LIGAR BOMBA")
        self.btn_bomba.setFont(QFont("Arial", 12, QFont.Bold))
        self.btn_bomba.setFixedHeight(50)
        self.btn_bomba.setStyleSheet("background-color: #e0e0e0; color: #555555;")
        self.btn_bomba.setEnabled(False)  # Inicia desabilitado até "conectar"
        self.btn_bomba.clicked.connect(self.simular_alternancia_bomba)

        layout_controle.addWidget(self.btn_bomba)
        group_controle.setLayout(layout_controle)
        main_layout.addWidget(group_controle)

        # -------------------------------------------------------------
        # 3. Painel de Leitura dos Holding Registers
        # -------------------------------------------------------------
        group_dados = QGroupBox("Leitura do Processo (Holding Registers)")
        layout_dados = QGridLayout()

        # Labels de título
        lbl_vazao_title = QLabel("Vazão Atual (Reg 0):")
        lbl_volume_title = QLabel("Volume Acumulado (Reg 1):")
        lbl_pulsos_title = QLabel("Pulsos Último Intervalo (Reg 2):")
        lbl_servo_title = QLabel("Posição do Servo (Reg 3):")
        lbl_extra_title = QLabel("Registrador Adicional (Reg 4):")

        # Labels de exibição dos valores
        self.val_vazao = QLabel("0.00 L/min")
        self.val_volume = QLabel("0.00 Litros")
        self.val_pulsos = QLabel("0")
        self.val_servo = QLabel("0°")
        self.val_extra = QLabel("0")

        # Estilização do texto dos valores
        fonte_valores = QFont("Arial", 11, QFont.Bold)
        for val in [self.val_vazao, self.val_volume, self.val_pulsos, self.val_servo, self.val_extra]:
            val.setFont(fonte_valores)
            val.setStyleSheet("color: #1A237E;")  # Azul escuro

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

        # -------------------------------------------------------------
        # 4. Painel para Testar/Simular Dados na Tela
        # -------------------------------------------------------------
        group_simulacao = QGroupBox("Ferramenta de Teste de Layout")
        layout_simulacao = QHBoxLayout()

        self.btn_gerar_dados = QPushButton("Gerar Valores Aleatórios")
        self.btn_gerar_dados.setEnabled(False)
        self.btn_gerar_dados.clicked.connect(self.simular_dados_recebidos)
        layout_simulacao.addWidget(self.btn_gerar_dados)

        group_simulacao.setLayout(layout_simulacao)
        main_layout.addWidget(group_simulacao)

        # Barra de Status
        self.setStatusBar(QStatusBar())
        self.statusBar().showMessage("Status: Desconectado (Modo de Teste de Interface)")

    # -------------------------------------------------------------
    # Métodos de Simulação de Eventos da Interface
    # -------------------------------------------------------------
    def simular_conexao(self):
        if self.btn_conectar.text() == "Conectar":
            self.btn_conectar.setText("Desconectar")
            self.btn_bomba.setEnabled(True)
            self.btn_gerar_dados.setEnabled(True)
            
            self.input_porta.setEnabled(False)
            self.input_baud.setEnabled(False)
            self.input_slave.setEnabled(False)
            
            self.atualizar_estilo_bomba()
            self.statusBar().showMessage(f"Status: Conectado (Simulado em {self.input_porta.text()})")
        else:
            self.btn_conectar.setText("Conectar")
            self.btn_bomba.setEnabled(False)
            self.btn_gerar_dados.setEnabled(False)
            
            self.input_porta.setEnabled(True)
            self.input_baud.setEnabled(True)
            self.input_slave.setEnabled(True)
            
            self.btn_bomba.setText("LIGAR BOMBA")
            self.btn_bomba.setStyleSheet("background-color: #e0e0e0; color: #555555;")
            self.statusBar().showMessage("Status: Desconectado")

    def simular_alternancia_bomba(self):
        self.estado_bomba_simulado = not self.estado_bomba_simulado
        self.atualizar_estilo_bomba()

    def atualizar_estilo_bomba(self):
        if self.estado_bomba_simulado:
            self.btn_bomba.setText("DESLIGAR BOMBA")
            self.btn_bomba.setStyleSheet("background-color: #4CAF50; color: white;") # Verde
        else:
            self.btn_bomba.setText("LIGAR BOMBA")
            self.btn_bomba.setStyleSheet("background-color: #F44336; color: white;") # Vermelho

    def simular_dados_recebidos(self):
        import random
        # Gera valores numéricos para testar a atualização dos rótulos
        vazao = random.uniform(5.0, 25.5)
        volume = random.uniform(10.0, 150.0)
        pulsos = random.randint(30, 120)
        servo = random.choice([0, 30, 60, 90, 110])
        extra = random.randint(100, 999)

        self.val_vazao.setText(f"{vazao:.2f} L/min")
        self.val_volume.setText(f"{volume:.2f} Litros")
        self.val_pulsos.setText(str(pulsos))
        self.val_servo.setText(f"{servo}°")
        self.val_extra.setText(str(extra))

        self.statusBar().showMessage("Status: Valores atualizados na tela com sucesso!")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    janela = JanelaTesteLayout()
    janela.show()
    sys.exit(app.exec_())
