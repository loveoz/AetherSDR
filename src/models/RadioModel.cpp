#include "RadioModel.h"
#include <QDebug>
#include <QRegularExpression>

namespace AetherSDR {

RadioModel::RadioModel(QObject* parent)
    : QObject(parent)
{
    connect(&m_connection, &RadioConnection::statusReceived,
            this, &RadioModel::onStatusReceived);
    connect(&m_connection, &RadioConnection::connected,
            this, &RadioModel::onConnected);
    connect(&m_connection, &RadioConnection::disconnected,
            this, &RadioModel::onDisconnected);
    connect(&m_connection, &RadioConnection::errorOccurred,
            this, &RadioModel::onConnectionError);
    connect(&m_connection, &RadioConnection::versionReceived,
            this, &RadioModel::onVersionReceived);
}

bool RadioModel::isConnected() const
{
    return m_connection.isConnected();
}

SliceModel* RadioModel::slice(int id) const
{
    for (auto* s : m_slices)
        if (s->sliceId() == id) return s;
    return nullptr;
}

// ─── Actions ──────────────────────────────────────────────────────────────────

void RadioModel::connectToRadio(const RadioInfo& info)
{
    m_name  = info.name;
    m_model = info.model;
    m_connection.connectToRadio(info);
}

void RadioModel::disconnectFromRadio()
{
    m_connection.disconnectFromRadio();
}

void RadioModel::setTransmit(bool tx)
{
    m_connection.sendCommand(QString("xmit %1").arg(tx ? 1 : 0));
}

// ─── Connection slots ─────────────────────────────────────────────────────────

void RadioModel::onConnected()
{
    qDebug() << "RadioModel: connected";
    emit connectionStateChanged(true);

    // Flush any slice commands that queued up before we connected
    for (auto* s : m_slices) {
        for (const QString& cmd : s->drainPendingCommands())
            m_connection.sendCommand(cmd);
    }
}

void RadioModel::onDisconnected()
{
    qDebug() << "RadioModel: disconnected";
    emit connectionStateChanged(false);
}

void RadioModel::onConnectionError(const QString& msg)
{
    qWarning() << "RadioModel: connection error:" << msg;
    emit connectionError(msg);
    emit connectionStateChanged(false);
}

void RadioModel::onVersionReceived(const QString& v)
{
    m_version = v;
    emit infoChanged();
}

// ─── Status dispatch ──────────────────────────────────────────────────────────
//
// Object strings look like:
//   "radio"           → global radio properties
//   "slice 0"         → slice receiver
//   "panadapter 0"    → panadapter (spectrum)
//   "meter 1"         → meter reading
//   "removed=True"    → object was removed

void RadioModel::onStatusReceived(const QString& object,
                                  const QMap<QString, QString>& kvs)
{
    if (object == "radio") {
        handleRadioStatus(kvs);
        return;
    }

    static const QRegularExpression sliceRe(R"(^slice\s+(\d+)$)");
    const auto sliceMatch = sliceRe.match(object);
    if (sliceMatch.hasMatch()) {
        const bool removed = kvs.value("in_use") == "0";
        handleSliceStatus(sliceMatch.captured(1).toInt(), kvs, removed);
        return;
    }

    if (object.startsWith("meter")) {
        handleMeterStatus(kvs);
    }
}

void RadioModel::handleRadioStatus(const QMap<QString, QString>& kvs)
{
    bool changed = false;
    if (kvs.contains("model")) { m_model = kvs["model"]; changed = true; }
    if (changed) emit infoChanged();
}

void RadioModel::handleSliceStatus(int id,
                                    const QMap<QString, QString>& kvs,
                                    bool removed)
{
    SliceModel* s = slice(id);

    if (removed) {
        if (s) {
            m_slices.removeOne(s);
            emit sliceRemoved(id);
            s->deleteLater();
        }
        return;
    }

    if (!s) {
        s = new SliceModel(id, this);
        // Forward slice commands to the radio
        connect(s, &SliceModel::commandReady, this, [this](const QString& cmd){
            m_connection.sendCommand(cmd);
        });
        m_slices.append(s);
        emit sliceAdded(s);
    }

    s->applyStatus(kvs);

    // Send any queued commands (e.g. if GUI changed freq before status arrived)
    if (m_connection.isConnected()) {
        for (const QString& cmd : s->drainPendingCommands())
            m_connection.sendCommand(cmd);
    }
}

void RadioModel::handleMeterStatus(const QMap<QString, QString>& kvs)
{
    // Meter format: "1.num=100 1.nam=FWDPWR 1.low=-150.0 1.hi=20.0 1.desc=Forward Power"
    // In practice the radio sends meter readings as "num" with a float value.
    if (kvs.contains("fwdpwr"))
        m_txPower = kvs["fwdpwr"].toFloat();
    if (kvs.contains("patemp"))
        m_paTemp = kvs["patemp"].toFloat();
    emit metersChanged();
}

} // namespace AetherSDR
