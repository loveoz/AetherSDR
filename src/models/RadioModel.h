#pragma once

#include "core/RadioConnection.h"
#include "SliceModel.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>

namespace AetherSDR {

// RadioModel is the central data model for a connected radio.
// It owns the RadioConnection, processes incoming status messages,
// and exposes the radio's current state to the GUI via Qt properties/signals.
class RadioModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString name        READ name        NOTIFY infoChanged)
    Q_PROPERTY(QString model       READ model       NOTIFY infoChanged)
    Q_PROPERTY(QString version     READ version     NOTIFY infoChanged)
    Q_PROPERTY(bool    connected   READ isConnected NOTIFY connectionStateChanged)
    Q_PROPERTY(float   paTemp      READ paTemp      NOTIFY metersChanged)
    Q_PROPERTY(float   txPower     READ txPower     NOTIFY metersChanged)

public:
    explicit RadioModel(QObject* parent = nullptr);

    // Access the underlying connection
    RadioConnection* connection() { return &m_connection; }

    // Getters
    QString name()    const { return m_name; }
    QString model()   const { return m_model; }
    QString version() const { return m_version; }
    bool isConnected() const;
    float paTemp()    const { return m_paTemp; }
    float txPower()   const { return m_txPower; }

    QList<SliceModel*> slices() const { return m_slices; }
    SliceModel* slice(int id) const;

    // High-level actions
    void connectToRadio(const RadioInfo& info);
    void disconnectFromRadio();
    void setTransmit(bool tx);

signals:
    void infoChanged();
    void connectionStateChanged(bool connected);
    void sliceAdded(SliceModel* slice);
    void sliceRemoved(int sliceId);
    void metersChanged();
    void connectionError(const QString& msg);

private slots:
    void onStatusReceived(const QString& object, const QMap<QString, QString>& kvs);
    void onConnected();
    void onDisconnected();
    void onConnectionError(const QString& msg);
    void onVersionReceived(const QString& version);

private:
    void handleRadioStatus(const QMap<QString, QString>& kvs);
    void handleSliceStatus(int id, const QMap<QString, QString>& kvs, bool removed);
    void handleMeterStatus(const QMap<QString, QString>& kvs);

    RadioConnection m_connection;

    QString m_name;
    QString m_model;
    QString m_version;
    float   m_paTemp{0.0f};
    float   m_txPower{0.0f};

    QList<SliceModel*> m_slices;
};

} // namespace AetherSDR
