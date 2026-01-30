#include "obd2protocol.h"
#include <QDebug>
#include <QEventLoop>
#include <QMap>
#include <QTimer>
#include <QThread>
#include <QRegularExpression>

OBD2Protocol::OBD2Protocol(CANInterface *canInterface, QObject *parent)
    : DiagnosticProtocol(canInterface, parent)
{
    // OBD-II стандартные ID
    setRequestId(0x7DF);  // Broadcast request
    setResponseId(0x7E8); // Response ID (может быть 0x7E8-0x7EB)
    setTimeout(3000);
}

bool OBD2Protocol::readPID(quint8 mode, quint8 pid, OBD2Value &value)
{
    QByteArray request = buildOBD2Request(mode, pid);
    if (!sendRequest(request)) {
        return false;
    }
    
    QByteArray response;
    if (!waitForOBD2Response(mode, pid, response)) {
        return false;
    }
    
    value.name = pidName(pid);
    value.value = decodePIDValueString(pid, response);
    value.unit = decodePIDUnit(pid);
    value.isValid = !response.isEmpty();
    
    emit pidValueReceived(pid, value);
    return value.isValid;
}

bool OBD2Protocol::readMultiplePIDs(quint8 mode, const QList<quint8> &pids, QMap<quint8, OBD2Value> &values)
{
    values.clear();
    
    for (quint8 pid : pids) {
        OBD2Value value;
        if (readPID(mode, pid, value)) {
            values[pid] = value;
        }
        // Небольшая задержка между запросами
        QThread::msleep(50);
    }
    
    return !values.isEmpty();
}

bool OBD2Protocol::readEngineRPM(double &rpm)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::EngineRPM, value)) {
        return false;
    }
    
    rpm = value.value.toDouble();
    return value.isValid;
}

bool OBD2Protocol::readVehicleSpeed(int &speed)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::VehicleSpeed, value)) {
        return false;
    }
    
    speed = value.value.toInt();
    return value.isValid;
}

bool OBD2Protocol::readCoolantTemp(int &temp)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::CoolantTemp, value)) {
        return false;
    }
    
    temp = value.value.toInt();
    return value.isValid;
}

bool OBD2Protocol::readThrottlePosition(double &position)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::ThrottlePosition, value)) {
        return false;
    }
    
    position = value.value.toDouble();
    return value.isValid;
}

bool OBD2Protocol::readEngineLoad(double &load)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::EngineLoad, value)) {
        return false;
    }
    
    load = value.value.toDouble();
    return value.isValid;
}

bool OBD2Protocol::readFuelPressure(int &pressure)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::FuelPressure, value)) {
        return false;
    }
    
    pressure = value.value.toInt();
    return value.isValid;
}

bool OBD2Protocol::readIntakeManifoldPressure(int &pressure)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::IntakeManifoldPressure, value)) {
        return false;
    }
    
    pressure = value.value.toInt();
    return value.isValid;
}

bool OBD2Protocol::readIntakeAirTemp(int &temp)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::IntakeAirTemp, value)) {
        return false;
    }
    
    temp = value.value.toInt();
    return value.isValid;
}

bool OBD2Protocol::readMAFAirFlowRate(double &rate)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::MAFAirFlowRate, value)) {
        return false;
    }
    
    rate = value.value.toDouble();
    return value.isValid;
}

bool OBD2Protocol::readTimingAdvance(double &advance)
{
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, OBD2PIDs::TimingAdvance, value)) {
        return false;
    }
    
    advance = value.value.toDouble();
    return value.isValid;
}

bool OBD2Protocol::readShortTermFuelTrim(int bank, double &trim)
{
    quint8 pid = (bank == 1) ? OBD2PIDs::ShortTermFuelTrim_Bank1 : OBD2PIDs::ShortTermFuelTrim_Bank2;
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, pid, value)) {
        return false;
    }
    
    trim = value.value.toDouble();
    return value.isValid;
}

bool OBD2Protocol::readLongTermFuelTrim(int bank, double &trim)
{
    quint8 pid = (bank == 1) ? OBD2PIDs::LongTermFuelTrim_Bank1 : OBD2PIDs::LongTermFuelTrim_Bank2;
    OBD2Value value;
    if (!readPID(OBD2Services::ShowCurrentData, pid, value)) {
        return false;
    }
    
    trim = value.value.toDouble();
    return value.isValid;
}

