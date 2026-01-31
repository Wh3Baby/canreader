#include "caninterface.h"
#include "usbdevice.h"
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include <QCoreApplication>

CANInterface::CANInterface(QObject *parent)
    : QObject(parent)
    , m_connected(false)
    , m_useUSB(false)
    , m_currentBaudRate(0)
    , m_readTimeout(5000)
    , m_writeTimeout(1000)
    , m_maxBufferSize(MAX_BUFFER_SIZE)
    , m_filterEnabled(false)
    , m_lastSecondMessages(0)
{
    m_serialPort = new QSerialPort(this);
    QObject::connect(m_serialPort, &QSerialPort::readyRead, this, &CANInterface::onDataReceived);
    QObject::connect(m_serialPort, &QSerialPort::errorOccurred, this, &CANInterface::onSerialError);
    
    m_usbDevice = new USBDevice(this);
    QObject::connect(m_usbDevice, &USBDevice::dataReceived, this, &CANInterface::onUSBDataReceived);
    QObject::connect(m_usbDevice, &USBDevice::errorOccurred, this, &CANInterface::errorOccurred);
    
    // Таймер для обновления статистики
    m_statsTimer = new QTimer(this);
    QObject::connect(m_statsTimer, &QTimer::timeout, this, &CANInterface::updateStatistics);
    m_statsTimer->start(1000); // Обновление каждую секунду
    
    // Инициализация статистики
    resetStatistics();
}

CANInterface::~CANInterface()
{
    disconnect();
}

