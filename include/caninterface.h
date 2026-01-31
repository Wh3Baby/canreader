#ifndef CANINTERFACE_H
#define CANINTERFACE_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QByteArray>
#include <QMap>
#include <QDateTime>

class USBDevice;

struct CANMessage {
    quint32 id;
    QByteArray data;
    QDateTime timestamp;
    bool isReceived;
};

struct Statistics {
    quint64 messagesSent;
    quint64 messagesReceived;
    quint64 errorsCount;
    QDateTime firstMessageTime;
    QDateTime lastMessageTime;
    QMap<quint32, quint64> messagesPerId;
};

class CANInterface : public QObject
{
    Q_OBJECT

public:
    explicit CANInterface(QObject *parent = nullptr);
    ~CANInterface();
    
    bool connect(const QString &portName, int baudRateKbps);
    bool connectUSB(quint16 vendorId = 0x20A2, quint16 productId = 0x0001, int baudRateKbps = 250);
    void disconnect();
    bool isConnected() const;
    bool sendMessage(quint32 canId, const QByteArray &data);
    QStringList getAvailablePorts() const;
    void refreshPortList();
    
    // Фильтрация
    void setFilterEnabled(bool enabled);
    void addFilterId(quint32 id, bool allow);
    void clearFilters();
    bool isMessageFiltered(quint32 id) const;
    
    // Статистика
    Statistics getStatistics() const;
    void resetStatistics();
    quint64 getMessagesPerSecond() const;
    
    // Настройки
    void setReadTimeout(int milliseconds);
    void setWriteTimeout(int milliseconds);
    void setMaxBufferSize(int size);

signals:
    void messageReceived(const QString &message);
    void messageReceivedDetailed(quint32 id, const QByteArray &data, const QDateTime &timestamp);
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString &error);
    void statisticsUpdated();

private slots:
    void onDataReceived();
    void onSerialError(QSerialPort::SerialPortError error);
    void onUSBDataReceived(const QByteArray &data);
    void updateStatistics();

private:
    void parseReceivedData(QByteArray &data);
    QByteArray buildCanFrame(quint32 canId, const QByteArray &data);
    QString formatCanMessage(quint32 canId, const QByteArray &data);
    bool validateFrame(const QByteArray &frame) const;
    
    QSerialPort *m_serialPort;
    USBDevice *m_usbDevice;
    QByteArray m_buffer;
    bool m_connected;
    bool m_useUSB;
    int m_currentBaudRate;
    
    // Таймауты
    int m_readTimeout;
    int m_writeTimeout;
    int m_maxBufferSize;
    
    // Фильтрация
    bool m_filterEnabled;
    QMap<quint32, bool> m_filterIds; // true = разрешить, false = запретить
    
    // Статистика
    Statistics m_stats;
    QTimer *m_statsTimer;
    quint64 m_lastSecondMessages;
    QDateTime m_lastSecondTime;
    
    // Протокол Scanmatic 2 Pro
    static constexpr quint8 FRAME_START = 0xAA;
    static constexpr quint8 FRAME_END = 0x55;
    static constexpr int MAX_BUFFER_SIZE = 4096; // Защита от переполнения
};

#endif // CANINTERFACE_H

