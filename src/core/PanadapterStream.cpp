#include "PanadapterStream.h"
#include "RadioConnection.h"

#include <QNetworkDatagram>
#include <QHostAddress>
#include <QtEndian>
#include <QDebug>

namespace AetherSDR {

// ─── VITA-49 header layout (28 bytes, big-endian) ─────────────────────────────
// Word 0 (bytes  0- 3): Packet header (type, flags, packet count, size in words)
// Word 1 (bytes  4- 7): Stream ID
// Word 2 (bytes  8-11): Class ID OUI / info type (upper)
// Word 3 (bytes 12-15): Class ID (lower)
// Word 4 (bytes 16-19): Integer timestamp (seconds, UTC)
// Word 5 (bytes 20-23): Fractional timestamp (upper)
// Word 6 (bytes 24-27): Fractional timestamp (lower)
// Byte 28+            : Payload — int16_t FFT bins, big-endian
//
// Stream ID patterns (FLEX radio):
//   0x4xxxxxxx — panadapter FFT frames
//   0x42xxxxxx — waterfall pixel rows
//   0x0001xxxx — audio (remote audio)
//
// UDP delivery (SmartSDR v1.x LAN):
//   "client set udpport" is NOT supported on firmware v1.4.0.0 (returns 50001000).
//   Instead: bind port 4991 (the well-known VITA-49 receive port for local clients)
//   and send a one-byte UDP registration packet to the radio at port 4992 so the
//   radio learns our IP:port from the datagram source address.

PanadapterStream::PanadapterStream(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead,
            this, &PanadapterStream::onDatagramReady);
}

bool PanadapterStream::isRunning() const
{
    return m_socket.state() == QAbstractSocket::BoundState;
}

bool PanadapterStream::start(RadioConnection* conn)
{
    if (isRunning()) return true;

    // Bind to an OS-assigned UDP port.
    if (!m_socket.bind(QHostAddress::AnyIPv4, 0)) {
        qWarning() << "PanadapterStream: failed to bind UDP socket:"
                   << m_socket.errorString();
        return false;
    }

    m_localPort = m_socket.localPort();
    qDebug() << "PanadapterStream: bound to UDP port" << m_localPort;

    // Send a one-byte UDP registration packet to the radio's well-known port (4992).
    // The radio learns our IP:port from the datagram's source address and will
    // direct subsequent VITA-49 frames here.
    // "client set udpport" is not supported on firmware v1.4.0.0 (returns 50001000);
    // this packet is the only way to register on that firmware.
    const QHostAddress radioAddr = conn->radioAddress();
    const qint64 sent = m_socket.writeDatagram(QByteArray(1, '\x00'), radioAddr, 4992);
    qDebug() << "PanadapterStream: sent UDP registration to"
             << radioAddr.toString() << ":4992, bytes sent =" << sent;

    m_conn = conn;
    return true;
}

void PanadapterStream::stop()
{
    m_socket.close();
    m_localPort = 0;
}

// ─── Datagram reception ───────────────────────────────────────────────────────

void PanadapterStream::setDbmRange(float minDbm, float maxDbm)
{
    m_minDbm = minDbm;
    m_maxDbm = maxDbm;
    qDebug() << "PanadapterStream: dBm range set to" << minDbm << "->" << maxDbm;
}

void PanadapterStream::onDatagramReady()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_socket.receiveDatagram();
        if (!dg.isNull())
            processDatagram(dg.data());
    }
}

void PanadapterStream::processDatagram(const QByteArray& data)
{
    if (data.size() < VITA49_HEADER_BYTES + 2) return;

    const auto* raw = reinterpret_cast<const uchar*>(data.constData());

    // VITA-49 word 0 (bytes 0-3): packet header.
    // On FLEX radios, the top byte encodes the packet type:
    //   0x18... = IF Data (audio)
    //   0x38... = Extension Data (panadapter FFT)
    // Only process Extension Data (panadapter) frames.
    const quint32 word0    = qFromBigEndian<quint32>(raw);
    const quint32 streamId = qFromBigEndian<quint32>(raw + 4);

    static bool firstSeen = true;
    if (firstSeen) {
        firstSeen = false;
        qDebug() << "PanadapterStream: first datagram" << data.size()
                 << "bytes, word0=0x" + QString::number(word0, 16)
                 << "streamId=0x" + QString::number(streamId, 16);
    }

    // Skip IF-Data packets (top byte 0x18 = audio/IF streams).
    // Accept Extension Data packets (top byte 0x38 = panadapter FFT).
    if ((word0 >> 24) == 0x18u) return;

    // Payload: unsigned 16-bit FFT bins, big-endian.
    // The radio linearly maps power levels to the display dBm range:
    //   bin = 0x0000  → m_minDbm
    //   bin = 0xFFFF  → m_maxDbm
    const int payloadBytes = data.size() - VITA49_HEADER_BYTES;
    const int binCount     = payloadBytes / 2;
    if (binCount <= 0) return;

    const uchar* payload = raw + VITA49_HEADER_BYTES;
    const float  range   = m_maxDbm - m_minDbm;

    QVector<float> bins(binCount);
    for (int i = 0; i < binCount; ++i) {
        const quint16 sample = qFromBigEndian<quint16>(payload + i * 2);
        bins[i] = m_minDbm + (static_cast<float>(sample) / 65535.0f) * range;
    }

    emit spectrumReady(bins);
}

} // namespace AetherSDR
