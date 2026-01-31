#ifndef USBDEVICE_H
#define USBDEVICE_H

#include <QObject>
#include <QByteArray>
#include <QTimer>

// Forward declaration для libusb
struct libusb_device_handle;
struct libusb_context;

class USBDevice : public QObject
{
    Q_OBJECT

public:
    explicit USBDevice(QObject *parent = nullptr);
    ~USBDevice();
    
    bool open(quint16 vendorId, quint16 productId);
    void close();
    bool isOpen() const;
    
    bool write(const QByteArray &data);
    QByteArray read(int timeoutMs = 1000);
    
    QString errorString() const;

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);

private slots:
    void pollUSB();

private:
    libusb_context *m_context;
    libusb_device_handle *m_handle;
    bool m_isOpen;
    quint16 m_vendorId;
    quint16 m_productId;
    QTimer *m_pollTimer;
    QString m_lastError;
    
    static constexpr int USB_TIMEOUT = 1000; // мс
    static constexpr int BULK_IN_ENDPOINT = 0x81;  // Обычно для USB Bulk IN
    static constexpr int BULK_OUT_ENDPOINT = 0x01; // Обычно для USB Bulk OUT
};

#endif // USBDEVICE_H

