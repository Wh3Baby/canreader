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
#include <QTableWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QKeySequence>
#include <QShortcut>
#include <QTimer>
#include "caninterface.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onCanMessageReceived(const QString &message);
    void onCanMessageReceivedDetailed(quint32 id, const QByteArray &data, const QDateTime &timestamp);
    void onConnectionStatusChanged(bool connected);
    void onErrorOccurred(const QString &error);
    void onStatisticsUpdated();
    void onRefreshPortsClicked();
    void onClearLogClicked();
    void onSaveLogClicked();
    void onFilterToggled(bool enabled);
    void onAddFilterClicked();
    void onClearFiltersClicked();
    void onAutoRefreshPorts();

private:
    void setupUI();
    void logMessage(const QString &message, const QString &type = "INFO");
    void updateStatisticsDisplay();
    void addMessageToTable(quint32 id, const QByteArray &data, const QDateTime &timestamp, bool isReceived);
    void setupShortcuts();
    void saveSettings();
    void loadSettings();
    
    // UI элементы
    QTextEdit *m_logTextEdit;
    QTableWidget *m_messageTable;
    QPushButton *m_connectButton;
    QPushButton *m_refreshPortsButton;
    QPushButton *m_clearLogButton;
    QPushButton *m_saveLogButton;
    QComboBox *m_baudRateCombo;
    QComboBox *m_portCombo;
    QLineEdit *m_canIdEdit;
    QLineEdit *m_canDataEdit;
    QPushButton *m_sendButton;
    QLabel *m_statusLabel;
    QLabel *m_statsLabel;
    
    // Фильтры
    QCheckBox *m_filterEnabledCheck;
    QLineEdit *m_filterIdEdit;
    QPushButton *m_addFilterButton;
    QPushButton *m_clearFiltersButton;
    
    // CAN интерфейс
    CANInterface *m_canInterface;
    
    // Таймеры
    QTimer *m_autoRefreshTimer;
    
    bool m_isConnected;
    bool m_useTableView;
};

#endif // MAINWINDOW_H

