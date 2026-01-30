#ifndef DIAGNOSTICPROTOCOL_H
#define DIAGNOSTICPROTOCOL_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QDateTime>
#include <QTimer>

class CANInterface;

// Базовый класс для диагностических протоколов
class DiagnosticProtocol : public QObject
{
    Q_OBJECT

public:
    explicit DiagnosticProtocol(CANInterface *canInterface, QObject *parent = nullptr);
    virtual ~DiagnosticProtocol() = default;
    
    // Общие методы
    virtual QString protocolName() const = 0;
    virtual bool isSupported() const { return true; }
    
    // Настройки
    void setRequestId(quint32 id) { m_requestId = id; }
    void setResponseId(quint32 id) { m_responseId = id; }
    void setTimeout(int milliseconds) { m_timeout = milliseconds; }
    quint32 requestId() const { return m_requestId; }
    quint32 responseId() const { return m_responseId; }
    int timeout() const { return m_timeout; }

signals:
    void responseReceived(const QByteArray &response);
    void errorOccurred(const QString &error);
    void timeoutOccurred();

protected:
    CANInterface *m_canInterface;
    quint32 m_requestId;
    quint32 m_responseId;
    int m_timeout;
    QTimer *m_responseTimer;
    bool m_waitingForResponse;
    QByteArray m_lastResponse;
    
    virtual bool sendRequest(const QByteArray &request);
    virtual void processResponse(quint32 id, const QByteArray &data);
    virtual QByteArray buildRequest(const QByteArray &serviceData);
    virtual bool parseResponse(const QByteArray &data, QByteArray &responseData);
    
private slots:
    void onCanMessageReceived(quint32 id, const QByteArray &data, const QDateTime &timestamp);
    void onResponseTimeout();
};

#endif // DIAGNOSTICPROTOCOL_H

