#include "AudioEngine.h"

#include <QMediaDevices>
#include <QAudioDevice>
#include <QNetworkDatagram>
#include <QDebug>
#include <cmath>
#include <cstring>

namespace AetherSDR {

// ─── VITA-49 mini-header layout (28 bytes) ────────────────────────────────────
// Byte  0- 3: Packet header word (packet type, indicators, sequence, size)
// Byte  4- 7: Stream ID
// Byte  8-15: Class ID
// Byte 16-19: Integer timestamp (seconds)
// Byte 20-27: Fractional timestamp
// Byte 28+  : Payload (PCM audio)

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
    connect(&m_rxSocket, &QUdpSocket::readyRead,
            this, &AudioEngine::onRxDatagramReady);
}

AudioEngine::~AudioEngine()
{
    stopRxStream();
    stopTxStream();
}

QAudioFormat AudioEngine::makeFormat() const
{
    QAudioFormat fmt;
    fmt.setSampleRate(DEFAULT_SAMPLE_RATE);
    fmt.setChannelCount(2);                        // stereo
    fmt.setSampleFormat(QAudioFormat::Int16);
    return fmt;
}

// ─── RX stream ───────────────────────────────────────────────────────────────

bool AudioEngine::startRxStream(quint16 localPort)
{
    if (!m_rxSocket.bind(QHostAddress::AnyIPv4, localPort)) {
        qWarning() << "AudioEngine: failed to bind RX port" << localPort
                   << m_rxSocket.errorString();
        return false;
    }

    const QAudioFormat fmt = makeFormat();
    const QAudioDevice defaultOutput = QMediaDevices::defaultAudioOutput();

    if (!defaultOutput.isFormatSupported(fmt)) {
        qWarning() << "AudioEngine: default output device does not support 24kHz stereo Int16";
        // In production, attempt format conversion or find a compatible device.
    }

    m_audioSink   = new QAudioSink(defaultOutput, fmt, this);
    m_audioSink->setVolume(m_rxVolume);
    m_audioDevice = m_audioSink->start();   // returns a raw QIODevice for push-mode

    if (!m_audioDevice) {
        qWarning() << "AudioEngine: failed to open audio sink";
        delete m_audioSink;
        m_audioSink = nullptr;
        return false;
    }

    qDebug() << "AudioEngine: RX stream started on port" << localPort;
    emit rxStarted();
    return true;
}

void AudioEngine::stopRxStream()
{
    m_rxSocket.close();
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink  = nullptr;
        m_audioDevice = nullptr;
    }
    emit rxStopped();
}

void AudioEngine::setRxVolume(float v)
{
    m_rxVolume = qBound(0.0f, v, 1.0f);
    if (m_audioSink)
        m_audioSink->setVolume(m_rxVolume);
}

void AudioEngine::setMuted(bool muted)
{
    m_muted = muted;
    setRxVolume(muted ? 0.0f : m_rxVolume);
}

void AudioEngine::onRxDatagramReady()
{
    while (m_rxSocket.hasPendingDatagrams()) {
        QNetworkDatagram dg = m_rxSocket.receiveDatagram();
        if (!dg.isNull())
            processVita49Datagram(dg.data());
    }
}

void AudioEngine::processVita49Datagram(const QByteArray& datagram)
{
    if (datagram.size() <= VITA49_HEADER_BYTES) return;

    // Strip the VITA-49 header, leaving raw PCM payload.
    const QByteArray pcm = datagram.mid(VITA49_HEADER_BYTES);

    if (m_audioDevice && m_audioDevice->isOpen()) {
        m_audioDevice->write(pcm);
    }

    // Emit RMS for VU meter (throttled in a real app)
    emit levelChanged(computeRMS(pcm));
}

float AudioEngine::computeRMS(const QByteArray& pcm) const
{
    const int samples = pcm.size() / 2;  // 16-bit samples
    if (samples == 0) return 0.0f;

    const int16_t* data = reinterpret_cast<const int16_t*>(pcm.constData());
    double sum = 0.0;
    for (int i = 0; i < samples; ++i) {
        const double s = data[i] / 32768.0;
        sum += s * s;
    }
    return static_cast<float>(std::sqrt(sum / samples));
}

// ─── TX stream ────────────────────────────────────────────────────────────────

bool AudioEngine::startTxStream(const QHostAddress& radioAddress, quint16 radioPort)
{
    m_txAddress = radioAddress;
    m_txPort    = radioPort;

    const QAudioFormat fmt = makeFormat();
    const QAudioDevice defaultInput = QMediaDevices::defaultAudioInput();

    m_audioSource = new QAudioSource(defaultInput, fmt, this);
    m_micDevice   = m_audioSource->start();

    if (!m_micDevice) {
        qWarning() << "AudioEngine: failed to open audio source";
        delete m_audioSource;
        m_audioSource = nullptr;
        return false;
    }

    // Connect mic data-ready to TX sender
    connect(m_micDevice, &QIODevice::readyRead,
            this, &AudioEngine::onTxAudioReady);

    qDebug() << "AudioEngine: TX stream started → " << radioAddress << ":" << radioPort;
    return true;
}

void AudioEngine::stopTxStream()
{
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_micDevice   = nullptr;
    }
    m_txSocket.close();
}

void AudioEngine::onTxAudioReady()
{
    if (!m_micDevice) return;

    const QByteArray pcm = m_micDevice->readAll();
    if (pcm.isEmpty()) return;

    // Prepend a minimal VITA-49 header before sending to the radio.
    // Full VITA-49 framing is required by the FlexRadio firmware.
    // Here we build a simplified header; a production implementation
    // should compute the correct packet type word and timestamps.
    QByteArray header(VITA49_HEADER_BYTES, '\0');

    // Packet type: 0x18 = IF Data with stream ID, C=1 (class ID present)
    // For a real implementation, consult VITA-49 spec §6.
    header[0] = 0x18;

    QByteArray datagram = header + pcm;
    m_txSocket.writeDatagram(datagram, m_txAddress, m_txPort);
}

} // namespace AetherSDR
