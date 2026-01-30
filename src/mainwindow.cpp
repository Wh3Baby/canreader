#include "mainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QSettings>
#include <QTableWidgetItem>
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QShortcut>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QEventLoop>
#include "udsprotocol.h"
#include "obd2protocol.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_isConnected(false)
    , m_useTableView(true)
{
    setupUI();
    setupShortcuts();
    loadSettings();
    
    m_canInterface = new CANInterface(this);
    connect(m_canInterface, &CANInterface::messageReceived, 
            this, &MainWindow::onCanMessageReceived);
    connect(m_canInterface, &CANInterface::messageReceivedDetailed,
            this, &MainWindow::onCanMessageReceivedDetailed);
    connect(m_canInterface, &CANInterface::connectionStatusChanged, 
            this, &MainWindow::onConnectionStatusChanged);
    connect(m_canInterface, &CANInterface::errorOccurred, 
            this, &MainWindow::onErrorOccurred);
    connect(m_canInterface, &CANInterface::statisticsUpdated,
            this, &MainWindow::onStatisticsUpdated);
    
    // Инициализация диагностических протоколов
    m_udsProtocol = new UDSProtocol(m_canInterface, this);
    m_obd2Protocol = new OBD2Protocol(m_canInterface, this);
    connect(m_udsProtocol, &UDSProtocol::responseReceived, 
            this, &MainWindow::onDiagnosticResponseReceived);
    connect(m_udsProtocol, &UDSProtocol::errorOccurred, 
            this, &MainWindow::onDiagnosticError);
    connect(m_obd2Protocol, &OBD2Protocol::responseReceived, 
            this, &MainWindow::onDiagnosticResponseReceived);
    connect(m_obd2Protocol, &OBD2Protocol::errorOccurred, 
            this, &MainWindow::onDiagnosticError);
    
    // Автообновление списка портов каждые 5 секунд
    m_autoRefreshTimer = new QTimer(this);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &MainWindow::onAutoRefreshPorts);
    m_autoRefreshTimer->start(5000);
    
    logMessage("Программа запущена. Выберите порт и скорость, затем нажмите 'Подключиться'");
}