bool OBD2Protocol::readStoredDTC(QList<QString> &dtcList)
{
    QByteArray request = buildOBD2Request(OBD2Services::ShowStoredDTC, 0x00);
    if (!sendRequest(request)) {
        return false;
    }
    
    QByteArray response;
    if (!waitForOBD2Response(OBD2Services::ShowStoredDTC, 0x00, response)) {
        return false;
    }
    
    dtcList.clear();
    
    // Формат ответа: [mode+0x40] [count] [DTC1_high] [DTC1_low] [DTC2_high] [DTC2_low] ...
    if (response.size() >= 2) {
        quint8 count = response[1];
        for (int i = 0; i < count && (i * 2 + 3) < response.size(); i++) {
            quint8 high = response[i * 2 + 2];
            quint8 low = response[i * 2 + 3];
            quint16 dtc = (static_cast<quint16>(high) << 8) | low;
            dtcList.append(formatDTC(QString::number(dtc, 16)));
        }
    }
    
    emit dtcReceived(dtcList);
    return true;
}

bool OBD2Protocol::clearDTC()
{
    QByteArray request = buildOBD2Request(OBD2Services::ClearDTCAndStoredValues, 0x00);
    if (!sendRequest(request)) {
        return false;
    }
    
    QByteArray response;
    return waitForOBD2Response(OBD2Services::ClearDTCAndStoredValues, 0x00, response);
}

bool OBD2Protocol::readPendingDTC(QList<QString> &dtcList)
{
    QByteArray request = buildOBD2Request(OBD2Services::ShowPendingDTC, 0x00);
    if (!sendRequest(request)) {
        return false;
    }
    
    QByteArray response;
    if (!waitForOBD2Response(OBD2Services::ShowPendingDTC, 0x00, response)) {
        return false;
    }
    
    dtcList.clear();
    
    if (response.size() >= 2) {
        quint8 count = response[1];
        for (int i = 0; i < count && (i * 2 + 3) < response.size(); i++) {
            quint8 high = response[i * 2 + 2];
            quint8 low = response[i * 2 + 3];
            quint16 dtc = (static_cast<quint16>(high) << 8) | low;
            dtcList.append(formatDTC(QString::number(dtc, 16)));
        }
    }
    
    emit dtcReceived(dtcList);
    return true;
}

bool OBD2Protocol::readVIN(QString &vin)
{
    // VIN читается через режим 09, PID 0x02
    QByteArray request = buildOBD2Request(OBD2Services::RequestVehicleInfo, 0x02);
    if (!sendRequest(request)) {
        return false;
    }
    
    QByteArray response;
    if (!waitForOBD2Response(OBD2Services::RequestVehicleInfo, 0x02, response)) {
        return false;
    }
    
    // VIN в ASCII формате
    if (response.size() > 1) {
        vin = QString::fromLatin1(response.mid(1));
        return true;
    }
    
    return false;
}

bool OBD2Protocol::readCalibrationID(QString &calId)
{
    QByteArray request = buildOBD2Request(OBD2Services::RequestVehicleInfo, 0x04);
    if (!sendRequest(request)) {
        return false;
    }
    
    QByteArray response;
    if (!waitForOBD2Response(OBD2Services::RequestVehicleInfo, 0x04, response)) {
        return false;
    }
    
    if (response.size() > 1) {
        calId = QString::fromLatin1(response.mid(1));
        return true;
    }
    
    return false;
}

bool OBD2Protocol::readECUName(QString &ecuName)
{
    QByteArray request = buildOBD2Request(OBD2Services::RequestVehicleInfo, 0x0A);
    if (!sendRequest(request)) {
        return false;
    }
    
    QByteArray response;
    if (!waitForOBD2Response(OBD2Services::RequestVehicleInfo, 0x0A, response)) {
        return false;
    }
    
    if (response.size() > 1) {
        ecuName = QString::fromLatin1(response.mid(1));
        return true;
    }
    
    return false;
}

QString OBD2Protocol::formatDTC(const QString &dtcCode)
{
    bool ok;
    quint16 code = dtcCode.toUShort(&ok, 16);
    if (!ok) {
        return dtcCode;
    }
    
    quint8 firstByte = (code >> 14) & 0x03;
    quint16 dtc = code & 0x3FFF;
    
    QString prefix;
    switch (firstByte) {
        case 0: prefix = "P0"; break;
        case 1: prefix = "P1"; break;
        case 2: prefix = "C"; break;
        case 3: prefix = "B"; break;
        default: prefix = "U"; break;
    }
    
    return QString("%1%2").arg(prefix).arg(dtc, 4, 16, QChar('0')).toUpper();
}

