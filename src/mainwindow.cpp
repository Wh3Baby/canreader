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