MainWindow::~MainWindow()
{
    saveSettings();
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
    m_baudRateCombo->addItem("125", 125);
    m_baudRateCombo->addItem("250", 250);
    m_baudRateCombo->addItem("500", 500);
    m_baudRateCombo->addItem("1000", 1000);
    m_baudRateCombo->setCurrentIndex(1); // 250 по умолчанию
    m_baudRateCombo->setMinimumWidth(100);
    
    m_refreshPortsButton = new QPushButton("Обновить", this);
    m_refreshPortsButton->setToolTip("Обновить список портов");
    connect(m_refreshPortsButton, &QPushButton::clicked, this, &MainWindow::onRefreshPortsClicked);
    
    m_connectButton = new QPushButton("Подключиться", this);
    m_connectButton->setMinimumWidth(120);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    
    connectionLayout->addWidget(portLabel);
    connectionLayout->addWidget(m_portCombo);
    connectionLayout->addWidget(m_refreshPortsButton);
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
    
    // Фильтры
    QGroupBox *filterGroup = new QGroupBox("Фильтры CAN ID", this);
    QHBoxLayout *filterLayout = new QHBoxLayout(filterGroup);
    
    m_filterEnabledCheck = new QCheckBox("Включить фильтрацию", this);
    m_filterIdEdit = new QLineEdit(this);
    m_filterIdEdit->setPlaceholderText("CAN ID (hex)");
    m_filterIdEdit->setMaximumWidth(100);
    m_filterIdEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f]{1,8}"), this));
    m_addFilterButton = new QPushButton("Добавить", this);
    m_clearFiltersButton = new QPushButton("Очистить", this);
    
    connect(m_filterEnabledCheck, &QCheckBox::toggled, this, &MainWindow::onFilterToggled);
    connect(m_addFilterButton, &QPushButton::clicked, this, &MainWindow::onAddFilterClicked);
    connect(m_clearFiltersButton, &QPushButton::clicked, this, &MainWindow::onClearFiltersClicked);
    
    filterLayout->addWidget(m_filterEnabledCheck);
    filterLayout->addWidget(new QLabel("ID:", this));
    filterLayout->addWidget(m_filterIdEdit);
    filterLayout->addWidget(m_addFilterButton);
    filterLayout->addWidget(m_clearFiltersButton);
    filterLayout->addStretch();
    
    // Лог и таблица
    QGroupBox *logGroup = new QGroupBox("Лог сообщений", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    
    QHBoxLayout *logButtonsLayout = new QHBoxLayout();
    m_clearLogButton = new QPushButton("Очистить лог", this);
    m_saveLogButton = new QPushButton("Сохранить лог", this);
    connect(m_clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
    connect(m_saveLogButton, &QPushButton::clicked, this, &MainWindow::onSaveLogClicked);
    logButtonsLayout->addWidget(m_clearLogButton);
    logButtonsLayout->addWidget(m_saveLogButton);
    logButtonsLayout->addStretch();
    
    // Таблица сообщений
    m_messageTable = new QTableWidget(this);
    m_messageTable->setColumnCount(4);
    m_messageTable->setHorizontalHeaderLabels(QStringList() << "Время" << "ID" << "Данные" << "Направление");
    m_messageTable->horizontalHeader()->setStretchLastSection(true);
    m_messageTable->setAlternatingRowColors(true);
    m_messageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_messageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // Текстовый лог
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Courier", 9));
    m_logTextEdit->setMaximumHeight(150);
    m_logTextEdit->hide(); // По умолчанию показываем таблицу
    
    logLayout->addLayout(logButtonsLayout);
    logLayout->addWidget(m_messageTable, 2);
    logLayout->addWidget(m_logTextEdit, 1);
    
    // Статус
    m_statusLabel = new QLabel("Не подключено", this);
    m_statsLabel = new QLabel("", this);
    statusBar()->addWidget(m_statusLabel);
    statusBar()->addPermanentWidget(m_statsLabel);
    
    // Добавление групп в главный layout
    mainLayout->addWidget(connectionGroup);
    mainLayout->addWidget(sendGroup);
    mainLayout->addWidget(filterGroup);
    
    // Диагностика
    setupDiagnosticUI();
    mainLayout->addWidget(m_diagnosticTabs);
    
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
        QString logMsg = QString("Отправлено: ID=0x%1, Данные=%2")
                         .arg(canId, 0, 16).arg(canDataStr.toUpper());
        logMessage(logMsg, "SEND");
        addMessageToTable(canId, data, QDateTime::currentDateTime(), false);
    } else {
        logMessage("Ошибка отправки сообщения", "ERROR");
    }
}

void MainWindow::onCanMessageReceived(const QString &message)
{
    logMessage(message, "RECV");
}

void MainWindow::onCanMessageReceivedDetailed(quint32 id, const QByteArray &data, const QDateTime &timestamp)
{
    addMessageToTable(id, data, timestamp, true);
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

void MainWindow::onRefreshPortsClicked()
{
    m_portCombo->clear();
    QStringList ports = m_canInterface->getAvailablePorts();
    m_portCombo->addItems(ports);
    logMessage(QString("Список портов обновлен. Найдено портов: %1").arg(ports.size()));
}

void MainWindow::onClearLogClicked()
{
    m_logTextEdit->clear();
    m_messageTable->setRowCount(0);
    logMessage("Лог очищен");
}

void MainWindow::onSaveLogClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить лог", 
                                                    QString("can_log_%1.txt")
                                                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
                                                    "Текстовые файлы (*.txt);;CSV файлы (*.csv)");
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        
        if (fileName.endsWith(".csv")) {
            // CSV формат
            out << "Время,ID,Данные,Направление\n";
            for (int i = 0; i < m_messageTable->rowCount(); ++i) {
                out << m_messageTable->item(i, 0)->text() << ","
                    << m_messageTable->item(i, 1)->text() << ","
                    << m_messageTable->item(i, 2)->text() << ","
                    << m_messageTable->item(i, 3)->text() << "\n";
            }
        } else {
            // Текстовый формат
            out << m_logTextEdit->toPlainText();
        }
        
        file.close();
        logMessage(QString("Лог сохранен в файл: %1").arg(fileName), "SUCCESS");
    } else {
        logMessage(QString("Ошибка сохранения файла: %1").arg(file.errorString()), "ERROR");
    }
}