QString OBD2Protocol::pidName(quint8 pid)
{
    static QMap<quint8, QString> pidNames = {
        {OBD2PIDs::EngineRPM, "Engine RPM"},
        {OBD2PIDs::VehicleSpeed, "Vehicle Speed"},
        {OBD2PIDs::CoolantTemp, "Coolant Temperature"},
        {OBD2PIDs::ThrottlePosition, "Throttle Position"},
        {OBD2PIDs::EngineLoad, "Engine Load"},
        {OBD2PIDs::FuelPressure, "Fuel Pressure"},
        {OBD2PIDs::IntakeManifoldPressure, "Intake Manifold Pressure"},
        {OBD2PIDs::IntakeAirTemp, "Intake Air Temperature"},
        {OBD2PIDs::MAFAirFlowRate, "MAF Air Flow Rate"},
        {OBD2PIDs::TimingAdvance, "Timing Advance"},
    };
    
    return pidNames.value(pid, QString("PID 0x%1").arg(pid, 2, 16, QChar('0')).toUpper());
}

double OBD2Protocol::decodePIDValue(quint8 pid, const QByteArray &data)
{
    if (data.size() < 3) {
        return 0.0;
    }
    
    quint8 a = static_cast<quint8>(data[1]);
    quint8 b = static_cast<quint8>(data[2]);
    
    switch (pid) {
        case OBD2PIDs::EngineRPM:
            return ((a * 256.0) + b) / 4.0;
        case OBD2PIDs::VehicleSpeed:
            return a;
        case OBD2PIDs::CoolantTemp:
            return a - 40;
        case OBD2PIDs::ThrottlePosition:
            return (a * 100.0) / 255.0;
        case OBD2PIDs::EngineLoad:
            return (a * 100.0) / 255.0;
        case OBD2PIDs::FuelPressure:
            return a * 3;
        case OBD2PIDs::IntakeManifoldPressure:
            return a;
        case OBD2PIDs::IntakeAirTemp:
            return a - 40;
        case OBD2PIDs::MAFAirFlowRate:
            return ((a * 256.0) + b) / 100.0;
        case OBD2PIDs::TimingAdvance:
            return (a / 2.0) - 64.0;
        default:
            return static_cast<double>(a);
    }
}

QString OBD2Protocol::decodePIDUnit(quint8 pid)
{
    switch (pid) {
        case OBD2PIDs::EngineRPM: return "rpm";
        case OBD2PIDs::VehicleSpeed: return "km/h";
        case OBD2PIDs::CoolantTemp:
        case OBD2PIDs::IntakeAirTemp: return "°C";
        case OBD2PIDs::ThrottlePosition:
        case OBD2PIDs::EngineLoad: return "%";
        case OBD2PIDs::FuelPressure:
        case OBD2PIDs::IntakeManifoldPressure: return "kPa";
        case OBD2PIDs::MAFAirFlowRate: return "g/s";
        case OBD2PIDs::TimingAdvance: return "°";
        default: return "";
    }
}

QString OBD2Protocol::decodePIDValueString(quint8 pid, const QByteArray &data)
{
    double value = decodePIDValue(pid, data);
    QString unit = decodePIDUnit(pid);
    
    if (unit.isEmpty()) {
        return QString::number(value, 'f', 2);
    }
    
    return QString::number(value, 'f', 2) + " " + unit;
}

bool OBD2Protocol::parseOBD2Response(const QByteArray &data, quint8 &mode, quint8 &pid, QByteArray &responseData)
{
    if (data.size() < 2) {
        return false;
    }
    
    quint8 responseMode = static_cast<quint8>(data[0]);
    
    // Positive response = mode | 0x40
    if (responseMode < 0x40) {
        return false;
    }
    
    mode = responseMode - 0x40;
    
    if (data.size() > 1) {
        pid = static_cast<quint8>(data[1]);
        responseData = data.mid(1);
    } else {
        pid = 0;
        responseData = data.mid(1);
    }
    
    return true;
}

QByteArray OBD2Protocol::buildOBD2Request(quint8 mode, quint8 pid)
{
    QByteArray request;
    request.append(mode);
    request.append(pid);
    return request;
}

bool OBD2Protocol::waitForOBD2Response(quint8 expectedMode, quint8 expectedPid, QByteArray &response, int timeout)
{
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(timeout);
    
    connect(this, &OBD2Protocol::responseReceived, &loop, [&](const QByteArray &resp) {
        quint8 mode, pid;
        QByteArray responseData;
        if (parseOBD2Response(resp, mode, pid, responseData)) {
            if (mode == expectedMode && pid == expectedPid) {
                response = resp;
                loop.quit();
            }
        }
    });
    
    connect(this, &OBD2Protocol::errorOccurred, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeoutTimer.start();
    loop.exec();
    timeoutTimer.stop();
    
    return !response.isEmpty();
}

