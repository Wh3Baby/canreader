#include "usbdevice.h"
#include <QDebug>
#include <libusb-1.0/libusb.h>

USBDevice::USBDevice(QObject *parent)
    : QObject(parent)
    , m_context(nullptr)
    , m_handle(nullptr)
    , m_isOpen(false)
    , m_vendorId(0)
    , m_productId(0)
{
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &USBDevice::pollUSB);
    
    // Инициализация libusb
    int result = libusb_init(&m_context);
    if (result < 0) {
        m_lastError = QString("Ошибка инициализации libusb: %1").arg(libusb_error_name(result));
        qDebug() << m_lastError;
    }
}

USBDevice::~USBDevice()
{
    close();
    if (m_context) {
        libusb_exit(m_context);
        m_context = nullptr;
    }
}

bool USBDevice::open(quint16 vendorId, quint16 productId)
{
    if (!m_context) {
        m_lastError = "libusb не инициализирован";
        emit errorOccurred(m_lastError);
        return false;
    }
    
    if (m_isOpen) {
        close();
    }
    
    m_vendorId = vendorId;
    m_productId = productId;
    
    // Поиск устройства
    m_handle = libusb_open_device_with_vid_pid(m_context, vendorId, productId);
    if (!m_handle) {
        m_lastError = QString("Устройство VID:%1 PID:%2 не найдено")
                     .arg(vendorId, 4, 16, QChar('0'))
                     .arg(productId, 4, 16, QChar('0'));
        emit errorOccurred(m_lastError);
        return false;
    }
    
    // Освобождение интерфейса если он занят драйвером
    int result = libusb_set_auto_detach_kernel_driver(m_handle, 1);
    if (result != LIBUSB_SUCCESS && result != LIBUSB_ERROR_NOT_SUPPORTED) {
        qDebug() << "Предупреждение: не удалось установить auto-detach:" << libusb_error_name(result);
    }
    
    // Попытка заявить интерфейс (обычно интерфейс 0)
    result = libusb_claim_interface(m_handle, 0);
    if (result < 0) {
        m_lastError = QString("Не удалось заявить интерфейс: %1").arg(libusb_error_name(result));
        libusb_close(m_handle);
        m_handle = nullptr;
        emit errorOccurred(m_lastError);
        return false;
    }
    
    m_isOpen = true;
    m_pollTimer->start(10); // Опрос каждые 10 мс
    qDebug() << "USB устройство успешно открыто";
    return true;
}

void USBDevice::close()
{
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
    
    if (m_handle) {
        libusb_release_interface(m_handle, 0);
        libusb_close(m_handle);
        m_handle = nullptr;
    }
    
    m_isOpen = false;
    qDebug() << "USB устройство закрыто";
}

bool USBDevice::isOpen() const
{
    return m_isOpen && m_handle != nullptr;
}

bool USBDevice::write(const QByteArray &data)
{
    if (!m_isOpen || !m_handle) {
        m_lastError = "Устройство не открыто";
        emit errorOccurred(m_lastError);
        return false;
    }
    
    int transferred = 0;
    int result = libusb_bulk_transfer(m_handle, BULK_OUT_ENDPOINT,
                                      const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.constData())),
                                      data.size(), &transferred, USB_TIMEOUT);
    
    if (result < 0) {
        m_lastError = QString("Ошибка записи USB: %1").arg(libusb_error_name(result));
        emit errorOccurred(m_lastError);
        return false;
    }
    
    if (transferred != data.size()) {
        m_lastError = QString("Записано %1 из %2 байт").arg(transferred).arg(data.size());
        emit errorOccurred(m_lastError);
        return false;
    }
    
    return true;
}

QByteArray USBDevice::read(int timeoutMs)
{
    QByteArray data;
    if (!m_isOpen || !m_handle) {
        return data;
    }
    
    unsigned char buffer[512]; // Буфер для чтения
    int transferred = 0;
    int result = libusb_bulk_transfer(m_handle, BULK_IN_ENDPOINT,
                                      buffer, sizeof(buffer), &transferred, timeoutMs);
    
    if (result == LIBUSB_SUCCESS && transferred > 0) {
        data = QByteArray(reinterpret_cast<const char*>(buffer), transferred);
    } else if (result != LIBUSB_ERROR_TIMEOUT) {
        m_lastError = QString("Ошибка чтения USB: %1").arg(libusb_error_name(result));
        emit errorOccurred(m_lastError);
    }
    
    return data;
}

QString USBDevice::errorString() const
{
    return m_lastError;
}

void USBDevice::pollUSB()
{
    if (!m_isOpen || !m_handle) {
        return;
    }
    
    // Читаем данные из USB
    QByteArray data = read(0); // Неблокирующее чтение
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
    
    // Обработка событий libusb
    if (m_context) {
        struct timeval timeout = {0, 0}; // Неблокирующий
        libusb_handle_events_timeout(m_context, &timeout);
    }
}