void MainWindow::onFilterToggled(bool enabled)
{
    m_canInterface->setFilterEnabled(enabled);
    logMessage(enabled ? "Фильтрация включена" : "Фильтрация выключена");
}

void MainWindow::onAddFilterClicked()
{
    QString idStr = m_filterIdEdit->text();
    if (idStr.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите CAN ID для фильтра!");
        return;
    }
    
    bool ok;
    quint32 id = idStr.toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат CAN ID!");
        return;
    }
    
    m_canInterface->addFilterId(id, true); // true = разрешить
    logMessage(QString("Добавлен фильтр для ID: 0x%1 (разрешить)").arg(id, 0, 16));
    m_filterIdEdit->clear();
}

void MainWindow::onClearFiltersClicked()
{
    m_canInterface->clearFilters();
    logMessage("Все фильтры очищены");
}

void MainWindow::onStatisticsUpdated()
{
    updateStatisticsDisplay();
}

void MainWindow::updateStatisticsDisplay()
{
    Statistics stats = m_canInterface->getStatistics();
    quint64 mps = m_canInterface->getMessagesPerSecond();
    
    QString statsText = QString("Отправлено: %1 | Принято: %2 | Ошибок: %3 | Скорость: %4 msg/s")
                        .arg(stats.messagesSent)
                        .arg(stats.messagesReceived)
                        .arg(stats.errorsCount)
                        .arg(mps);
    m_statsLabel->setText(statsText);
}

void MainWindow::addMessageToTable(quint32 id, const QByteArray &data, const QDateTime &timestamp, bool isReceived)
{
    int row = m_messageTable->rowCount();
    m_messageTable->insertRow(row);
    
    // Время
    m_messageTable->setItem(row, 0, new QTableWidgetItem(timestamp.toString("hh:mm:ss.zzz")));
    
    // ID
    m_messageTable->setItem(row, 1, new QTableWidgetItem(QString("0x%1").arg(id, 0, 16).toUpper()));
    
    // Данные
    QString dataStr;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) dataStr += " ";
        dataStr += QString("%1").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    m_messageTable->setItem(row, 2, new QTableWidgetItem(dataStr));
    
    // Направление
    m_messageTable->setItem(row, 3, new QTableWidgetItem(isReceived ? "RX" : "TX"));
    
    // Цветовая подсветка
    if (isReceived) {
        m_messageTable->item(row, 3)->setForeground(QBrush(QColor("purple")));
    } else {
        m_messageTable->item(row, 3)->setForeground(QBrush(QColor("blue")));
    }
    
    // Автопрокрутка к последней строке
    m_messageTable->scrollToBottom();
    
    // Ограничение количества строк (удаляем старые)
    const int MAX_ROWS = 1000;
    while (m_messageTable->rowCount() > MAX_ROWS) {
        m_messageTable->removeRow(0);
    }
}

void MainWindow::setupShortcuts()
{
    // Enter для отправки
    QShortcut *sendShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    connect(sendShortcut, &QShortcut::activated, this, &MainWindow::onSendClicked);
    
    // Ctrl+L для очистки лога
    QShortcut *clearShortcut = new QShortcut(QKeySequence("Ctrl+L"), this);
    connect(clearShortcut, &QShortcut::activated, this, &MainWindow::onClearLogClicked);
    
    // F5 для обновления портов
    QShortcut *refreshShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(refreshShortcut, &QShortcut::activated, this, &MainWindow::onRefreshPortsClicked);
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("lastPort", m_portCombo->currentText());
    settings.setValue("lastBaudRate", m_baudRateCombo->currentIndex());
}

void MainWindow::loadSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    // Восстановление последних настроек
    QString lastPort = settings.value("lastPort").toString();
    int lastBaudIndex = settings.value("lastBaudRate", 1).toInt();
    
    if (lastBaudIndex >= 0 && lastBaudIndex < m_baudRateCombo->count()) {
        m_baudRateCombo->setCurrentIndex(lastBaudIndex);
    }
}

