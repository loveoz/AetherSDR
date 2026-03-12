#pragma once

#include <QObject>
#include <QAudioSink>
#include <QAudioSource>
#include <QAudioFormat>
#include <QIODevice>
#include <QUdpSocket>
#include <QByteArray>

namespace AetherSDR {

// SmartSDR audio streams arrive as VITA-49 UDP datagrams.
// Each datagram contains a 28-byte VITA-49 header followed by
// PCM samples (16-bit signed, stereo, 24000 Hz by default).
//
// AudioEngine:
//  1. Listens on a UDP port for the radio's audio stream.
//  2. Strips the VITA-49 header and feeds raw PCM to Qt audio output.
//  3. Optionally captures mic audio and sends it back (TX path).

class AudioEngine : public QObject {
    Q_OBJECT

public:
    static constexpr int DEFAULT_SAMPLE_RATE = 24000;
    static constexpr int VITA49_HEADER_BYTES = 28;

    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    // Start receiving audio on the given local UDP port.
    // The radio must have been told this port via:
    //   "audio stream create ip=<local_ip> port=<port>"
    bool startRxStream(quint16 localPort);
    void stopRxStream();

    // TX (microphone) – send raw PCM to radio's audio stream endpoint
    bool startTxStream(const QHostAddress& radioAddress, quint16 radioPort);
    void stopTxStream();

    float rxVolume() const  { return m_rxVolume; }
    void  setRxVolume(float v);

    bool isMuted() const   { return m_muted; }
    void setMuted(bool m);

signals:
    void rxStarted();
    void rxStopped();
    void levelChanged(float rms);  // audio level for VU meter, 0.0–1.0

private slots:
    void onRxDatagramReady();
    void onTxAudioReady();

private:
    QAudioFormat makeFormat() const;
    void processVita49Datagram(const QByteArray& datagram);
    float computeRMS(const QByteArray& pcm) const;

    // RX
    QUdpSocket    m_rxSocket;
    QAudioSink*   m_audioSink{nullptr};
    QIODevice*    m_audioDevice{nullptr};   // raw device from QAudioSink

    // TX
    QUdpSocket    m_txSocket;
    QAudioSource* m_audioSource{nullptr};
    QIODevice*    m_micDevice{nullptr};
    QHostAddress  m_txAddress;
    quint16       m_txPort{0};

    float m_rxVolume{1.0f};
    bool  m_muted{false};
};

} // namespace AetherSDR
