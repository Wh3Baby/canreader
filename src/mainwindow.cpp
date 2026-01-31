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
    setMinimumSize(1000, 700);
    resize(1200, 800);
    
    // Применение темной цветовой палитры
    setStyleSheet(
        "QMainWindow { background-color: #1E1E1E; }"
        "QGroupBox { font-weight: 500; border: 1px solid #3C3C3C; border-radius: 6px; margin-top: 12px; padding-top: 12px; background-color: #252526; color: #CCCCCC; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #858585; }"
        "QPushButton { background-color: #0E639C; color: #FFFFFF; border: none; padding: 8px 18px; border-radius: 5px; font-weight: 500; min-width: 100px; }"
        "QPushButton:hover { background-color: #1177BB; }"
        "QPushButton:pressed { background-color: #0A4F7A; }"
        "QPushButton:disabled { background-color: #3C3C3C; color: #6A6A6A; }"
        "QLineEdit, QComboBox { border: 1px solid #3C3C3C; border-radius: 5px; padding: 6px 10px; background-color: #3C3C3C; color: #CCCCCC; selection-background-color: #264F78; selection-color: #FFFFFF; }"
        "QLineEdit:focus, QComboBox:focus { border: 1px solid #0E639C; background-color: #2D2D30; }"
        "QComboBox::drop-down { border: none; background-color: #3C3C3C; }"
        "QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 4px solid #CCCCCC; }"
        "QComboBox QAbstractItemView { border: 1px solid #3C3C3C; border-radius: 4px; background-color: #252526; selection-background-color: #0E639C; selection-color: #FFFFFF; color: #CCCCCC; }"
        "QTableWidget { border: 1px solid #3C3C3C; border-radius: 5px; background-color: #252526; gridline-color: #2D2D30; alternate-background-color: #2D2D30; color: #CCCCCC; }"
        "QTableWidget::item { padding: 6px; color: #CCCCCC; }"
        "QTableWidget::item:selected { background-color: #264F78; color: #FFFFFF; }"
        "QHeaderView::section { background-color: #2D2D30; color: #CCCCCC; padding: 10px; border: none; border-bottom: 1px solid #3C3C3C; font-weight: 600; }"
        "QTabWidget::pane { border: 1px solid #3C3C3C; border-radius: 6px; background-color: #252526; top: -1px; }"
        "QTabBar::tab { background-color: #2D2D30; color: #858585; padding: 10px 24px; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; font-weight: 500; }"
        "QTabBar::tab:selected { background-color: #252526; color: #CCCCCC; border-bottom: 2px solid #0E639C; }"
        "QTabBar::tab:hover { background-color: #3C3C3C; color: #CCCCCC; }"
        "QTextBrowser { border: 1px solid #3C3C3C; border-radius: 5px; background-color: #1E1E1E; font-family: 'Courier New', monospace; color: #D4D4D4; }"
        "QStatusBar { background-color: #007ACC; border-top: 1px solid #3C3C3C; color: #FFFFFF; }"
        "QLabel { color: #CCCCCC; }"
        "QCheckBox { color: #CCCCCC; spacing: 6px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #3C3C3C; border-radius: 3px; background-color: #3C3C3C; }"
        "QCheckBox::indicator:checked { background-color: #0E639C; border-color: #0E639C; }"
        "QCheckBox::indicator:hover { border-color: #0E639C; }"
    );
    
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Панель подключения (всегда видна)
    QGroupBox *connectionGroup = new QGroupBox("🔌 Подключение", this);
    QHBoxLayout *connectionLayout = new QHBoxLayout(connectionGroup);
    connectionLayout->setSpacing(10);
    
    connectionLayout->addWidget(new QLabel("Порт:", this));
    m_portCombo = new QComboBox(this);
    m_portCombo->setMinimumWidth(200);
    m_portCombo->setEditable(false);
    connectionLayout->addWidget(m_portCombo);
    
    m_refreshPortsButton = new QPushButton("🔄", this);
    m_refreshPortsButton->setToolTip("Обновить список портов");
    m_refreshPortsButton->setMaximumWidth(40);
    connect(m_refreshPortsButton, &QPushButton::clicked, this, &MainWindow::onRefreshPortsClicked);
    connectionLayout->addWidget(m_refreshPortsButton);
    
    connectionLayout->addWidget(new QLabel("Скорость:", this));
    m_baudRateCombo = new QComboBox(this);
    m_baudRateCombo->addItem("125 кбит/с", 125);
    m_baudRateCombo->addItem("250 кбит/с", 250);
    m_baudRateCombo->addItem("500 кбит/с", 500);
    m_baudRateCombo->addItem("1000 кбит/с", 1000);
    m_baudRateCombo->setCurrentIndex(1);
    m_baudRateCombo->setMinimumWidth(120);
    connectionLayout->addWidget(m_baudRateCombo);
    
    m_connectButton = new QPushButton("Подключиться", this);
    m_connectButton->setMinimumWidth(150);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connectionLayout->addWidget(m_connectButton);
    
    connectionLayout->addStretch();
    
    // Главные вкладки
    QTabWidget *mainTabs = new QTabWidget(this);
    
    // Вкладка CAN
    QWidget *canTab = new QWidget(this);
    QVBoxLayout *canTabLayout = new QVBoxLayout(canTab);
    canTabLayout->setSpacing(10);
    canTabLayout->setContentsMargins(5, 5, 5, 5);
    
    // Отправка сообщений
    QGroupBox *sendGroup = new QGroupBox("📤 Отправка сообщения", this);
    QGridLayout *sendLayout = new QGridLayout(sendGroup);
    sendLayout->setSpacing(10);
    
    sendLayout->addWidget(new QLabel("CAN ID (hex):", this), 0, 0);
    m_canIdEdit = new QLineEdit(this);
    m_canIdEdit->setPlaceholderText("123");
    m_canIdEdit->setMaximumWidth(120);
    QRegularExpression hexRegExp("[0-9A-Fa-f]{1,8}");
    m_canIdEdit->setValidator(new QRegularExpressionValidator(hexRegExp, this));
    sendLayout->addWidget(m_canIdEdit, 0, 1);
    
    sendLayout->addWidget(new QLabel("Данные (hex):", this), 1, 0);
    m_canDataEdit = new QLineEdit(this);
    m_canDataEdit->setPlaceholderText("01 02 03 04 05 06 07 08");
    QRegularExpression dataRegExp("([0-9A-Fa-f]{2}\\s?)*");
    m_canDataEdit->setValidator(new QRegularExpressionValidator(dataRegExp, this));
    sendLayout->addWidget(m_canDataEdit, 1, 1);
    
    m_sendButton = new QPushButton("Отправить", this);
    m_sendButton->setEnabled(false);
    m_sendButton->setMinimumHeight(35);
    sendLayout->addWidget(m_sendButton, 0, 2, 2, 1);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    
    sendLayout->setColumnStretch(1, 1);
    
    // Фильтры
    QGroupBox *filterGroup = new QGroupBox("🔍 Фильтры", this);
    QHBoxLayout *filterLayout = new QHBoxLayout(filterGroup);
    filterLayout->setSpacing(10);
    
    m_filterEnabledCheck = new QCheckBox("Включить", this);
    m_filterIdEdit = new QLineEdit(this);
    m_filterIdEdit->setPlaceholderText("CAN ID (hex)");
    m_filterIdEdit->setMaximumWidth(120);
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
    
    // Таблица сообщений
    QGroupBox *logGroup = new QGroupBox("📋 Сообщения", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    logLayout->setSpacing(5);
    
    QHBoxLayout *logButtonsLayout = new QHBoxLayout();
    m_clearLogButton = new QPushButton("Очистить", this);
    m_saveLogButton = new QPushButton("Сохранить", this);
    connect(m_clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
    connect(m_saveLogButton, &QPushButton::clicked, this, &MainWindow::onSaveLogClicked);
    logButtonsLayout->addWidget(m_clearLogButton);
    logButtonsLayout->addWidget(m_saveLogButton);
    logButtonsLayout->addStretch();
    
    m_messageTable = new QTableWidget(this);
    m_messageTable->setColumnCount(4);
    m_messageTable->setHorizontalHeaderLabels(QStringList() << "Время" << "ID" << "Данные" << "Направление");
    m_messageTable->horizontalHeader()->setStretchLastSection(true);
    m_messageTable->setAlternatingRowColors(true);
    m_messageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_messageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_messageTable->setShowGrid(true);
    m_messageTable->verticalHeader()->setVisible(false);
    m_messageTable->setFont(QFont("Courier New", 9));
    
    // Настройка ширины колонок
    m_messageTable->setColumnWidth(0, 150); // Время
    m_messageTable->setColumnWidth(1, 100);  // ID
    m_messageTable->setColumnWidth(2, 300);  // Данные
    
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Courier New", 9));
    m_logTextEdit->setMaximumHeight(120);
    m_logTextEdit->show(); // Показываем лог по умолчанию
    
    logLayout->addLayout(logButtonsLayout);
    logLayout->addWidget(m_messageTable, 1);
    logLayout->addWidget(m_logTextEdit);
    
    canTabLayout->addWidget(sendGroup);
    canTabLayout->addWidget(filterGroup);
    canTabLayout->addWidget(logGroup, 1);
    
    mainTabs->addTab(canTab, "CAN");
    
    // Диагностика
    setupDiagnosticUI();
    mainTabs->addTab(m_diagnosticTabs, "Диагностика");
    
    mainLayout->addWidget(connectionGroup);
    mainLayout->addWidget(mainTabs, 1);
    
    // Статус бар
    m_statusLabel = new QLabel("● Не подключено", this);
    m_statusLabel->setStyleSheet("color: #F48771; font-weight: 500;");
    m_statsLabel = new QLabel("", this);
    m_statsLabel->setStyleSheet("color: #FFFFFF;");
    statusBar()->addWidget(m_statusLabel);
    statusBar()->addPermanentWidget(m_statsLabel);
    
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
            QMessageBox::information(this, "Успех", "Подключение к адаптеру установлено успешно!");
        } else {
            logMessage("Ошибка подключения", "ERROR");
            // Ошибка уже показана через onErrorOccurred, но можно добавить дополнительную информацию
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
    if (!m_canInterface) {
        QMessageBox::critical(this, "Ошибка", "CAN интерфейс не инициализирован!");
        return;
    }
    
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь к адаптеру!");
        return;
    }
    
    QString canIdStr = m_canIdEdit->text().trimmed();
    QString canDataStr = m_canDataEdit->text().trimmed();
    
    if (canIdStr.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите CAN ID!");
        m_canIdEdit->setFocus();
        return;
    }
    
    bool ok;
    quint32 canId = canIdStr.toUInt(&ok, 16);
    if (!ok || canId > 0x1FFFFFFF) {
        QMessageBox::warning(this, "Ошибка", QString("Неверный формат CAN ID!\nДопустимый диапазон: 0x000 - 0x1FFFFFFF"));
        m_canIdEdit->setFocus();
        m_canIdEdit->selectAll();
        return;
    }
    
    // Парсинг данных
    QByteArray data;
    if (!canDataStr.isEmpty()) {
        QStringList bytes = canDataStr.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
        for (const QString &byte : bytes) {
            bool byteOk;
            quint8 value = byte.toUInt(&byteOk, 16);
            if (byteOk && value <= 0xFF) {
                data.append(value);
            } else {
                QMessageBox::warning(this, "Ошибка", QString("Неверный формат данных: %1\nИспользуйте hex значения (00-FF)").arg(byte));
                m_canDataEdit->setFocus();
                return;
            }
        }
    }
    
    if (data.size() > 8) {
        QMessageBox::warning(this, "Ошибка", QString("CAN сообщение не может содержать более 8 байт!\nПолучено: %1 байт").arg(data.size()));
        m_canDataEdit->setFocus();
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
    // Форматируем сообщение для лога
    QString dataStr;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) dataStr += " ";
        dataStr += QString("%1").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    QString logMsg = QString("Принято: ID=0x%1, Данные=%2")
                     .arg(id, 0, 16).arg(dataStr);
    logMessage(logMsg, "RECV");
    
    // Добавляем в таблицу
    addMessageToTable(id, data, timestamp, true);
}

