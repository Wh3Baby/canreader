#ifndef OBD2PROTOCOL_H
#define OBD2PROTOCOL_H

#include "diagnosticprotocol.h"
#include <QMap>

// OBD-II Service IDs (SAE J1979)
namespace OBD2Services {
    constexpr quint8 ShowCurrentData = 0x01;
    constexpr quint8 ShowFreezeFrameData = 0x02;
    constexpr quint8 ShowStoredDTC = 0x03;
    constexpr quint8 ClearDTCAndStoredValues = 0x04;
    constexpr quint8 TestResults_OxygenSensor = 0x05;
    constexpr quint8 TestResults_OnBoard = 0x06;
    constexpr quint8 ShowPendingDTC = 0x07;
    constexpr quint8 ControlOperation = 0x08;
    constexpr quint8 RequestVehicleInfo = 0x09;
}

// OBD-II PIDs (Parameter IDs)
namespace OBD2PIDs {
    // Mode 01 PIDs
    constexpr quint8 SupportedPIDs_01_20 = 0x00;
    constexpr quint8 MonitorStatus = 0x01;
    constexpr quint8 FreezeDTC = 0x02;
    constexpr quint8 FuelSystemStatus = 0x03;
    constexpr quint8 EngineLoad = 0x04;
    constexpr quint8 CoolantTemp = 0x05;
    constexpr quint8 ShortTermFuelTrim_Bank1 = 0x06;
    constexpr quint8 LongTermFuelTrim_Bank1 = 0x07;
    constexpr quint8 ShortTermFuelTrim_Bank2 = 0x08;
    constexpr quint8 LongTermFuelTrim_Bank2 = 0x09;
    constexpr quint8 FuelPressure = 0x0A;
    constexpr quint8 IntakeManifoldPressure = 0x0B;
    constexpr quint8 EngineRPM = 0x0C;
    constexpr quint8 VehicleSpeed = 0x0D;
    constexpr quint8 TimingAdvance = 0x0E;
    constexpr quint8 IntakeAirTemp = 0x0F;
    constexpr quint8 MAFAirFlowRate = 0x10;
    constexpr quint8 ThrottlePosition = 0x11;
    constexpr quint8 CommandedSecondaryAirStatus = 0x12;
    constexpr quint8 OxygenSensorsPresent = 0x13;
    constexpr quint8 OxygenSensor1 = 0x14;
    constexpr quint8 OxygenSensor2 = 0x15;
    constexpr quint8 OxygenSensor3 = 0x16;
    constexpr quint8 OxygenSensor4 = 0x17;
    constexpr quint8 OxygenSensor5 = 0x18;
    constexpr quint8 OxygenSensor6 = 0x19;
    constexpr quint8 OxygenSensor7 = 0x1A;
    constexpr quint8 OxygenSensor8 = 0x1B;
    constexpr quint8 OBDStandard = 0x1C;
    constexpr quint8 OxygenSensorsPresent_Alt = 0x1D;
    constexpr quint8 AuxiliaryInputStatus = 0x1E;
    constexpr quint8 EngineRunTime = 0x1F;
}

struct OBD2Value {
    QString name;
    QString value;
    QString unit;
    bool isValid;
};

class OBD2Protocol : public DiagnosticProtocol
{
    Q_OBJECT

public:
    explicit OBD2Protocol(CANInterface *canInterface, QObject *parent = nullptr);
    
    QString protocolName() const override { return "OBD-II (SAE J1979)"; }
    
    // Базовые команды
    bool readPID(quint8 mode, quint8 pid, OBD2Value &value);
    bool readMultiplePIDs(quint8 mode, const QList<quint8> &pids, QMap<quint8, OBD2Value> &values);
    
    // Режим 01 - Текущие данные
    bool readEngineRPM(double &rpm);
    bool readVehicleSpeed(int &speed);
    bool readCoolantTemp(int &temp);
    bool readThrottlePosition(double &position);
    bool readEngineLoad(double &load);
    bool readFuelPressure(int &pressure);
    bool readIntakeManifoldPressure(int &pressure);
    bool readIntakeAirTemp(int &temp);
    bool readMAFAirFlowRate(double &rate);
    bool readTimingAdvance(double &advance);
    bool readShortTermFuelTrim(int bank, double &trim);
    bool readLongTermFuelTrim(int bank, double &trim);
    
    // Режим 03 - Сохраненные DTC
    bool readStoredDTC(QList<QString> &dtcList);
    
    // Режим 04 - Очистка DTC
    bool clearDTC();
    
    // Режим 07 - Ожидающие DTC
    bool readPendingDTC(QList<QString> &dtcList);
    
    // Режим 09 - Информация о транспортном средстве
    bool readVIN(QString &vin);
    bool readCalibrationID(QString &calId);
    bool readECUName(QString &ecuName);
    
    // Утилиты
    static QString formatDTC(const QString &dtcCode);
    static QString pidName(quint8 pid);
    static double decodePIDValue(quint8 pid, const QByteArray &data);
    static QString decodePIDUnit(quint8 pid);
    static QString decodePIDValueString(quint8 pid, const QByteArray &data);

signals:
    void pidValueReceived(quint8 pid, const OBD2Value &value);
    void dtcReceived(const QList<QString> &dtcList);

private:
    bool parseOBD2Response(const QByteArray &data, quint8 &mode, quint8 &pid, QByteArray &responseData);
    QByteArray buildOBD2Request(quint8 mode, quint8 pid);
    bool waitForOBD2Response(quint8 expectedMode, quint8 expectedPid, QByteArray &response, int timeout = 3000);
};

#endif // OBD2PROTOCOL_H

