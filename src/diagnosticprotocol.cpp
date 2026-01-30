#include "diagnosticprotocol.h"
#include "caninterface.h"
#include <QDebug>
#include <QEventLoop>

DiagnosticProtocol::DiagnosticProtocol(CANInterface *canInterface, QObject *parent)
    : QObject(parent)
    , m_canInterface(canInterface)
    , m_requestId(0x7DF)  // Стандартный OBD-II request ID
    , m_responseId(0x7E8) // Стандартный OBD-II response ID
    , m_timeout(3000)
    , m_waitingForResponse(false)
{
    m_responseTimer = new QTimer(this);
    m_responseTimer->setSingleShot(true);
    connect(m_responseTimer, &QTimer::timeout, this, &DiagnosticProtocol::onResponseTimeout);
    
    if (m_canInterface) {
        connect(m_canInterface, &CANInterface::messageReceivedDetailed,
                this, &DiagnosticProtocol::onCanMessageReceived);
    }
}

bool DiagnosticProtocol::sendRequest(const QByteArray &request)
{
    if (!m_canInterface || !m_canInterface->isConnected()) {
        emit errorOccurred("CAN интерфейс не подключен");
        return false;
    }
    
    QByteArray frame = buildRequest(request);
    if (frame.isEmpty()) {
        emit errorOccurred("Ошибка построения запроса");
        return false;
    }
    
    m_waitingForResponse = true;
    m_lastResponse.clear();
    m_responseTimer->start(m_timeout);
    
    bool sent = m_canInterface->sendMessage(m_requestId, frame);
    if (!sent) {
        m_waitingForResponse = false;
        m_responseTimer->stop();
        emit errorOccurred("Ошибка отправки запроса");
        return false;
    }
    
    return true;
}

QByteArray DiagnosticProtocol::buildRequest(const QByteArray &serviceData)
{
    // Базовая реализация - просто возвращаем данные сервиса
    // Подклассы могут переопределить для добавления заголовков
    return serviceData;
}

bool DiagnosticProtocol::parseResponse(const QByteArray &data, QByteArray &responseData)
{
    // Базовая реализация - просто копируем данные
    // Подклассы должны переопределить для парсинга протокола
    responseData = data;
    return true;
}

void DiagnosticProtocol::onCanMessageReceived(quint32 id, const QByteArray &data, const QDateTime &timestamp)
{
    Q_UNUSED(timestamp);
    
    if (!m_waitingForResponse) {
        return;
    }
    
    // Проверяем, что это ответ на наш запрос
    if (id != m_responseId && id != (m_responseId + 1) && id != (m_responseId + 2) && id != (m_responseId + 3)) {
        // OBD-II может использовать несколько ID для ответов (0x7E8-0x7EB)
        // UDS обычно использует один ID, но может быть расширен
        return;
    }
    
    QByteArray responseData;
    if (parseResponse(data, responseData)) {
        m_responseTimer->stop();
        m_waitingForResponse = false;
        m_lastResponse = responseData;
        emit responseReceived(responseData);
    }
}

void DiagnosticProtocol::onResponseTimeout()
{
    if (m_waitingForResponse) {
        m_waitingForResponse = false;
        emit timeoutOccurred();
        emit errorOccurred("Таймаут ожидания ответа");
    }
}

void DiagnosticProtocol::processResponse(quint32 id, const QByteArray &data)
{
    Q_UNUSED(id);
    Q_UNUSED(data);
    // Базовая реализация - подклассы могут переопределить
}

