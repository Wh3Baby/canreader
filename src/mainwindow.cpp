#include "mainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_isConnected(false)
{
    setupUI();
    
    m_canInterface = new CANInterface(this);
    connect(m_canInterface, &CANInterface::messageReceived, 
            this, &MainWindow::onCanMessageReceived);
    connect(m_canInterface, &CANInterface::connectionStatusChanged, 
            this, &MainWindow::onConnectionStatusChanged);
    connect(m_canInterface, &CANInterface::errorOccurred, 
            this, &MainWindow::onErrorOccurred);
    
    logMessage("Программа запущена. Выберите порт и скорость, затем нажмите 'Подключиться'");
}

MainWindow::~MainWindow()
{
    if (m_isConnected) {
        m_canInterface->disconnect();
    }
}

void MainWindow::setupUI()
{
    setWindowTitle("CAN Reader - Scanmatic 2 Pro");
    setMinimumSize(800, 600);
    resize(900, 700);
    
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Группа подключения
    QGroupBox *connectionGroup = new QGroupBox("Подключение", this);
    QHBoxLayout *connectionLayout = new QHBoxLayout(connectionGroup);
    
    QLabel *portLabel = new QLabel("Последовательный порт:", this);
    m_portCombo = new QComboBox(this);
    m_portCombo->setMinimumWidth(150);
    
    QLabel *baudLabel = new QLabel("Скорость (кбит/с):", this);
    m_baudRateCombo = new QComboBox(this);
    m_baudRateCombo->addItem("250", 250);
    m_baudRateCombo->addItem("500", 500);
    m_baudRateCombo->setCurrentIndex(0);
    m_baudRateCombo->setMinimumWidth(100);
    
    m_connectButton = new QPushButton("Подключиться", this);
    m_connectButton->setMinimumWidth(120);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    
    connectionLayout->addWidget(portLabel);
    connectionLayout->addWidget(m_portCombo);
    connectionLayout->addWidget(baudLabel);
    connectionLayout->addWidget(m_baudRateCombo);
    connectionLayout->addWidget(m_connectButton);
    connectionLayout->addStretch();
    
    // Группа отправки сообщений
    QGroupBox *sendGroup = new QGroupBox("Отправка CAN сообщения", this);
    QVBoxLayout *sendLayout = new QVBoxLayout(sendGroup);
    
    QHBoxLayout *canIdLayout = new QHBoxLayout();
    QLabel *canIdLabel = new QLabel("CAN ID (hex):", this);
    m_canIdEdit = new QLineEdit(this);
    m_canIdEdit->setPlaceholderText("123");
    m_canIdEdit->setMaximumWidth(100);
    QRegularExpression hexRegExp("[0-9A-Fa-f]{1,8}");
    m_canIdEdit->setValidator(new QRegularExpressionValidator(hexRegExp, this));
    canIdLayout->addWidget(canIdLabel);
    canIdLayout->addWidget(m_canIdEdit);
    canIdLayout->addStretch();
    
    QHBoxLayout *canDataLayout = new QHBoxLayout();
    QLabel *canDataLabel = new QLabel("Данные (hex, через пробел):", this);
    m_canDataEdit = new QLineEdit(this);
    m_canDataEdit->setPlaceholderText("01 02 03 04 05 06 07 08");
    QRegularExpression dataRegExp("([0-9A-Fa-f]{2}\\s?)*");
    m_canDataEdit->setValidator(new QRegularExpressionValidator(dataRegExp, this));
    canDataLayout->addWidget(canDataLabel);
    canDataLayout->addWidget(m_canDataEdit);
    
    m_sendButton = new QPushButton("Отправить", this);
    m_sendButton->setMinimumWidth(120);
    m_sendButton->setEnabled(false);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    
    sendLayout->addLayout(canIdLayout);
    sendLayout->addLayout(canDataLayout);
    sendLayout->addWidget(m_sendButton, 0, Qt::AlignRight);
    
    // Лог
    QGroupBox *logGroup = new QGroupBox("Лог сообщений", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Courier", 9));
    logLayout->addWidget(m_logTextEdit);
    
    // Статус
    m_statusLabel = new QLabel("Не подключено", this);
    statusBar()->addWidget(m_statusLabel);
    
    // Добавление групп в главный layout
    mainLayout->addWidget(connectionGroup);
    mainLayout->addWidget(sendGroup);
    mainLayout->addWidget(logGroup);
    
    // Обновление списка портов
    m_canInterface->refreshPortList();
    QStringList ports = m_canInterface->getAvailablePorts();
    m_portCombo->addItems(ports);
}

void MainWindow::onConnectClicked()
{
    if (!m_isConnected) {
        QString portName = m_portCombo->currentText();
        if (portName.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Выберите последовательный порт!");
            return;
        }
        
        int baudRate = m_baudRateCombo->currentData().toInt();
        logMessage(QString("Попытка подключения к %1 со скоростью %2 кбит/с...")
                   .arg(portName).arg(baudRate));
        
        if (m_canInterface->connect(portName, baudRate)) {
            m_isConnected = true;
            m_connectButton->setText("Отключиться");
            m_portCombo->setEnabled(false);
            m_baudRateCombo->setEnabled(false);
            m_sendButton->setEnabled(true);
            logMessage("Подключение установлено успешно", "SUCCESS");
        } else {
            logMessage("Ошибка подключения", "ERROR");
        }
    } else {
        m_canInterface->disconnect();
        m_isConnected = false;
        m_connectButton->setText("Подключиться");
        m_portCombo->setEnabled(true);
        m_baudRateCombo->setEnabled(true);
        m_sendButton->setEnabled(false);
        logMessage("Отключено от адаптера", "INFO");
    }
}

void MainWindow::onSendClicked()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь к адаптеру!");
        return;
    }
    
    QString canIdStr = m_canIdEdit->text();
    QString canDataStr = m_canDataEdit->text();
    
    if (canIdStr.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите CAN ID!");
        return;
    }
    
    bool ok;
    quint32 canId = canIdStr.toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат CAN ID!");
        return;
    }
    
    // Парсинг данных
    QByteArray data;
    if (!canDataStr.isEmpty()) {
        QStringList bytes = canDataStr.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        for (const QString &byte : bytes) {
            bool byteOk;
            quint8 value = byte.toUInt(&byteOk, 16);
            if (byteOk) {
                data.append(value);
            }
        }
    }
    
    if (data.size() > 8) {
        QMessageBox::warning(this, "Ошибка", "CAN сообщение не может содержать более 8 байт!");
        return;
    }
    
    if (m_canInterface->sendMessage(canId, data)) {
        logMessage(QString("Отправлено: ID=0x%1, Данные=%2")
                   .arg(canId, 0, 16).arg(canDataStr.toUpper()), "SEND");
    } else {
        logMessage("Ошибка отправки сообщения", "ERROR");
    }
}

void MainWindow::onCanMessageReceived(const QString &message)
{
    logMessage(message, "RECV");
}

void MainWindow::onConnectionStatusChanged(bool connected)
{
    m_isConnected = connected;
    if (connected) {
        m_statusLabel->setText("Подключено");
        m_statusLabel->setStyleSheet("color: green;");
    } else {
        m_statusLabel->setText("Не подключено");
        m_statusLabel->setStyleSheet("color: red;");
    }
}

void MainWindow::onErrorOccurred(const QString &error)
{
    logMessage(QString("ОШИБКА: %1").arg(error), "ERROR");
}

void MainWindow::logMessage(const QString &message, const QString &type)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString color;
    
    if (type == "ERROR") {
        color = "red";
    } else if (type == "SUCCESS") {
        color = "green";
    } else if (type == "SEND") {
        color = "blue";
    } else if (type == "RECV") {
        color = "purple";
    } else {
        color = "black";
    }
    
    QString formattedMessage = QString("[%1] <span style='color: %2;'><b>%3</b></span> %4")
                               .arg(timestamp, color, type, message);
    
    m_logTextEdit->append(formattedMessage);
    
    // Автопрокрутка вниз
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}