bool CANInterface::connect(const QString &portDisplayName, int baudRateKbps)
{
    if (!m_serialPort) {
        emit errorOccurred("Ошибка: серийный порт не инициализирован");
        return false;
    }
    
    if (portDisplayName.isEmpty()) {
        emit errorOccurred("Ошибка: имя порта не указано");
        return false;
    }
    
    if (m_connected) {
        disconnect();
    }
    
    // Извлекаем реальное имя порта из строки с описанием
    // Формат: "ttyUSB0 (FTDI Serial Converter)" -> "ttyUSB0"
    // Или: "COM3 - USB Serial Port" -> "COM3"
    QString portName = portDisplayName.trimmed();
    int spaceIndex = portName.indexOf(' ');
    if (spaceIndex > 0) {
        portName = portName.left(spaceIndex);
    }
    
    if (portName.isEmpty()) {
        emit errorOccurred("Ошибка: не удалось извлечь имя порта");
        return false;
    }
    
    // Проверка доступности порта
    QSerialPortInfo portInfo(portName);
    if (portInfo.isNull()) {
        // Попробуем найти порт по VID/PID если порт не найден по имени
        const quint16 targetVendorId = 0x20A2;
        const quint16 targetProductId = 0x0001;
        
        bool foundByVidPid = false;
        const auto allPorts = QSerialPortInfo::availablePorts();
        for (const QSerialPortInfo &info : allPorts) {
            if (info.vendorIdentifier() == targetVendorId && 
                info.productIdentifier() == targetProductId) {
                portInfo = info;
                foundByVidPid = true;
                qDebug() << "Найден адаптер по VID/PID:" << info.portName();
                break;
            }
        }
        
        if (!foundByVidPid) {
            QString errorMsg = QString("Порт %1 не найден в системе.\n\n").arg(portName);
            errorMsg += "Проверьте:\n";
            errorMsg += "1. Подключено ли устройство USB (VID:20A2 PID:0001)\n";
            errorMsg += "2. Установлены ли драйверы для устройства\n";
            errorMsg += "3. Создается ли виртуальный COM порт при подключении\n";
            errorMsg += "4. Если устройство не создает COM порт, может потребоваться специальный драйвер или протокол";
            emit errorOccurred(errorMsg);
            return false;
        }
    }
    
    m_serialPort->setPort(portInfo);
    
    // Дополнительная диагностика
    qDebug() << "Подключение к порту:" << portInfo.portName();
    qDebug() << "VID:" << QString::number(portInfo.vendorIdentifier(), 16);
    qDebug() << "PID:" << QString::number(portInfo.productIdentifier(), 16);
    qDebug() << "Описание:" << portInfo.description();
    qDebug() << "Производитель:" << portInfo.manufacturer();
    
    // Scanmatic 2 Pro использует стандартные скорости последовательного порта
    // Маппинг скоростей CAN на скорости последовательного порта
    int serialBaudRate = 115200; // По умолчанию
    switch (baudRateKbps) {
        case 125:  serialBaudRate = 57600;  break;
        case 250:  serialBaudRate = 115200; break;
        case 500:  serialBaudRate = 230400; break;
        case 1000: serialBaudRate = 460800; break;
        default:   serialBaudRate = 115200; break;
    }
    m_serialPort->setBaudRate(serialBaudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    
    m_currentBaudRate = baudRateKbps;
    
    qDebug() << "Попытка открыть порт:" << portName << "со скоростью" << serialBaudRate;
    
    if (m_serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "Порт успешно открыт";
        // Очистка буферов порта перед использованием
        m_serialPort->clear();
        m_buffer.clear();
        
        // Задержка для инициализации устройства после открытия порта
        // Используем QCoreApplication::processEvents чтобы не блокировать UI
        QElapsedTimer delayTimer;
        delayTimer.start();
        while (delayTimer.elapsed() < 100) {
            QCoreApplication::processEvents();
        }
        
        // Отправка команды инициализации для Scanmatic 2 Pro
        // Формат: 0xAA (старт) + 0x01 (команда инициализации) + CAN скорость + 0x55 (конец)
        QByteArray initCmd;
        initCmd.append(FRAME_START);
        initCmd.append(static_cast<quint8>(0x01)); // Команда инициализации
        // Маппинг скорости CAN на код команды
        quint8 speedCode = 0x01; // По умолчанию 250
        switch (baudRateKbps) {
            case 125:  speedCode = 0x00; break;
            case 250:  speedCode = 0x01; break;
            case 500:  speedCode = 0x02; break;
            case 1000: speedCode = 0x03; break;
            default:   speedCode = 0x01; break;
        }
        initCmd.append(speedCode);
        initCmd.append(static_cast<quint8>(0x00)); // Резерв
        initCmd.append(FRAME_END);
        
        // Очистка выходного буфера перед отправкой
        m_serialPort->clear(QSerialPort::Output);
        
        qint64 bytesWritten = m_serialPort->write(initCmd);
        if (bytesWritten != initCmd.size()) {
            emit errorOccurred(QString("Ошибка записи команды инициализации: записано %1 из %2 байт")
                              .arg(bytesWritten).arg(initCmd.size()));
            m_serialPort->close();
            return false;
        }
        
        // Увеличенный таймаут для инициализации (5 секунд)
        if (!m_serialPort->waitForBytesWritten(5000)) {
            emit errorOccurred("Таймаут при инициализации адаптера. Проверьте подключение устройства.");
            m_serialPort->close();
            return false;
        }
        
        // Дополнительная задержка после отправки команды инициализации
        // Используем QCoreApplication::processEvents чтобы не блокировать UI
        QElapsedTimer delayTimer2;
        delayTimer2.start();
        while (delayTimer2.elapsed() < 200) {
            QCoreApplication::processEvents();
        }
        
        // Очистка входного буфера после инициализации
        m_serialPort->clear(QSerialPort::Input);
        
        m_connected = true;
        resetStatistics();
        emit connectionStatusChanged(true);
        qDebug() << "Подключение к адаптеру установлено успешно";
        return true;
    } else {
        QString errorMsg = QString("Не удалось открыть порт %1: %2")
                          .arg(portName)
                          .arg(m_serialPort->errorString());
        
        // Дополнительная диагностика
        if (m_serialPort->error() == QSerialPort::PermissionError) {
            errorMsg += "\nВозможно, недостаточно прав доступа. Попробуйте запустить с правами администратора или добавить пользователя в группу dialout (Linux).";
        } else if (m_serialPort->error() == QSerialPort::DeviceNotFoundError) {
            errorMsg += QString("\nУстройство не найдено. Проверьте:\n"
                              "- Подключено ли устройство USB (VID:20A2 PID:0001)\n"
                              "- Установлены ли драйверы\n"
                              "- Определяется ли порт в системе (lsusb / dmesg на Linux)");
        } else if (m_serialPort->error() == QSerialPort::OpenError) {
            errorMsg += "\nПорт уже открыт другим приложением.";
        }
        
        emit errorOccurred(errorMsg);
        qDebug() << "Ошибка открытия порта:" << errorMsg;
        return false;
    }
}

bool CANInterface::connectUSB(quint16 vendorId, quint16 productId, int baudRateKbps)
{
    if (!m_usbDevice) {
        emit errorOccurred("USB устройство не инициализировано");
        return false;
    }
    
    if (m_connected) {
        disconnect();
    }
    
    m_currentBaudRate = baudRateKbps;
    
    qDebug() << "Попытка подключения к USB устройству VID:" << QString::number(vendorId, 16) 
             << "PID:" << QString::number(productId, 16);
    
    if (!m_usbDevice->open(vendorId, productId)) {
        emit errorOccurred(QString("Не удалось открыть USB устройство: %1").arg(m_usbDevice->errorString()));
        return false;
    }
    
    // Очистка буфера
    m_buffer.clear();
    
    // Задержка для инициализации устройства
    QElapsedTimer delayTimer;
    delayTimer.start();
    while (delayTimer.elapsed() < 100) {
        QCoreApplication::processEvents();
    }
    
    // Отправка команды инициализации для адаптера
    // Формат: 0xAA (старт) + 0x01 (команда инициализации) + CAN скорость + 0x55 (конец)
    QByteArray initCmd;
    initCmd.append(FRAME_START);
    initCmd.append(static_cast<quint8>(0x01)); // Команда инициализации
    // Маппинг скорости CAN на код команды
    quint8 speedCode = 0x01; // По умолчанию 250
    switch (baudRateKbps) {
        case 125:  speedCode = 0x00; break;
        case 250:  speedCode = 0x01; break;
        case 500:  speedCode = 0x02; break;
        case 1000: speedCode = 0x03; break;
        default:   speedCode = 0x01; break;
    }
    initCmd.append(speedCode);
    initCmd.append(static_cast<quint8>(0x00)); // Резерв
    initCmd.append(FRAME_END);
    
    if (!m_usbDevice->write(initCmd)) {
        emit errorOccurred(QString("Ошибка записи команды инициализации: %1").arg(m_usbDevice->errorString()));
        m_usbDevice->close();
        return false;
    }
    
    // Дополнительная задержка после отправки команды инициализации
    QElapsedTimer delayTimer2;
    delayTimer2.start();
    while (delayTimer2.elapsed() < 200) {
        QCoreApplication::processEvents();
    }
    
    m_connected = true;
    m_useUSB = true;
    resetStatistics();
    emit connectionStatusChanged(true);
    qDebug() << "Подключение к USB адаптеру установлено успешно";
    return true;
#else
    emit errorOccurred("USB подключение недоступно. libusb не найден при компиляции.");
    return false;
#endif
}

void CANInterface::disconnect()
{
    if (m_useUSB) {
        if (m_usbDevice) {
            m_usbDevice->close();
        }
    } else {
        if (m_serialPort && m_serialPort->isOpen()) {
            m_serialPort->close();
        }
    }
    m_connected = false;
    m_useUSB = false;
    m_buffer.clear();
    emit connectionStatusChanged(false);
}

bool CANInterface::isConnected() const
{
    if (m_useUSB) {
        return m_connected && m_usbDevice && m_usbDevice->isOpen();
    } else {
        return m_connected && m_serialPort->isOpen();
    }
}

bool CANInterface::sendMessage(quint32 canId, const QByteArray &data)
{
    if (!isConnected()) {
        emit errorOccurred("Адаптер не подключен");
        return false;
    }
    
    // Валидация CAN ID (стандартный 11 бит или расширенный 29 бит)
    if (canId > 0x1FFFFFFF) {
        emit errorOccurred(QString("Неверный CAN ID: 0x%1 (максимум 29 бит)").arg(canId, 0, 16));
        m_stats.errorsCount++;
        return false;
    }
    
    if (data.size() > 8) {
        emit errorOccurred(QString("CAN сообщение не может содержать более 8 байт (получено: %1)").arg(data.size()));
        m_stats.errorsCount++;
        return false;
    }
    
    QByteArray frame = buildCanFrame(canId, data);
    if (frame.isEmpty()) {
        emit errorOccurred("Ошибка построения CAN кадра");
        m_stats.errorsCount++;
        return false;
    }
    
    // Отправка через USB или Serial
    bool success = false;
    
    if (m_useUSB) {
        if (!m_usbDevice || !m_usbDevice->isOpen()) {
            emit errorOccurred("USB устройство не открыто");
            m_stats.errorsCount++;
            return false;
        }
        
        success = m_usbDevice->write(frame);
        if (!success) {
            emit errorOccurred(QString("Ошибка записи в USB: %1").arg(m_usbDevice->errorString()));
            m_stats.errorsCount++;
            return false;
        }
    } else {
        if (!m_serialPort) {
            emit errorOccurred("Ошибка: серийный порт не инициализирован");
            m_stats.errorsCount++;
            return false;
        }
        
        if (!m_serialPort->isOpen()) {
            emit errorOccurred("Порт не открыт");
            m_stats.errorsCount++;
            return false;
        }
        
        qint64 bytesWritten = m_serialPort->write(frame);
        
        if (bytesWritten == frame.size()) {
            // Увеличенный таймаут для записи (3 секунды вместо 1)
            int writeTimeout = qMax(m_writeTimeout, 3000);
            success = m_serialPort->waitForBytesWritten(writeTimeout);
            if (!success) {
                QString errorMsg = QString("Таймаут при записи в порт: %1").arg(m_serialPort->errorString());
                emit errorOccurred(errorMsg);
                m_stats.errorsCount++;
                
                // Проверка, не отключилось ли устройство
                if (m_serialPort->error() == QSerialPort::ResourceError) {
                    disconnect();
                }
                return false;
            }
        } else {
            emit errorOccurred(QString("Ошибка записи в порт: записано %1 из %2 байт. Ошибка: %3")
                              .arg(bytesWritten).arg(frame.size()).arg(m_serialPort->errorString()));
            m_stats.errorsCount++;
            return false;
        }
    }
    
    if (success) {
        // Обновление статистики
        m_stats.messagesSent++;
        m_stats.messagesPerId[canId]++;
        if (m_stats.firstMessageTime.isNull()) {
            m_stats.firstMessageTime = QDateTime::currentDateTime();
        }
        m_stats.lastMessageTime = QDateTime::currentDateTime();
        emit statisticsUpdated();
        return true;
    }
    
    return false;
}

QStringList CANInterface::getAvailablePorts() const
{
    QStringList ports;
    const auto portInfos = QSerialPortInfo::availablePorts();
    
    // VID и PID для адаптера
    const quint16 targetVendorId = 0x20A2;
    const quint16 targetProductId = 0x0001;
    
    qDebug() << "Поиск доступных портов... Найдено:" << portInfos.size();
    
    bool foundTargetDevice = false;
    
    // Добавляем опцию прямого USB подключения
    // Проверяем, доступно ли USB устройство напрямую
    ports.append("USB (прямое подключение VID:20A2 PID:0001)");
    foundTargetDevice = true; // Предполагаем что USB доступен
    
    for (const QSerialPortInfo &portInfo : portInfos) {
        QString portName = portInfo.portName();
        QString description = portInfo.description();
        QString manufacturer = portInfo.manufacturer();
        QString serialNumber = portInfo.serialNumber();
        quint16 vendorId = portInfo.vendorIdentifier();
        quint16 productId = portInfo.productIdentifier();
        
        qDebug() << "Порт:" << portName 
                 << "VID:" << QString::number(vendorId, 16) 
                 << "PID:" << QString::number(productId, 16)
                 << "Описание:" << description;
        
        // Проверка на соответствие VID/PID
        bool matchesTarget = false;
        if (vendorId == targetVendorId && productId == targetProductId) {
            matchesTarget = true;
            foundTargetDevice = true;
            qDebug() << "Найден целевой адаптер на порту:" << portName;
        }
        
        // Формируем информативную строку для отображения
        QString displayName = portName;
        
#ifdef Q_OS_WIN
        // На Windows: COM1, COM2 и т.д.
        if (!description.isEmpty()) {
            displayName += QString(" - %1").arg(description);
        }
        if (matchesTarget) {
            displayName += " [USB-CAN Адаптер VID:20A2 PID:0001]";
        }
        // Показываем VID:PID если доступны
        if (vendorId != 0 || productId != 0) {
            displayName += QString(" [VID:%1 PID:%2]")
                          .arg(vendorId, 4, 16, QChar('0')).arg(productId, 4, 16, QChar('0'));
        }
#else
        // На Linux: /dev/ttyUSB0, /dev/ttyACM0 и т.д.
        if (!description.isEmpty() || !manufacturer.isEmpty()) {
            QString info = description;
            if (!manufacturer.isEmpty()) {
                if (!info.isEmpty()) info += " ";
                info += manufacturer;
            }
            displayName += QString(" (%1)").arg(info);
        }
        if (matchesTarget) {
            displayName += " [USB-CAN Адаптер VID:20A2 PID:0001]";
        }
        // На Linux также показываем VID:PID если доступны
        if (vendorId != 0 || productId != 0) {
            displayName += QString(" [VID:%1 PID:%2]")
                          .arg(vendorId, 4, 16, QChar('0')).arg(productId, 4, 16, QChar('0'));
        }
#endif
        
        ports.append(displayName);
    }
    
    if (!foundTargetDevice) {
        qDebug() << "ВНИМАНИЕ: Адаптер с VID:20A2 PID:0001 не найден среди доступных COM портов!";
        qDebug() << "Используйте прямое USB подключение если доступно.";
    }
    
    return ports;
}

void CANInterface::refreshPortList()
{
    // Метод для обновления списка портов (можно вызывать периодически)
}

void CANInterface::setFilterEnabled(bool enabled)
{
    m_filterEnabled = enabled;
}

void CANInterface::addFilterId(quint32 id, bool allow)
{
    m_filterIds[id] = allow;
}

void CANInterface::clearFilters()
{
    m_filterIds.clear();
}

bool CANInterface::isMessageFiltered(quint32 id) const
{
    if (!m_filterEnabled) {
        return false; // Фильтр отключен, пропускаем все
    }
    
    if (m_filterIds.contains(id)) {
        return !m_filterIds[id]; // Если в списке и false - фильтруем
    }
    
    // Если ID нет в списке, пропускаем (можно изменить логику)
    return false;
}

Statistics CANInterface::getStatistics() const
{
    return m_stats;
}

void CANInterface::resetStatistics()
{
    m_stats.messagesSent = 0;
    m_stats.messagesReceived = 0;
    m_stats.errorsCount = 0;
    m_stats.firstMessageTime = QDateTime();
    m_stats.lastMessageTime = QDateTime();
    m_stats.messagesPerId.clear();
    m_lastSecondMessages = 0;
    m_lastSecondTime = QDateTime::currentDateTime();
    emit statisticsUpdated();
}

quint64 CANInterface::getMessagesPerSecond() const
{
    return m_lastSecondMessages;
}

void CANInterface::setReadTimeout(int milliseconds)
{
    m_readTimeout = milliseconds;
}

void CANInterface::setWriteTimeout(int milliseconds)
{
    m_writeTimeout = milliseconds;
}

void CANInterface::setMaxBufferSize(int size)
{
    m_maxBufferSize = qMax(1024, size); // Минимум 1KB
}

void CANInterface::updateStatistics()
{
    QDateTime now = QDateTime::currentDateTime();
    if (m_lastSecondTime.isValid()) {
        qint64 elapsed = m_lastSecondTime.msecsTo(now);
        if (elapsed >= 1000) {
            m_lastSecondMessages = 0;
            m_lastSecondTime = now;
        }
    } else {
        m_lastSecondTime = now;
    }
}

bool CANInterface::validateFrame(const QByteArray &frame) const
{
    if (frame.size() < 8) return false; // Минимальный размер кадра
    if (static_cast<quint8>(frame[0]) != FRAME_START) return false;
    if (static_cast<quint8>(frame[frame.size() - 1]) != FRAME_END) return false;
    return true;
}

void CANInterface::onDataReceived()
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        return;
    }
    
    QByteArray data = m_serialPort->readAll();
    if (data.isEmpty()) {
        return;
    }
    
    // Защита от переполнения буфера
    if (m_buffer.size() + data.size() > m_maxBufferSize) {
        emit errorOccurred(QString("Переполнение буфера! Очистка. Размер: %1 байт").arg(m_buffer.size()));
        m_buffer.clear();
        m_stats.errorsCount++;
        emit statisticsUpdated();
    }
    
    m_buffer.append(data);
    parseReceivedData(m_buffer);
}

