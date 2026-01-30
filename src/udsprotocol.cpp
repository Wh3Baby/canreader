#include "udsprotocol.h"
#include <QDebug>
#include <QRegularExpression>

UDSProtocol::UDSProtocol(CANInterface *canInterface, QObject *parent)
    : DiagnosticProtocol(canInterface, parent)
    , m_currentSession(0x01) // Default session
    , m_securityLevel(0)
{
    // UDS обычно использует функциональные адреса
    // По умолчанию используем стандартные OBD-II ID, но можно настроить
    setRequestId(0x7DF);
    setResponseId(0x7E8);
    setTimeout(5000); // UDS может требовать больше времени
}

bool UDSProtocol::testerPresent()
{
    QByteArray request = buildUDSPacket(UDSServices::TesterPresent, QByteArray());
    return sendRequest(request);
}

bool UDSProtocol::readDataByIdentifier(quint16 did, QByteArray &response)
{
    QByteArray data;
    data.append(static_cast<quint8>((did >> 8) & 0xFF));
    data.append(static_cast<quint8>(did & 0xFF));
    
    QByteArray request = buildUDSPacket(UDSServices::ReadDataByIdentifier, data);
    if (!sendRequest(request)) {
        return false;
    }
    
    // Ждем ответа (синхронно)
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        if (serviceId == UDSServices::ReadDataByIdentifierResponse) {
            response = responseData;
            return true;
        } else if (serviceId == (UDSServices::ReadDataByIdentifier | 0x40)) {
            // Positive response
            response = responseData;
            return true;
        }
    }
    
    return false;
}

bool UDSProtocol::writeDataByIdentifier(quint16 did, const QByteArray &data)
{
    QByteArray requestData;
    requestData.append(static_cast<quint8>((did >> 8) & 0xFF));
    requestData.append(static_cast<quint8>(did & 0xFF));
    requestData.append(data);
    
    QByteArray request = buildUDSPacket(UDSServices::WriteDataByIdentifier, requestData);
    if (!sendRequest(request)) {
        return false;
    }
    
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        return (serviceId == (UDSServices::WriteDataByIdentifier | 0x40) ||
                serviceId == UDSServices::WriteDataByIdentifierResponse);
    }
    
    return false;
}

bool UDSProtocol::readMemoryByAddress(quint32 address, quint32 length, QByteArray &data)
{
    QByteArray requestData;
    
    // Формат адреса и длины зависит от размера (1-4 байта)
    quint8 addressSize = 4;
    quint8 lengthSize = 4;
    
    // Определяем минимальный размер
    if (address < 0x100) addressSize = 1;
    else if (address < 0x10000) addressSize = 2;
    else if (address < 0x1000000) addressSize = 3;
    
    if (length < 0x100) lengthSize = 1;
    else if (length < 0x10000) lengthSize = 2;
    else if (length < 0x1000000) lengthSize = 3;
    
    quint8 memorySize = ((addressSize - 1) << 4) | (lengthSize - 1);
    requestData.append(memorySize);
    
    // Адрес (big-endian)
    for (int i = addressSize - 1; i >= 0; i--) {
        requestData.append(static_cast<quint8>((address >> (i * 8)) & 0xFF));
    }
    
    // Длина (big-endian)
    for (int i = lengthSize - 1; i >= 0; i--) {
        requestData.append(static_cast<quint8>((length >> (i * 8)) & 0xFF));
    }
    
    QByteArray request = buildUDSPacket(UDSServices::ReadMemoryByAddress, requestData);
    if (!sendRequest(request)) {
        return false;
    }
    
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        if (serviceId == (UDSServices::ReadMemoryByAddress | 0x40)) {
            data = responseData;
            return true;
        }
    }
    
    return false;
}

bool UDSProtocol::writeMemoryByAddress(quint32 address, const QByteArray &data)
{
    QByteArray requestData;
    
    quint8 addressSize = 4;
    if (address < 0x100) addressSize = 1;
    else if (address < 0x10000) addressSize = 2;
    else if (address < 0x1000000) addressSize = 3;
    
    quint8 memorySize = ((addressSize - 1) << 4) | 0x0F; // Данные всегда в конце
    requestData.append(memorySize);
    
    // Адрес
    for (int i = addressSize - 1; i >= 0; i--) {
        requestData.append(static_cast<quint8>((address >> (i * 8)) & 0xFF));
    }
    
    requestData.append(data);
    
    QByteArray request = buildUDSPacket(UDSServices::WriteMemoryByAddress, requestData);
    if (!sendRequest(request)) {
        return false;
    }
    
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        return (serviceId == (UDSServices::WriteMemoryByAddress | 0x40));
    }
    
    return false;
}