void MainWindow::onAutoRefreshPorts()
{
    if (!m_isConnected) {
        // Обновляем список портов только если не подключены
        QStringList ports = m_canInterface->getAvailablePorts();
        QString currentPort = m_portCombo->currentText();
        
        m_portCombo->clear();
        m_portCombo->addItems(ports);
        
        // Восстанавливаем выбранный порт если он еще существует
        int index = m_portCombo->findText(currentPort);
        if (index >= 0) {
            m_portCombo->setCurrentIndex(index);
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F5) {
        onRefreshPortsClicked();
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (m_isConnected) {
        m_canInterface->disconnect();
    }
    event->accept();
}

void MainWindow::setupDiagnosticUI()
{
    m_diagnosticTabs = new QTabWidget(this);
    
    // UDS вкладка
    m_udsGroup = new QGroupBox("UDS (ISO 14229)", this);
    QVBoxLayout *udsLayout = new QVBoxLayout(m_udsGroup);
    
    // Чтение DID
    QHBoxLayout *readDIDLayout = new QHBoxLayout();
    readDIDLayout->addWidget(new QLabel("DID (hex):", this));
    m_udsDIDEdit = new QLineEdit(this);
    m_udsDIDEdit->setPlaceholderText("F190");
    m_udsDIDEdit->setMaximumWidth(100);
    QPushButton *readDIDBtn = new QPushButton("Читать DID", this);
    connect(readDIDBtn, &QPushButton::clicked, this, &MainWindow::onUDSReadDID);
    readDIDLayout->addWidget(m_udsDIDEdit);
    readDIDLayout->addWidget(readDIDBtn);
    readDIDLayout->addStretch();
    
    // Запись DID
    QHBoxLayout *writeDIDLayout = new QHBoxLayout();
    writeDIDLayout->addWidget(new QLabel("Данные (hex):", this));
    m_udsDataEdit = new QLineEdit(this);
    m_udsDataEdit->setPlaceholderText("01 02 03");
    QPushButton *writeDIDBtn = new QPushButton("Записать DID", this);
    connect(writeDIDBtn, &QPushButton::clicked, this, &MainWindow::onUDSWriteDID);
    writeDIDLayout->addWidget(m_udsDataEdit);
    writeDIDLayout->addWidget(writeDIDBtn);
    writeDIDLayout->addStretch();
    
    // Чтение памяти
    QHBoxLayout *readMemLayout = new QHBoxLayout();
    readMemLayout->addWidget(new QLabel("Адрес:", this));
    m_udsAddressEdit = new QLineEdit(this);
    m_udsAddressEdit->setPlaceholderText("0x12345678");
    m_udsAddressEdit->setMaximumWidth(150);
    readMemLayout->addWidget(m_udsAddressEdit);
    readMemLayout->addWidget(new QLabel("Длина:", this));
    m_udsLengthEdit = new QLineEdit(this);
    m_udsLengthEdit->setPlaceholderText("16");
    m_udsLengthEdit->setMaximumWidth(80);
    QPushButton *readMemBtn = new QPushButton("Читать память", this);
    connect(readMemBtn, &QPushButton::clicked, this, &MainWindow::onUDSReadMemory);
    readMemLayout->addWidget(m_udsLengthEdit);
    readMemLayout->addWidget(readMemBtn);
    readMemLayout->addStretch();
    
    // Безопасный доступ
    QHBoxLayout *securityLayout = new QHBoxLayout();
    securityLayout->addWidget(new QLabel("Уровень:", this));
    m_udsSecurityLevelEdit = new QLineEdit(this);
    m_udsSecurityLevelEdit->setPlaceholderText("1");
    m_udsSecurityLevelEdit->setMaximumWidth(80);
    QPushButton *securityBtn = new QPushButton("Безопасный доступ", this);
    connect(securityBtn, &QPushButton::clicked, this, &MainWindow::onUDSSecurityAccess);
    securityLayout->addWidget(m_udsSecurityLevelEdit);
    securityLayout->addWidget(securityBtn);
    securityLayout->addStretch();
    
    // Сессия
    QHBoxLayout *sessionLayout = new QHBoxLayout();
    sessionLayout->addWidget(new QLabel("Сессия:", this));
    m_udsSessionEdit = new QLineEdit(this);
    m_udsSessionEdit->setPlaceholderText("1=Default, 2=Programming, 3=Extended");
    QPushButton *sessionBtn = new QPushButton("Начать сессию", this);
    connect(sessionBtn, &QPushButton::clicked, this, &MainWindow::onUDSStartSession);
    sessionLayout->addWidget(m_udsSessionEdit);
    sessionLayout->addWidget(sessionBtn);
    sessionLayout->addStretch();
    
    // DTC
    QHBoxLayout *dtcLayout = new QHBoxLayout();
    QPushButton *readDTCBtn = new QPushButton("Читать DTC", this);
    QPushButton *clearDTCBtn = new QPushButton("Очистить DTC", this);
    connect(readDTCBtn, &QPushButton::clicked, this, &MainWindow::onUDSReadDTC);
    connect(clearDTCBtn, &QPushButton::clicked, this, &MainWindow::onUDSClearDTC);
    dtcLayout->addWidget(readDTCBtn);
    dtcLayout->addWidget(clearDTCBtn);
    dtcLayout->addStretch();
    
    udsLayout->addLayout(readDIDLayout);
    udsLayout->addLayout(writeDIDLayout);
    udsLayout->addLayout(readMemLayout);
    udsLayout->addLayout(securityLayout);
    udsLayout->addLayout(sessionLayout);
    udsLayout->addLayout(dtcLayout);
    
    // OBD-II вкладка
    m_obd2Group = new QGroupBox("OBD-II (SAE J1979)", this);
    QVBoxLayout *obd2Layout = new QVBoxLayout(m_obd2Group);
    
    // Режим и PID
    QHBoxLayout *pidLayout = new QHBoxLayout();
    pidLayout->addWidget(new QLabel("Режим:", this));
    m_obd2ModeCombo = new QComboBox(this);
    m_obd2ModeCombo->addItem("01 - Текущие данные", 0x01);
    m_obd2ModeCombo->addItem("03 - Сохраненные DTC", 0x03);
    m_obd2ModeCombo->addItem("04 - Очистить DTC", 0x04);
    m_obd2ModeCombo->addItem("07 - Ожидающие DTC", 0x07);
    m_obd2ModeCombo->addItem("09 - Информация", 0x09);
    pidLayout->addWidget(m_obd2ModeCombo);
    pidLayout->addWidget(new QLabel("PID (hex):", this));
    m_obd2PIDEdit = new QLineEdit(this);
    m_obd2PIDEdit->setPlaceholderText("0C (RPM), 0D (Speed)");
    m_obd2PIDEdit->setMaximumWidth(150);
    QPushButton *readPIDBtn = new QPushButton("Читать PID", this);
    connect(readPIDBtn, &QPushButton::clicked, this, &MainWindow::onOBD2ReadPID);
    pidLayout->addWidget(m_obd2PIDEdit);
    pidLayout->addWidget(readPIDBtn);
    pidLayout->addStretch();
    
    // Быстрые команды
    QHBoxLayout *quickLayout = new QHBoxLayout();
    QPushButton *readDTCBtn2 = new QPushButton("Читать DTC", this);
    QPushButton *clearDTCBtn2 = new QPushButton("Очистить DTC", this);
    QPushButton *readVINBtn = new QPushButton("Читать VIN", this);
    connect(readDTCBtn2, &QPushButton::clicked, this, &MainWindow::onOBD2ReadDTC);
    connect(clearDTCBtn2, &QPushButton::clicked, this, &MainWindow::onOBD2ClearDTC);
    connect(readVINBtn, &QPushButton::clicked, this, &MainWindow::onOBD2ReadVIN);
    quickLayout->addWidget(readDTCBtn2);
    quickLayout->addWidget(clearDTCBtn2);
    quickLayout->addWidget(readVINBtn);
    quickLayout->addStretch();
    
    obd2Layout->addLayout(pidLayout);
    obd2Layout->addLayout(quickLayout);
    
    // Вывод диагностики
    m_diagnosticOutput = new QTextBrowser(this);
    m_diagnosticOutput->setMaximumHeight(150);
    m_diagnosticOutput->setFont(QFont("Courier", 9));
    
    udsLayout->addWidget(m_diagnosticOutput);
    obd2Layout->addWidget(m_diagnosticOutput);
    
    m_diagnosticTabs->addTab(m_udsGroup, "UDS");
    m_diagnosticTabs->addTab(m_obd2Group, "OBD-II");
}

void MainWindow::onUDSReadDID()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    bool ok;
    quint16 did = m_udsDIDEdit->text().toUShort(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат DID!");
        return;
    }
    
    QByteArray response;
    if (m_udsProtocol->readDataByIdentifier(did, response)) {
        QString hex = response.toHex(' ').toUpper();
        m_diagnosticOutput->append(QString("UDS: Чтение DID 0x%1: %2")
                                   .arg(did, 4, 16, QChar('0')).arg(hex));
    } else {
        m_diagnosticOutput->append(QString("UDS: Ошибка чтения DID 0x%1").arg(did, 4, 16, QChar('0')));
    }
}

void MainWindow::onUDSWriteDID()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    bool ok;
    quint16 did = m_udsDIDEdit->text().toUShort(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат DID!");
        return;
    }
    
    QByteArray data;
    QStringList bytes = m_udsDataEdit->text().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString &byte : bytes) {
        quint8 value = byte.toUInt(&ok, 16);
        if (ok) {
            data.append(value);
        }
    }
    
    if (m_udsProtocol->writeDataByIdentifier(did, data)) {
        m_diagnosticOutput->append(QString("UDS: Запись DID 0x%1 успешна").arg(did, 4, 16, QChar('0')));
    } else {
        m_diagnosticOutput->append(QString("UDS: Ошибка записи DID 0x%1").arg(did, 4, 16, QChar('0')));
    }
}