void CANInterface::parseReceivedData(QByteArray &buffer)
{
    // Парсинг протокола Scanmatic 2 Pro
    // Формат кадра: 0xAA (старт) + тип (0x02 для CAN данных) + длина + CAN ID (4 байта) + данные (до 8 байт) + 0x55 (конец)
    
    if (buffer.isEmpty()) {
        return;
    }
    
    while (buffer.size() >= 3) {
        int startIndex = buffer.indexOf(FRAME_START);
        if (startIndex == -1) {
            buffer.clear();
            break;
        }
        
        if (startIndex > 0) {
            buffer.remove(0, startIndex);
        }
        
        if (buffer.size() < 3) {
            break;
        }
        
        quint8 frameType = static_cast<quint8>(buffer[1]);
        quint8 dataLength = static_cast<quint8>(buffer[2]);
        
        // Минимальный размер кадра: старт (1) + тип (1) + длина (1) + CAN ID (4) + данные (0-8) + конец (1)
        int minFrameSize = 3 + 4 + 1; // минимум с CAN ID и концом
        int maxFrameSize = 3 + 4 + 8 + 1; // максимум с 8 байтами данных
        
        if (frameType == 0x02 && dataLength <= 8) { // CAN данные
            int expectedFrameSize = 3 + 4 + dataLength + 1; // старт + тип + длина + CAN ID + данные + конец
            
            if (buffer.size() < expectedFrameSize) {
                // Недостаточно данных, ждем еще
                break;
            }
            
            int endIndex = -1;
            for (int i = 3 + 4 + dataLength; i < buffer.size(); ++i) {
                if (static_cast<quint8>(buffer[i]) == FRAME_END) {
                    endIndex = i;
                    break;
                }
            }
            
            if (endIndex == -1) {
                // Конец кадра не найден, возможно данные еще не пришли
                if (buffer.size() > maxFrameSize) {
                    // Слишком много данных без конца кадра, возможно ошибка
                    buffer.remove(0, 1); // Удаляем первый байт и пробуем снова
                }
                break;
            }
            
            // Извлекаем CAN ID (4 байта после длины)
            quint32 canId = 0;
            canId |= static_cast<quint32>(static_cast<quint8>(buffer[3])) << 24;
            canId |= static_cast<quint32>(static_cast<quint8>(buffer[4])) << 16;
            canId |= static_cast<quint32>(static_cast<quint8>(buffer[5])) << 8;
            canId |= static_cast<quint32>(static_cast<quint8>(buffer[6]));
            
            // Извлекаем данные
            QByteArray canData = buffer.mid(7, dataLength);
            
            // Проверка фильтра
            if (isMessageFiltered(canId)) {
                // Сообщение отфильтровано, удаляем из буфера но не обрабатываем
                buffer.remove(0, endIndex + 1);
                continue;
            }
            
            // Валидация кадра
            QByteArray frame = buffer.mid(0, endIndex + 1);
            if (!validateFrame(frame)) {
                emit errorOccurred(QString("Получен невалидный кадр для ID 0x%1").arg(canId, 0, 16));
                m_stats.errorsCount++;
                buffer.remove(0, 1); // Пропускаем байт
                continue;
            }
            
            QDateTime timestamp = QDateTime::currentDateTime();
            
            // Обновление статистики
            m_stats.messagesReceived++;
            m_stats.messagesPerId[canId]++;
            if (!m_stats.firstMessageTime.isValid()) {
                m_stats.firstMessageTime = timestamp;
            }
            m_stats.lastMessageTime = timestamp;
            m_lastSecondMessages++;
            emit statisticsUpdated();
            
            QString message = formatCanMessage(canId, canData);
            emit messageReceived(message);
            emit messageReceivedDetailed(canId, canData, timestamp);
            
            // Удаляем обработанный кадр из буфера
            buffer.remove(0, endIndex + 1);
        } else {
            // Неизвестный тип кадра или ошибка, пропускаем байт
            buffer.remove(0, 1);
        }
    }
}

