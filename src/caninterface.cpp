#include "caninterface.h"
#include <QDebug>

CANInterface::CANInterface(QObject *parent)
    : QObject(parent)
    , m_connected(false)
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
    if (m_connected) {
        disconnect();
    }
    
    // Извлекаем реальное имя порта из строки с описанием
    // Формат: "ttyUSB0 (FTDI Serial Converter)" -> "ttyUSB0"
    // Или: "COM3 - USB Serial Port" -> "COM3"
    QString portName = portDisplayName;
    int spaceIndex = portName.indexOf(' ');
    if (spaceIndex > 0) {
        portName = portName.left(spaceIndex);
    }
    
    m_serialPort->setPortName(portName);
    
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
    
    if (m_serialPort->open(QIODevice::ReadWrite)) {
        m_connected = true;
        m_buffer.clear();
        
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
        
        m_serialPort->write(initCmd);
        if (!m_serialPort->waitForBytesWritten(m_writeTimeout)) {
            emit errorOccurred("Таймаут при инициализации адаптера");
            m_serialPort->close();
            return false;
        }
        
        resetStatistics();
        emit connectionStatusChanged(true);
        return true;
    } else {
        emit errorOccurred(QString("Не удалось открыть порт: %1").arg(m_serialPort->errorString()));
        return false;
    }
}

void CANInterface::disconnect()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    m_connected = false;
    m_buffer.clear();
    emit connectionStatusChanged(false);
}

bool CANInterface::isConnected() const
{
    return m_connected && m_serialPort->isOpen();
}

bool CANInterface::sendMessage(quint32 canId, const QByteArray &data)
{
    if (!isConnected()) {
        emit errorOccurred("Адаптер не подключен");
        return false;
    }
    
    if (data.size() > 8) {
        emit errorOccurred("CAN сообщение не может содержать более 8 байт");
        return false;
    }
    
    QByteArray frame = buildCanFrame(canId, data);
    qint64 bytesWritten = m_serialPort->write(frame);
    
    if (bytesWritten == frame.size()) {
        if (m_serialPort->waitForBytesWritten(m_writeTimeout)) {
            // Обновление статистики
            m_stats.messagesSent++;
            m_stats.messagesPerId[canId]++;
            emit statisticsUpdated();
            return true;
        } else {
            emit errorOccurred("Таймаут при записи в порт");
            return false;
        }
    } else {
        emit errorOccurred(QString("Ошибка записи в порт: записано %1 из %2 байт").arg(bytesWritten).arg(frame.size()));
        m_stats.errorsCount++;
        return false;
    }
}

QStringList CANInterface::getAvailablePorts() const
{
    QStringList ports;
    const auto portInfos = QSerialPortInfo::availablePorts();
    
    for (const QSerialPortInfo &portInfo : portInfos) {
        QString portName = portInfo.portName();
        QString description = portInfo.description();
        QString manufacturer = portInfo.manufacturer();
        
        // Формируем информативную строку для отображения
        QString displayName = portName;
        
#ifdef Q_OS_WIN
        // На Windows: COM1, COM2 и т.д.
        if (!description.isEmpty()) {
            displayName += QString(" - %1").arg(description);
        }
#else
        // На Linux: /dev/ttyUSB0, /dev/ttyACM0 и т.д.
        // QSerialPortInfo::portName() уже возвращает имя устройства
        if (!description.isEmpty() || !manufacturer.isEmpty()) {
            QString info = description;
            if (!manufacturer.isEmpty()) {
                if (!info.isEmpty()) info += " ";
                info += manufacturer;
            }
            displayName += QString(" (%1)").arg(info);
        }
#endif
        
        ports.append(displayName);
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
    QByteArray data = m_serialPort->readAll();
    
    // Защита от переполнения буфера
    if (m_buffer.size() + data.size() > m_maxBufferSize) {
        emit errorOccurred(QString("Переполнение буфера! Очистка. Размер: %1 байт").arg(m_buffer.size()));
        m_buffer.clear();
        m_stats.errorsCount++;
    }
    
    m_buffer.append(data);
    parseReceivedData(m_buffer);
}

void CANInterface::parseReceivedData(QByteArray &buffer)
{
    // Парсинг протокола Scanmatic 2 Pro
    // Формат кадра: 0xAA (старт) + тип (0x02 для CAN данных) + длина + CAN ID (4 байта) + данные (до 8 байт) + 0x55 (конец)
    
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