void MainWindow::onUDSReadMemory()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    bool ok;
    quint32 address = m_udsAddressEdit->text().toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат адреса!");
        return;
    }
    
    quint32 length = m_udsLengthEdit->text().toUInt(&ok, 10);
    if (!ok || length == 0) {
        QMessageBox::warning(this, "Ошибка", "Неверная длина!");
        return;
    }
    
    QByteArray data;
    if (m_udsProtocol->readMemoryByAddress(address, length, data)) {
        QString hex = data.toHex(' ').toUpper();
        m_diagnosticOutput->append(QString("UDS: Память 0x%1 (%2 байт): %3")
                                   .arg(address, 8, 16, QChar('0')).arg(length).arg(hex));
    } else {
        m_diagnosticOutput->append(QString("UDS: Ошибка чтения памяти"));
    }
}

void MainWindow::onUDSWriteMemory()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    bool ok;
    quint32 address = m_udsAddressEdit->text().toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат адреса!");
        return;
    }
    
    QByteArray data;
    QStringList bytes = m_udsDataEdit->text().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString &byte : bytes) {
        quint8 value = byte.toUInt(&ok, 16);
        if (ok) {
            data.append(value);
        }
    }
    
    if (data.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите данные для записи!");
        return;
    }
    
    if (m_udsProtocol->writeMemoryByAddress(address, data)) {
        m_diagnosticOutput->append(QString("UDS: Запись в память 0x%1 успешна").arg(address, 8, 16, QChar('0')));
    } else {
        m_diagnosticOutput->append(QString("UDS: Ошибка записи в память"));
    }
}