QByteArray CANInterface::buildCanFrame(quint32 canId, const QByteArray &data)
{
    QByteArray frame;
    
    frame.append(FRAME_START);                    // Старт кадра
    frame.append(0x02);                           // Тип: CAN данные
    frame.append(static_cast<quint8>(data.size())); // Длина данных
    
    // CAN ID (4 байта, big-endian)
    frame.append(static_cast<quint8>((canId >> 24) & 0xFF));
    frame.append(static_cast<quint8>((canId >> 16) & 0xFF));
    frame.append(static_cast<quint8>((canId >> 8) & 0xFF));
    frame.append(static_cast<quint8>(canId & 0xFF));
    
    // Данные
    frame.append(data);
    
    frame.append(FRAME_END);                     // Конец кадра
    
    return frame;
}

QString CANInterface::formatCanMessage(quint32 canId, const QByteArray &data)
{
    QString dataStr;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) dataStr += " ";
        dataStr += QString("%1").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    
    return QString("ID=0x%1, Данные=%2")
           .arg(canId, 0, 16).arg(dataStr);
}

void CANInterface::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        emit errorOccurred("Критическая ошибка порта. Возможно, устройство отключено.");
        disconnect();
    } else if (error != QSerialPort::NoError) {
        emit errorOccurred(QString("Ошибка порта: %1").arg(m_serialPort->errorString()));
    }
}

