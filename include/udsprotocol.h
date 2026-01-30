#ifndef UDSPROTOCOL_H
#define UDSPROTOCOL_H

#include "diagnosticprotocol.h"
#include <QMap>

// UDS Service IDs (ISO 14229)
namespace UDSServices {
    constexpr quint8 TesterPresent = 0x3E;
    constexpr quint8 ReadDataByIdentifier = 0x22;
    constexpr quint8 ReadMemoryByAddress = 0x23;
    constexpr quint8 ReadScalingDataByIdentifier = 0x24;
    constexpr quint8 SecurityAccess = 0x27;
    constexpr quint8 CommunicationControl = 0x28;
    constexpr quint8 ReadDataByPeriodicIdentifier = 0x2A;
    constexpr quint8 DynamicallyDefineDataIdentifier = 0x2C;
    constexpr quint8 WriteDataByIdentifier = 0x2E;
    constexpr quint8 InputOutputControlByIdentifier = 0x2F;
    constexpr quint8 RoutineControl = 0x31;
    constexpr quint8 RequestDownload = 0x34;
    constexpr quint8 RequestUpload = 0x35;
    constexpr quint8 TransferData = 0x36;
    constexpr quint8 RequestTransferExit = 0x37;
    constexpr quint8 RequestFileTransfer = 0x38;
    constexpr quint8 WriteMemoryByAddress = 0x3D;
    constexpr quint8 ClearDiagnosticInformation = 0x14;
    constexpr quint8 ReadDTCInformation = 0x19;
    constexpr quint8 ReadDTCByStatusMask = 0x19;
    constexpr quint8 ControlDTCSetting = 0x85;
    constexpr quint8 ResponseOnEvent = 0x86;
    constexpr quint8 LinkControl = 0x87;
    constexpr quint8 ReadDataByIdentifierResponse = 0x62;
    constexpr quint8 WriteDataByIdentifierResponse = 0x6E;
    constexpr quint8 SecurityAccessResponse = 0x67;
    constexpr quint8 ClearDiagnosticInformationResponse = 0x54;
    constexpr quint8 ReadDTCInformationResponse = 0x59;
}

// UDS Negative Response Codes
namespace UDSErrors {
    constexpr quint8 PositiveResponse = 0x00;
    constexpr quint8 GeneralReject = 0x10;
    constexpr quint8 ServiceNotSupported = 0x11;
    constexpr quint8 SubFunctionNotSupported = 0x12;
    constexpr quint8 IncorrectMessageLengthOrInvalidFormat = 0x13;
    constexpr quint8 ResponseTooLong = 0x14;
    constexpr quint8 BusyRepeatRequest = 0x21;
    constexpr quint8 ConditionsNotCorrect = 0x22;
    constexpr quint8 RequestSequenceError = 0x24;
    constexpr quint8 NoResponseFromSubnetComponent = 0x25;
    constexpr quint8 FailurePreventsExecutionOfRequestedAction = 0x26;
    constexpr quint8 RequestOutOfRange = 0x31;
    constexpr quint8 SecurityAccessDenied = 0x33;
    constexpr quint8 InvalidKey = 0x35;
    constexpr quint8 ExceedNumberOfAttempts = 0x36;
    constexpr quint8 RequiredTimeDelayNotExpired = 0x37;
    constexpr quint8 UploadDownloadNotAccepted = 0x70;
    constexpr quint8 TransferDataSuspended = 0x71;
    constexpr quint8 GeneralProgrammingFailure = 0x72;
    constexpr quint8 WrongBlockSequenceCounter = 0x73;
    constexpr quint8 RequestCorrectlyReceived_ResponsePending = 0x78;
    constexpr quint8 SubFunctionNotSupportedInActiveSession = 0x7E;
    constexpr quint8 ServiceNotSupportedInActiveSession = 0x7F;
}

struct DTCCode {
    quint16 code;
    QString description;
    QString type; // P, B, C, U
    bool isActive;
    quint8 status;
};

class UDSProtocol : public DiagnosticProtocol
{
    Q_OBJECT

public:
    explicit UDSProtocol(CANInterface *canInterface, QObject *parent = nullptr);
    
    QString protocolName() const override { return "UDS (ISO 14229)"; }
    
    // Базовые команды
    bool testerPresent();
    bool readDataByIdentifier(quint16 did, QByteArray &response);
    bool writeDataByIdentifier(quint16 did, const QByteArray &data);
    bool readMemoryByAddress(quint32 address, quint32 length, QByteArray &data);
    bool writeMemoryByAddress(quint32 address, const QByteArray &data);
    
    // Безопасный доступ
    bool securityAccess(quint8 level, const QByteArray &key = QByteArray());
    bool requestSeed(quint8 level, QByteArray &seed);
    bool sendKey(quint8 level, const QByteArray &key);
    
    // DTC (Diagnostic Trouble Codes)
    bool clearDTC(quint8 groupOfDTC = 0xFF);
    bool readDTCByStatus(quint8 statusMask, QList<DTCCode> &dtcList);
    bool readDTCInformation(quint8 subFunction, const QByteArray &params, QByteArray &response);
    
    // Сессии
    bool startSession(quint8 sessionType);
    bool stopSession();
    
    // Утилиты
    static QString errorCodeToString(quint8 errorCode);
    static QString dtcCodeToString(quint16 dtcCode);
    static QString formatDTC(quint16 dtcCode);
    
    // Seed & Key алгоритмы (базовые)
    static QByteArray calculateKey(const QByteArray &seed, quint32 algorithm = 0);

signals:
    void dtcReceived(const QList<DTCCode> &dtcList);
    void securityAccessGranted(quint8 level);
    void securityAccessDenied(quint8 level, quint8 reason);

private:
    quint8 m_currentSession;
    quint8 m_securityLevel;
    QMap<quint8, QByteArray> m_seeds; // Сохраненные seeds для уровней
    
    bool parseUDSResponse(const QByteArray &data, quint8 &serviceId, QByteArray &responseData);
    bool isPositiveResponse(quint8 serviceId, const QByteArray &response);
    QByteArray buildUDSPacket(quint8 serviceId, const QByteArray &data);
};

#endif // UDSPROTOCOL_H