void MainWindow::onUDSSecurityAccess()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    bool ok;
    quint8 level = m_udsSecurityLevelEdit->text().toUInt(&ok, 10);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка", "Неверный уровень!");
        return;
    }
    
    QByteArray seed;
    if (m_udsProtocol->requestSeed(level, seed)) {
        QByteArray key = UDSProtocol::calculateKey(seed);
        if (m_udsProtocol->sendKey(level, key)) {
            m_diagnosticOutput->append(QString("UDS: Безопасный доступ уровень %1 получен").arg(level));
        } else {
            m_diagnosticOutput->append(QString("UDS: Ошибка отправки ключа"));
        }
    } else {
        m_diagnosticOutput->append(QString("UDS: Ошибка запроса seed"));
    }
}

void MainWindow::onUDSStartSession()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    bool ok;
    quint8 session = m_udsSessionEdit->text().toUInt(&ok, 10);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка", "Неверный номер сессии!");
        return;
    }
    
    if (m_udsProtocol->startSession(session)) {
        m_diagnosticOutput->append(QString("UDS: Сессия %1 начата").arg(session));
    } else {
        m_diagnosticOutput->append(QString("UDS: Ошибка начала сессии"));
    }
}

void MainWindow::onUDSClearDTC()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    if (m_udsProtocol->clearDTC()) {
        m_diagnosticOutput->append("UDS: DTC очищены");
    } else {
        m_diagnosticOutput->append("UDS: Ошибка очистки DTC");
    }
}

