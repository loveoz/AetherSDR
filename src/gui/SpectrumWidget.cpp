#include "SpectrumWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <cmath>

namespace AetherSDR {

SpectrumWidget::SpectrumWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // Dark background — avoids flicker on repaint
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void SpectrumWidget::setFrequencyRange(double centerMhz, double bandwidthMhz)
{
    m_centerMhz    = centerMhz;
    m_bandwidthMhz = bandwidthMhz;
    update();
}

void SpectrumWidget::setSliceFrequency(double freqMhz)
{
    m_sliceFreqMhz = freqMhz;
    update();
}

void SpectrumWidget::updateSpectrum(const QVector<float>& binsDbm)
{
    if (m_smoothed.size() != binsDbm.size())
        m_smoothed = binsDbm;
    else {
        for (int i = 0; i < binsDbm.size(); ++i)
            m_smoothed[i] = SMOOTH_ALPHA * binsDbm[i] + (1.0f - SMOOTH_ALPHA) * m_smoothed[i];
    }
    m_bins = binsDbm;
    update();
}

void SpectrumWidget::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
}

// ─── Paint ────────────────────────────────────────────────────────────────────

void SpectrumWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Background
    p.fillRect(rect(), QColor(0x0a, 0x0a, 0x14));

    drawGrid(p);
    drawSpectrum(p);
    drawSliceMarker(p);
}

void SpectrumWidget::drawGrid(QPainter& p)
{
    const int w = width();
    const int h = height();

    p.setPen(QPen(QColor(0x20, 0x30, 0x40), 1, Qt::DotLine));

    // Horizontal dB lines every 20 dB
    const int steps = static_cast<int>(m_dynamicRange / 20.0f);
    for (int i = 0; i <= steps; ++i) {
        const int y = static_cast<int>(h * i / static_cast<float>(steps));
        p.drawLine(0, y, w, y);

        const float dbm = m_refLevel - (m_dynamicRange * i / steps);
        p.setPen(QColor(0x40, 0x60, 0x70));
        p.drawText(2, y - 2, QString("%1 dBm").arg(static_cast<int>(dbm)));
        p.setPen(QPen(QColor(0x20, 0x30, 0x40), 1, Qt::DotLine));
    }

    // Vertical frequency lines every ~50 kHz
    const double startMhz = m_centerMhz - m_bandwidthMhz / 2.0;
    const double stepMhz  = 0.050;   // 50 kHz
    const double firstLine = std::ceil(startMhz / stepMhz) * stepMhz;
    const double endMhz    = m_centerMhz + m_bandwidthMhz / 2.0;

    p.setPen(QPen(QColor(0x20, 0x30, 0x40), 1, Qt::DotLine));
    for (double f = firstLine; f <= endMhz; f += stepMhz) {
        const int x = static_cast<int>((f - startMhz) / m_bandwidthMhz * w);
        p.drawLine(x, 0, x, h);

        p.setPen(QColor(0x40, 0x60, 0x70));
        const int khz = static_cast<int>(std::round((f - m_centerMhz) * 1000.0));
        p.drawText(x + 2, h - 4, QString("%1k").arg(khz));
        p.setPen(QPen(QColor(0x20, 0x30, 0x40), 1, Qt::DotLine));
    }
}

void SpectrumWidget::drawSpectrum(QPainter& p)
{
    if (m_smoothed.isEmpty()) {
        // No data yet — draw a noise floor placeholder
        p.setPen(QColor(0x00, 0x60, 0x80));
        p.drawText(rect(), Qt::AlignCenter, "No panadapter data\n(subscribe to panadapter stream)");
        return;
    }

    const int w = width();
    const int h = height();
    const int n = m_smoothed.size();

    QPainterPath path;
    bool first = true;

    for (int i = 0; i < n; ++i) {
        const float dbm = m_smoothed[i];
        const float norm = qBound(0.0f,
            (m_refLevel - dbm) / m_dynamicRange,
            1.0f);

        const int x = static_cast<int>(static_cast<float>(i) / n * w);
        const int y = static_cast<int>(norm * h);

        if (first) { path.moveTo(x, y); first = false; }
        else        path.lineTo(x, y);
    }

    // Filled area under the spectrum line
    path.lineTo(w, h);
    path.lineTo(0, h);
    path.closeSubpath();

    QLinearGradient grad(0, 0, 0, h);
    grad.setColorAt(0.0, QColor(0x00, 0xe5, 0xff, 200));
    grad.setColorAt(1.0, QColor(0x00, 0x40, 0x60, 60));

    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillPath(path, grad);
    p.setPen(QPen(QColor(0x00, 0xe5, 0xff), 1.5));
    p.drawPath(path);
}

void SpectrumWidget::drawSliceMarker(QPainter& p)
{
    const double startMhz = m_centerMhz - m_bandwidthMhz / 2.0;
    if (m_sliceFreqMhz < startMhz ||
        m_sliceFreqMhz > m_centerMhz + m_bandwidthMhz / 2.0)
        return;

    const int x = static_cast<int>(
        (m_sliceFreqMhz - startMhz) / m_bandwidthMhz * width());

    p.setPen(QPen(QColor(0xff, 0xa0, 0x00, 200), 1.5, Qt::DashLine));
    p.drawLine(x, 0, x, height());

    // Triangle marker at top
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xff, 0xa0, 0x00));
    QPolygon tri;
    tri << QPoint(x - 6, 0) << QPoint(x + 6, 0) << QPoint(x, 10);
    p.drawPolygon(tri);
}

} // namespace AetherSDR