void MainWindow::onConnectionStatusChanged(bool connected)
{
    m_isConnected = connected;
    if (connected) {
        m_statusLabel->setText("● Подключено");
        m_statusLabel->setStyleSheet("color: #89D185; font-weight: 500;");
    } else {
        m_statusLabel->setText("● Не подключено");
        m_statusLabel->setStyleSheet("color: #F48771; font-weight: 500;");
    }
}

void MainWindow::onErrorOccurred(const QString &error)
{
    logMessage(QString("ОШИБКА: %1").arg(error), "ERROR");
    // Показываем критичные ошибки в диалоговом окне
    QMessageBox::critical(this, "Ошибка", error);
}

void MainWindow::logMessage(const QString &message, const QString &type)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString color;
    
    // Цвета для темной темы
    if (type == "ERROR") {
        color = "#F48771"; // Мягкий красный
    } else if (type == "SUCCESS") {
        color = "#89D185"; // Мягкий зеленый
    } else if (type == "SEND") {
        color = "#4EC9B0"; // Бирюзовый
    } else if (type == "RECV") {
        color = "#CE9178"; // Оранжево-коричневый
    } else {
        color = "#CCCCCC"; // Светло-серый для обычных сообщений
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
    
    // Цветовая подсветка для темной темы
    if (isReceived) {
        m_messageTable->item(row, 3)->setForeground(QBrush(QColor("#CE9178"))); // Оранжево-коричневый для RX
    } else {
        m_messageTable->item(row, 3)->setForeground(QBrush(QColor("#4EC9B0"))); // Бирюзовый для TX
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
    QWidget *udsTab = new QWidget(this);
    QVBoxLayout *udsTabLayout = new QVBoxLayout(udsTab);
    udsTabLayout->setSpacing(10);
    udsTabLayout->setContentsMargins(5, 5, 5, 5);
    
    m_udsGroup = new QGroupBox("📡 UDS (ISO 14229)", this);
    QGridLayout *udsLayout = new QGridLayout(m_udsGroup);
    udsLayout->setSpacing(10);
    
    // Чтение DID
    udsLayout->addWidget(new QLabel("DID (hex):", this), 0, 0);
    m_udsDIDEdit = new QLineEdit(this);
    m_udsDIDEdit->setPlaceholderText("F190");
    m_udsDIDEdit->setMaximumWidth(120);
    udsLayout->addWidget(m_udsDIDEdit, 0, 1);
    QPushButton *readDIDBtn = new QPushButton("Читать", this);
    connect(readDIDBtn, &QPushButton::clicked, this, &MainWindow::onUDSReadDID);
    udsLayout->addWidget(readDIDBtn, 0, 2);
    
    // Запись DID
    udsLayout->addWidget(new QLabel("Данные (hex):", this), 1, 0);
    m_udsDataEdit = new QLineEdit(this);
    m_udsDataEdit->setPlaceholderText("01 02 03");
    udsLayout->addWidget(m_udsDataEdit, 1, 1);
    QPushButton *writeDIDBtn = new QPushButton("Записать", this);
    connect(writeDIDBtn, &QPushButton::clicked, this, &MainWindow::onUDSWriteDID);
    udsLayout->addWidget(writeDIDBtn, 1, 2);
    
    // Чтение памяти
    udsLayout->addWidget(new QLabel("Адрес:", this), 2, 0);
    m_udsAddressEdit = new QLineEdit(this);
    m_udsAddressEdit->setPlaceholderText("0x12345678");
    m_udsAddressEdit->setMaximumWidth(150);
    udsLayout->addWidget(m_udsAddressEdit, 2, 1);
    udsLayout->addWidget(new QLabel("Длина:", this), 2, 2);
    m_udsLengthEdit = new QLineEdit(this);
    m_udsLengthEdit->setPlaceholderText("16");
    m_udsLengthEdit->setMaximumWidth(80);
    udsLayout->addWidget(m_udsLengthEdit, 2, 3);
    QPushButton *readMemBtn = new QPushButton("Читать память", this);
    connect(readMemBtn, &QPushButton::clicked, this, &MainWindow::onUDSReadMemory);
    udsLayout->addWidget(readMemBtn, 2, 4);
    
    // Безопасный доступ
    udsLayout->addWidget(new QLabel("Уровень:", this), 3, 0);
    m_udsSecurityLevelEdit = new QLineEdit(this);
    m_udsSecurityLevelEdit->setPlaceholderText("1");
    m_udsSecurityLevelEdit->setMaximumWidth(80);
    udsLayout->addWidget(m_udsSecurityLevelEdit, 3, 1);
    QPushButton *securityBtn = new QPushButton("Безопасный доступ", this);
    connect(securityBtn, &QPushButton::clicked, this, &MainWindow::onUDSSecurityAccess);
    udsLayout->addWidget(securityBtn, 3, 2, 1, 2);
    
    // Сессия и DTC
    QHBoxLayout *sessionDtcLayout = new QHBoxLayout();
    sessionDtcLayout->addWidget(new QLabel("Сессия:", this));
    m_udsSessionEdit = new QLineEdit(this);
    m_udsSessionEdit->setPlaceholderText("1=Default, 2=Programming, 3=Extended");
    m_udsSessionEdit->setMaximumWidth(200);
    sessionDtcLayout->addWidget(m_udsSessionEdit);
    QPushButton *sessionBtn = new QPushButton("Начать сессию", this);
    connect(sessionBtn, &QPushButton::clicked, this, &MainWindow::onUDSStartSession);
    sessionDtcLayout->addWidget(sessionBtn);
    sessionDtcLayout->addStretch();
    QPushButton *readDTCBtn = new QPushButton("Читать DTC", this);
    QPushButton *clearDTCBtn = new QPushButton("Очистить DTC", this);
    connect(readDTCBtn, &QPushButton::clicked, this, &MainWindow::onUDSReadDTC);
    connect(clearDTCBtn, &QPushButton::clicked, this, &MainWindow::onUDSClearDTC);
    sessionDtcLayout->addWidget(readDTCBtn);
    sessionDtcLayout->addWidget(clearDTCBtn);
    udsLayout->addLayout(sessionDtcLayout, 4, 0, 1, 5);
    
    udsLayout->setColumnStretch(1, 1);
    
    // Вывод диагностики
    m_diagnosticOutput = new QTextBrowser(this);
    m_diagnosticOutput->setFont(QFont("Courier New", 9));
    m_diagnosticOutput->setMinimumHeight(200);
    
    udsTabLayout->addWidget(m_udsGroup);
    udsTabLayout->addWidget(m_diagnosticOutput, 1);
    
    // OBD-II вкладка
    QWidget *obd2Tab = new QWidget(this);
    QVBoxLayout *obd2TabLayout = new QVBoxLayout(obd2Tab);
    obd2TabLayout->setSpacing(10);
    obd2TabLayout->setContentsMargins(5, 5, 5, 5);
    
    m_obd2Group = new QGroupBox("🚗 OBD-II (SAE J1979)", this);
    QGridLayout *obd2Layout = new QGridLayout(m_obd2Group);
    obd2Layout->setSpacing(10);
    
    // Режим и PID
    obd2Layout->addWidget(new QLabel("Режим:", this), 0, 0);
    m_obd2ModeCombo = new QComboBox(this);
    m_obd2ModeCombo->addItem("01 - Текущие данные", 0x01);
    m_obd2ModeCombo->addItem("03 - Сохраненные DTC", 0x03);
    m_obd2ModeCombo->addItem("04 - Очистить DTC", 0x04);
    m_obd2ModeCombo->addItem("07 - Ожидающие DTC", 0x07);
    m_obd2ModeCombo->addItem("09 - Информация", 0x09);
    obd2Layout->addWidget(m_obd2ModeCombo, 0, 1);
    obd2Layout->addWidget(new QLabel("PID (hex):", this), 0, 2);
    m_obd2PIDEdit = new QLineEdit(this);
    m_obd2PIDEdit->setPlaceholderText("0C (RPM), 0D (Speed)");
    m_obd2PIDEdit->setMaximumWidth(150);
    obd2Layout->addWidget(m_obd2PIDEdit, 0, 3);
    QPushButton *readPIDBtn = new QPushButton("Читать PID", this);
    connect(readPIDBtn, &QPushButton::clicked, this, &MainWindow::onOBD2ReadPID);
    obd2Layout->addWidget(readPIDBtn, 0, 4);
    
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
    obd2Layout->addLayout(quickLayout, 1, 0, 1, 5);
    
    obd2Layout->setColumnStretch(1, 1);
    
    obd2TabLayout->addWidget(m_obd2Group);
    obd2TabLayout->addWidget(m_diagnosticOutput, 1);
    
    m_diagnosticTabs->addTab(udsTab, "UDS");
    m_diagnosticTabs->addTab(obd2Tab, "OBD-II");
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
    if (!m_isConnected) {
        QMessageBox::warning(this, "Ошибка", "Сначала подключитесь!");
        return;
    }
    
    quint8 mode = m_obd2ModeCombo->currentData().toUInt();
    QString pidText = m_obd2PIDEdit->text();
    
    if (pidText.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите PID для чтения!");
        return;
    }
    
    // Парсим список PID (через пробел или запятую)
    QStringList pidStrings = pidText.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
    QList<quint8> pids;
    
    for (const QString &pidStr : pidStrings) {
        bool ok;
        quint8 pid = pidStr.toUInt(&ok, 16);
        if (ok) {
            pids.append(pid);
        } else {
            m_diagnosticOutput->append(QString("OBD-II: Пропущен неверный PID: %1").arg(pidStr));
        }
    }
    
    if (pids.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Не найдено ни одного валидного PID!");
        return;
    }
    
    m_diagnosticOutput->append(QString("OBD-II: Чтение %1 PID...").arg(pids.size()));
    
    QMap<quint8, OBD2Value> values;
    if (m_obd2Protocol->readMultiplePIDs(mode, pids, values)) {
        m_diagnosticOutput->append("OBD-II: Результаты:");
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            const OBD2Value &value = it.value();
            m_diagnosticOutput->append(QString("  PID 0x%1: %2 = %3")
                                      .arg(it.key(), 2, 16, QChar('0'))
                                      .arg(value.name)
                                      .arg(value.value));
        }
    } else {
        m_diagnosticOutput->append("OBD-II: Ошибка чтения PID");
    }
}

