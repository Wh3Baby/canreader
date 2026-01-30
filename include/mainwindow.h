#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QStatusBar>
#include "caninterface.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onCanMessageReceived(const QString &message);
    void onConnectionStatusChanged(bool connected);
    void onErrorOccurred(const QString &error);

private:
    void setupUI();
    void logMessage(const QString &message, const QString &type = "INFO");
    
    // UI элементы
    QTextEdit *m_logTextEdit;
    QPushButton *m_connectButton;
    QComboBox *m_baudRateCombo;
    QComboBox *m_portCombo;
    QLineEdit *m_canIdEdit;
    QLineEdit *m_canDataEdit;
    QPushButton *m_sendButton;
    QLabel *m_statusLabel;
    
    // CAN интерфейс
    CANInterface *m_canInterface;
    
    bool m_isConnected;
};

#endif // MAINWINDOW_H

