#include "caninterface.h"
#include <QDebug>

CANInterface::CANInterface(QObject *parent)
    : QObject(parent)
    , m_connected(false)
    , m_currentBaudRate(0)
{
    m_serialPort = new QSerialPort(this);
    QObject::connect(m_serialPort, &QSerialPort::readyRead, this, &CANInterface::onDataReceived);
    QObject::connect(m_serialPort, &QSerialPort::errorOccurred, this, &CANInterface::onSerialError);
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
    // Для CAN 250 кбит/с используем 115200 бод
    // Для CAN 500 кбит/с используем 230400 бод
    int serialBaudRate = (baudRateKbps == 500) ? 230400 : 115200;
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
        initCmd.append(static_cast<quint8>(baudRateKbps == 500 ? 0x02 : 0x01)); // 0x01 = 250, 0x02 = 500
        initCmd.append(static_cast<quint8>(0x00)); // Резерв
        initCmd.append(FRAME_END);
        
        m_serialPort->write(initCmd);
        m_serialPort->waitForBytesWritten(1000);
        
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
        m_serialPort->waitForBytesWritten(100);
        return true;
    } else {
        emit errorOccurred("Ошибка записи в порт");
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

void CANInterface::onDataReceived()
{
    QByteArray data = m_serialPort->readAll();
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
            
            QString message = formatCanMessage(canId, canData);
            emit messageReceived(message);
            
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

