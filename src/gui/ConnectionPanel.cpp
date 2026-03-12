#include "ConnectionPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDebug>

namespace AetherSDR {

ConnectionPanel::ConnectionPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(6);

    // Status row
    auto* statusRow = new QHBoxLayout;
    m_indicatorLabel = new QLabel("●", this);
    m_indicatorLabel->setFixedWidth(20);
    m_indicatorLabel->setAlignment(Qt::AlignCenter);
    setConnected(false);  // set initial colour

    m_statusLabel = new QLabel("Not connected", this);
    statusRow->addWidget(m_indicatorLabel);
    statusRow->addWidget(m_statusLabel, 1);
    vbox->addLayout(statusRow);

    // Discovered radios list
    auto* group = new QGroupBox("Discovered Radios", this);
    auto* gbox  = new QVBoxLayout(group);
    m_radioList = new QListWidget(group);
    m_radioList->setSelectionMode(QAbstractItemView::SingleSelection);
    gbox->addWidget(m_radioList);
    vbox->addWidget(group, 1);

    // Connect/disconnect button
    m_connectBtn = new QPushButton("Connect", this);
    m_connectBtn->setEnabled(false);
    vbox->addWidget(m_connectBtn);

    connect(m_radioList, &QListWidget::itemSelectionChanged,
            this, &ConnectionPanel::onListSelectionChanged);
    connect(m_connectBtn, &QPushButton::clicked,
            this, &ConnectionPanel::onConnectClicked);
}

void ConnectionPanel::setConnected(bool connected)
{
    m_connected = connected;
    m_indicatorLabel->setStyleSheet(
        connected ? "color: #00e5ff; font-size: 18px;"
                  : "color: #404040; font-size: 18px;");
    m_connectBtn->setText(connected ? "Disconnect" : "Connect");
    m_connectBtn->setEnabled(connected || m_radioList->currentItem() != nullptr);
}

void ConnectionPanel::setStatusText(const QString& text)
{
    m_statusLabel->setText(text);
}

// ─── Radio list management ────────────────────────────────────────────────────

void ConnectionPanel::onRadioDiscovered(const RadioInfo& radio)
{
    m_radios.append(radio);
    m_radioList->addItem(radio.displayName());
}

void ConnectionPanel::onRadioUpdated(const RadioInfo& radio)
{
    for (int i = 0; i < m_radios.size(); ++i) {
        if (m_radios[i].serial == radio.serial) {
            m_radios[i] = radio;
            m_radioList->item(i)->setText(radio.displayName());
            return;
        }
    }
}

void ConnectionPanel::onRadioLost(const QString& serial)
{
    for (int i = 0; i < m_radios.size(); ++i) {
        if (m_radios[i].serial == serial) {
            delete m_radioList->takeItem(i);
            m_radios.removeAt(i);
            return;
        }
    }
}

void ConnectionPanel::onListSelectionChanged()
{
    m_connectBtn->setEnabled(!m_connected && m_radioList->currentItem() != nullptr);
}

void ConnectionPanel::onConnectClicked()
{
    if (m_connected) {
        emit disconnectRequested();
        return;
    }

    const int row = m_radioList->currentRow();
    if (row < 0 || row >= m_radios.size()) return;
    emit connectRequested(m_radios[row]);
}

} // namespace AetherSDR
