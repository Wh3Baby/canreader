#ifndef CANINTERFACE_H
#define CANINTERFACE_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QByteArray>

class CANInterface : public QObject
{
    Q_OBJECT

public:
    explicit CANInterface(QObject *parent = nullptr);
    ~CANInterface();
    
    bool connect(const QString &portName, int baudRateKbps);
    void disconnect();
    bool isConnected() const;
    bool sendMessage(quint32 canId, const QByteArray &data);
    QStringList getAvailablePorts() const;
    void refreshPortList();

signals:
    void messageReceived(const QString &message);
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString &error);

private slots:
    void onDataReceived();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    void parseReceivedData(const QByteArray &data);
    QByteArray buildCanFrame(quint32 canId, const QByteArray &data);
    QString formatCanMessage(quint32 canId, const QByteArray &data);
    
    QSerialPort *m_serialPort;
    QByteArray m_buffer;
    bool m_connected;
    int m_currentBaudRate;
    
    // Протокол Scanmatic 2 Pro
    static constexpr quint8 FRAME_START = 0xAA;
    static constexpr quint8 FRAME_END = 0x55;
};

#endif // CANINTERFACE_H