bool UDSProtocol::securityAccess(quint8 level, const QByteArray &key)
{
    if (key.isEmpty()) {
        // Запрос seed
        return requestSeed(level, m_seeds[level]);
    } else {
        // Отправка key
        return sendKey(level, key);
    }
}

bool UDSProtocol::requestSeed(quint8 level, QByteArray &seed)
{
    QByteArray requestData;
    requestData.append(level);
    
    QByteArray request = buildUDSPacket(UDSServices::SecurityAccess, requestData);
    if (!sendRequest(request)) {
        return false;
    }
    
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        if (serviceId == (UDSServices::SecurityAccess | 0x40)) {
            if (responseData.size() >= 1 && responseData[0] == level + 1) {
                seed = responseData.mid(1);
                m_seeds[level] = seed;
                emit securityAccessGranted(level);
                return true;
            }
        } else if (serviceId == 0x7F) {
            // Negative response
            if (responseData.size() >= 2) {
                quint8 errorCode = responseData[1];
                emit securityAccessDenied(level, errorCode);
            }
        }
    }
    
    return false;
}

bool UDSProtocol::sendKey(quint8 level, const QByteArray &key)
{
    QByteArray requestData;
    requestData.append(level + 1); // Key level = seed level + 1
    requestData.append(key);
    
    QByteArray request = buildUDSPacket(UDSServices::SecurityAccess, requestData);
    if (!sendRequest(request)) {
        return false;
    }
    
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        if (serviceId == (UDSServices::SecurityAccess | 0x40)) {
            m_securityLevel = level;
            emit securityAccessGranted(level);
            return true;
        } else if (serviceId == 0x7F) {
            if (responseData.size() >= 2) {
                quint8 errorCode = responseData[1];
                emit securityAccessDenied(level, errorCode);
            }
        }
    }
    
    return false;
}

bool UDSProtocol::clearDTC(quint8 groupOfDTC)
{
    QByteArray requestData;
    requestData.append(0xFF); // Sub-function: clear all DTCs
    requestData.append(groupOfDTC);
    requestData.append(0xFF); // DTC mask
    requestData.append(0xFF);
    requestData.append(0xFF);
    
    QByteArray request = buildUDSPacket(UDSServices::ClearDiagnosticInformation, requestData);
    if (!sendRequest(request)) {
        return false;
    }
    
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        return (serviceId == UDSServices::ClearDiagnosticInformationResponse);
    }
    
    return false;
}

bool UDSProtocol::readDTCByStatus(quint8 statusMask, QList<DTCCode> &dtcList)
{
    QByteArray requestData;
    requestData.append(0x02); // Sub-function: reportDTCByStatusMask
    requestData.append(statusMask);
    
    QByteArray request = buildUDSPacket(UDSServices::ReadDTCInformation, requestData);
    if (!sendRequest(request)) {
        return false;
    }
    
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        if (serviceId == UDSServices::ReadDTCInformationResponse) {
            dtcList.clear();
            
            // Парсим DTC коды (каждый DTC = 4 байта: 2 байта код + 1 байт статус + 1 байт расширенный статус)
            for (int i = 0; i < responseData.size(); i += 4) {
                if (i + 3 < responseData.size()) {
                    DTCCode dtc;
                    quint16 dtcCode = (static_cast<quint16>(responseData[i]) << 8) | responseData[i + 1];
                    dtc.code = dtcCode;
                    dtc.status = responseData[i + 2];
                    dtc.isActive = (dtc.status & 0x80) != 0;
                    dtc.description = dtcCodeToString(dtcCode);
                    dtc.type = formatDTC(dtcCode).left(1);
                    dtcList.append(dtc);
                }
            }
            
            emit dtcReceived(dtcList);
            return true;
        }
    }
    
    return false;
}

bool UDSProtocol::readDTCInformation(quint8 subFunction, const QByteArray &params, QByteArray &response)
{
    QByteArray requestData;
    requestData.append(subFunction);
    requestData.append(params);
    
    QByteArray request = buildUDSPacket(UDSServices::ReadDTCInformation, requestData);
    if (!sendRequest(request)) {
        return false;
    }
    
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        if (serviceId == UDSServices::ReadDTCInformationResponse) {
            response = responseData;
            return true;
        }
    }
    
    return false;
}

