#pragma once

#include <QWidget>
#include <QVector>

namespace AetherSDR {

// Panadapter / spectrum display widget.
//
// Renders an FFT waterfall-style spectrum view.
// In this initial prototype, feed data via updateSpectrum().
//
// Next steps (see README):
//  - Subscribe to panadapter stream (UDP VITA-49 on a separate port).
//  - Parse 16-bit FFT bins from each datagram.
//  - Call updateSpectrum() on each frame.
//  - Add waterfall history (scrolling 2D texture).
//  - Add frequency overlay, slice markers, and VFO lines.
class SpectrumWidget : public QWidget {
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget* parent = nullptr);

    QSize sizeHint() const override { return {800, 200}; }

    // Set the frequency range covered by this panadapter
    void setFrequencyRange(double centerMhz, double bandwidthMhz);

    // Feed a new FFT frame. bins are dBm values (typically -140 to 0).
    void updateSpectrum(const QVector<float>& binsDbm);

    // Overlay: show a vertical line for the active slice at freqMhz
    void setSliceFrequency(double freqMhz);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawGrid(QPainter& p);
    void drawSpectrum(QPainter& p);
    void drawSliceMarker(QPainter& p);

    QVector<float> m_bins;       // current FFT frame (dBm)
    QVector<float> m_smoothed;   // exponential-smoothed for visual stability

    double m_centerMhz{14.225};
    double m_bandwidthMhz{0.192};  // 192 kHz default
    double m_sliceFreqMhz{14.225};

    float m_refLevel{0.0f};         // top of display (dBm)
    float m_dynamicRange{100.0f};   // dB range shown

    static constexpr float SMOOTH_ALPHA = 0.4f;
};

} // namespace AetherSDR
