#pragma once

#include "core/RadioDiscovery.h"

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

namespace AetherSDR {

// Panel that shows discovered radios and a Connect/Disconnect button.
class ConnectionPanel : public QWidget {
    Q_OBJECT

public:
    explicit ConnectionPanel(QWidget* parent = nullptr);

    void setConnected(bool connected);
    void setStatusText(const QString& text);

public slots:
    void onRadioDiscovered(const RadioInfo& radio);
    void onRadioUpdated(const RadioInfo& radio);
    void onRadioLost(const QString& serial);

signals:
    void connectRequested(const RadioInfo& radio);
    void disconnectRequested();

private slots:
    void onConnectClicked();
    void onListSelectionChanged();

private:
    QListWidget* m_radioList;
    QPushButton* m_connectBtn;
    QLabel*      m_statusLabel;
    QLabel*      m_indicatorLabel;

    QList<RadioInfo> m_radios;   // mirror of what's in the list
    bool m_connected{false};
};

} // namespace AetherSDR