bool UDSProtocol::startSession(quint8 sessionType)
{
    QByteArray requestData;
    requestData.append(sessionType);
    
    QByteArray request = buildUDSPacket(0x10, requestData); // StartDiagnosticSession
    if (!sendRequest(request)) {
        return false;
    }
    
    QEventLoop loop;
    connect(this, &UDSProtocol::responseReceived, &loop, &QEventLoop::quit);
    connect(this, &UDSProtocol::errorOccurred, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (m_lastResponse.isEmpty()) {
        return false;
    }
    
    quint8 serviceId;
    QByteArray responseData;
    if (parseUDSResponse(m_lastResponse, serviceId, responseData)) {
        if (serviceId == 0x50) { // StartDiagnosticSession response
            m_currentSession = sessionType;
            return true;
        }
    }
    
    return false;
}

bool UDSProtocol::stopSession()
{
    return startSession(0x01); // Вернуться в default session
}

QString UDSProtocol::errorCodeToString(quint8 errorCode)
{
    switch (errorCode) {
        case UDSErrors::GeneralReject: return "General Reject";
        case UDSErrors::ServiceNotSupported: return "Service Not Supported";
        case UDSErrors::SubFunctionNotSupported: return "Sub-Function Not Supported";
        case UDSErrors::IncorrectMessageLengthOrInvalidFormat: return "Incorrect Message Length Or Invalid Format";
        case UDSErrors::ResponseTooLong: return "Response Too Long";
        case UDSErrors::BusyRepeatRequest: return "Busy Repeat Request";
        case UDSErrors::ConditionsNotCorrect: return "Conditions Not Correct";
        case UDSErrors::RequestSequenceError: return "Request Sequence Error";
        case UDSErrors::SecurityAccessDenied: return "Security Access Denied";
        case UDSErrors::InvalidKey: return "Invalid Key";
        case UDSErrors::ExceedNumberOfAttempts: return "Exceed Number Of Attempts";
        case UDSErrors::RequiredTimeDelayNotExpired: return "Required Time Delay Not Expired";
        default: return QString("Unknown Error (0x%1)").arg(errorCode, 2, 16, QChar('0')).toUpper();
    }
}

QString UDSProtocol::dtcCodeToString(quint16 dtcCode)
{
    // Простая конвертация DTC кода в строку
    return formatDTC(dtcCode);
}

QString UDSProtocol::formatDTC(quint16 dtcCode)
{
    quint8 firstByte = (dtcCode >> 14) & 0x03;
    quint16 code = dtcCode & 0x3FFF;
    
    QString prefix;
    switch (firstByte) {
        case 0: prefix = "P0"; break; // Powertrain
        case 1: prefix = "P1"; break;
        case 2: prefix = "C"; break;  // Chassis
        case 3: prefix = "B"; break;  // Body
        default: prefix = "U"; break; // Network
    }
    
    return QString("%1%2").arg(prefix).arg(code, 4, 16, QChar('0')).toUpper();
}

QByteArray UDSProtocol::calculateKey(const QByteArray &seed, quint32 algorithm)
{
    Q_UNUSED(algorithm);
    // Простой алгоритм (обычно используется XOR или более сложные)
    // В реальности алгоритмы специфичны для производителя
    QByteArray key = seed;
    for (int i = 0; i < key.size(); i++) {
        key[i] = static_cast<char>(key[i] ^ 0xAA);
    }
    return key;
}

bool UDSProtocol::parseUDSResponse(const QByteArray &data, quint8 &serviceId, QByteArray &responseData)
{
    if (data.isEmpty()) {
        return false;
    }
    
    serviceId = static_cast<quint8>(data[0]);
    
    // Проверяем на negative response
    if (serviceId == 0x7F) {
        responseData = data;
        return true;
    }
    
    // Positive response - убираем первый байт (service ID)
    responseData = data.mid(1);
    return true;
}

bool UDSProtocol::isPositiveResponse(quint8 serviceId, const QByteArray &response)
{
    if (response.isEmpty()) {
        return false;
    }
    
    // Negative response всегда начинается с 0x7F
    if (response[0] == 0x7F) {
        return false;
    }
    
    // Positive response = service ID | 0x40
    return (response[0] == (serviceId | 0x40));
}

QByteArray UDSProtocol::buildUDSPacket(quint8 serviceId, const QByteArray &data)
{
    QByteArray packet;
    packet.append(serviceId);
    packet.append(data);
    return packet;
}

