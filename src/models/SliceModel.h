#pragma once

#include <QObject>
#include <QString>
#include <QMap>

namespace AetherSDR {

// A "slice" in SmartSDR terminology is an independent receive channel.
// Each slice has its own frequency, mode, filter, and audio settings.
class SliceModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(int    sliceId    READ sliceId)
    Q_PROPERTY(double frequency  READ frequency  WRITE setFrequency  NOTIFY frequencyChanged)
    Q_PROPERTY(QString mode      READ mode       WRITE setMode       NOTIFY modeChanged)
    Q_PROPERTY(int filterLow     READ filterLow  NOTIFY filterChanged)
    Q_PROPERTY(int filterHigh    READ filterHigh NOTIFY filterChanged)
    Q_PROPERTY(bool active       READ isActive   NOTIFY activeChanged)
    Q_PROPERTY(bool txSlice      READ isTxSlice  NOTIFY txSliceChanged)

public:
    explicit SliceModel(int id, QObject* parent = nullptr);

    // Getters
    int     sliceId()    const { return m_id; }
    double  frequency()  const { return m_frequency; }   // MHz
    QString mode()       const { return m_mode; }
    int     filterLow()  const { return m_filterLow; }   // Hz offset
    int     filterHigh() const { return m_filterHigh; }
    bool    isActive()   const { return m_active; }
    bool    isTxSlice()  const { return m_txSlice; }
    float   rfGain()     const { return m_rfGain; }
    float   audioGain()  const { return m_audioGain; }

    // Setters (emit signals AND queue radio commands via pendingCommands)
    void setFrequency(double mhz);
    void setMode(const QString& mode);
    void setFilterWidth(int low, int high);
    void setAudioGain(float gain);
    void setRfGain(float gain);

    // Apply a batch of KV pairs from a status message.
    void applyStatus(const QMap<QString, QString>& kvs);

    // Drain pending outgoing commands (called by RadioModel to send them)
    QStringList drainPendingCommands();

signals:
    void frequencyChanged(double mhz);
    void modeChanged(const QString& mode);
    void filterChanged(int low, int high);
    void activeChanged(bool active);
    void txSliceChanged(bool tx);
    void commandReady(const QString& cmd);  // ready to send to radio

private:
    int     m_id{0};
    double  m_frequency{0.0};      // MHz (0 = unset; first status update always fires frequencyChanged)
    QString m_mode{"USB"};
    int     m_filterLow{-1500};
    int     m_filterHigh{1500};
    bool    m_active{false};
    bool    m_txSlice{false};
    float   m_rfGain{0.0f};
    float   m_audioGain{50.0f};

    void sendCommand(const QString& cmd);

    QStringList m_pendingCommands;
};

} // namespace AetherSDR