void MainWindow::onUDSReadDTC()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    QList<DTCCode> dtcList;
    if (m_udsProtocol->readDTCByStatus(0xFF, dtcList)) {
        m_diagnosticOutput->append(QString("UDS: Найдено %1 DTC:").arg(dtcList.size()));
        for (const DTCCode &dtc : dtcList) {
            m_diagnosticOutput->append(QString("  %1 - %2 (%3)")
                                      .arg(UDSProtocol::formatDTC(dtc.code))
                                      .arg(dtc.description)
                                      .arg(dtc.isActive ? "Активен" : "Неактивен"));
        }
    } else {
        m_diagnosticOutput->append("UDS: Ошибка чтения DTC");
    }
}

void MainWindow::onOBD2ReadPID()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    quint8 mode = m_obd2ModeCombo->currentData().toUInt();
    bool ok;
    quint8 pid = m_obd2PIDEdit->text().toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Ошибка", "Неверный формат PID!");
        return;
    }
    
    OBD2Value value;
    if (m_obd2Protocol->readPID(mode, pid, value)) {
        m_diagnosticOutput->append(QString("OBD-II: %1 = %2")
                                  .arg(value.name).arg(value.value));
    } else {
        m_diagnosticOutput->append(QString("OBD-II: Ошибка чтения PID 0x%1").arg(pid, 2, 16, QChar('0')));
    }
}

void MainWindow::onOBD2ReadDTC()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    QList<QString> dtcList;
    if (m_obd2Protocol->readStoredDTC(dtcList)) {
        m_diagnosticOutput->append(QString("OBD-II: Найдено %1 DTC:").arg(dtcList.size()));
        for (const QString &dtc : dtcList) {
            m_diagnosticOutput->append(QString("  %1").arg(dtc));
        }
    } else {
        m_diagnosticOutput->append("OBD-II: Ошибка чтения DTC");
    }
}

void MainWindow::onOBD2ClearDTC()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    if (m_obd2Protocol->clearDTC()) {
        m_diagnosticOutput->append("OBD-II: DTC очищены");
    } else {
        m_diagnosticOutput->append("OBD-II: Ошибка очистки DTC");
    }
}

void MainWindow::onOBD2ReadVIN()
{
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    QString vin;
    if (m_obd2Protocol->readVIN(vin)) {
        m_diagnosticOutput->append(QString("OBD-II: VIN = %1").arg(vin));
    } else {
        m_diagnosticOutput->append("OBD-II: Ошибка чтения VIN");
    }
}

void MainWindow::onDiagnosticResponseReceived(const QByteArray &response)
{
    QString hex = response.toHex(' ').toUpper();
    m_diagnosticOutput->append(QString("Ответ: %1").arg(hex));
}

void MainWindow::onDiagnosticError(const QString &error)
{
    m_diagnosticOutput->append(QString("Ошибка: %1").arg(error));
}

void MainWindow::onOBD2ReadMultiplePIDs()
{
    // TODO: Реализовать чтение нескольких PID
    QMessageBox::information(this, "Info", "Функция в разработке");
}

