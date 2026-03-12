#include "FrequencyDial.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFontDatabase>
#include <QInputDialog>
#include <cmath>

namespace AetherSDR {

// ─── Layout constants ─────────────────────────────────────────────────────────
// We display: XXX.XXX.XXX  (9 digits, 2 separator dots)
//             [0][1][2].[3][4][5].[6][7][8]
// Column 0 = 100 MHz, col 1 = 10 MHz, col 2 = 1 MHz
// Col 3 = 100 kHz, col 4 = 10 kHz, col 5 = 1 kHz
// Col 6 = 100 Hz,  col 7 = 10 Hz,  col 8 = 1 Hz
// Separator dots appear after col 2 and after col 5.

static const double PLACE_VALUES_MHZ[9] = {
    100.0, 10.0, 1.0,
    0.1, 0.01, 0.001,
    0.0001, 0.00001, 0.000001
};

// Local copies of the layout constants for use in free functions.
static constexpr int kDigitWidth = 28;
static constexpr int kDotWidth   = 12;

FrequencyDial::FrequencyDial(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::WheelFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true);
}

QSize FrequencyDial::sizeHint() const
{
    // 9 digit columns + 2 separator dots + margins
    return { NUM_DIGITS * DIGIT_WIDTH + 2 * DOT_WIDTH + 16, DIGIT_HEIGHT + 16 };
}

QSize FrequencyDial::minimumSizeHint() const { return sizeHint(); }

// ─── Geometry helpers ─────────────────────────────────────────────────────────

// Returns the X offset of column col's left edge (0-based, in widget coords).
static int columnX(int col)
{
    // Insert a dot after col 2 (at +DOT_WIDTH) and after col 5 (at +DOT_WIDTH).
    int x = 8;   // left margin
    for (int i = 0; i < col; ++i) {
        x += kDigitWidth;
        if (i == 2 || i == 5) x += kDotWidth;
    }
    return x;
}

int FrequencyDial::columnAtX(int px) const
{
    for (int col = 0; col < NUM_DIGITS; ++col) {
        int x = columnX(col);
        if (px >= x && px < x + DIGIT_WIDTH)
            return col;
    }
    return -1;
}

double FrequencyDial::placeValueMhz(int col) const
{
    if (col < 0 || col >= NUM_DIGITS) return 0.0;
    return PLACE_VALUES_MHZ[col];
}

// ─── Frequency access ─────────────────────────────────────────────────────────

long long FrequencyDial::frequencyHz() const
{
    return static_cast<long long>(std::round(m_frequency * 1e6));
}

void FrequencyDial::setFrequencyHz(long long hz)
{
    hz = qBound(static_cast<long long>(MIN_FREQ * 1e6),
                hz,
                static_cast<long long>(MAX_FREQ * 1e6));
    const double mhz = hz / 1e6;
    if (qFuzzyCompare(mhz, m_frequency)) return;
    m_frequency = mhz;
    update();
    emit frequencyChanged(m_frequency);
}

void FrequencyDial::setFrequency(double mhz)
{
    setFrequencyHz(static_cast<long long>(std::round(mhz * 1e6)));
}

// ─── Paint ────────────────────────────────────────────────────────────────────

void FrequencyDial::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), QColor(0x1a, 0x1a, 0x2e));

    // Rounded border
    p.setPen(QPen(QColor(0x00, 0xb4, 0xd8), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect().adjusted(1,1,-1,-1), 6, 6);

    // Frequency as a 9-digit string (no dots)
    const long long hz = frequencyHz();
    const QString digits = QString("%1").arg(hz, 9, 10, QChar('0'));

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(DIGIT_HEIGHT - 8);
    font.setBold(true);
    p.setFont(font);

    const int top = 8;

    for (int col = 0; col < NUM_DIGITS; ++col) {
        const int x = columnX(col);

        // Highlight active column
        if (col == m_activeColumn) {
            p.fillRect(x, top, DIGIT_WIDTH, DIGIT_HEIGHT,
                       QColor(0x00, 0xb4, 0xd8, 60));
        }

        // Dimmer color for leading zeros
        const bool leading = (hz == 0) || (col < 6 &&
            digits.left(col + 1).toLongLong() == 0 && col < 2);

        p.setPen(leading ? QColor(0x40, 0x60, 0x70) : QColor(0x00, 0xe5, 0xff));

        const QRect digitRect(x, top, DIGIT_WIDTH, DIGIT_HEIGHT);
        p.drawText(digitRect, Qt::AlignCenter, QString(digits[col]));

        // Separator dot after col 2 and col 5
        if (col == 2 || col == 5) {
            p.setPen(QColor(0x00, 0xb4, 0xd8));
            const QRect dotRect(x + DIGIT_WIDTH, top, DOT_WIDTH, DIGIT_HEIGHT);
            p.drawText(dotRect, Qt::AlignCenter | Qt::AlignBottom, ".");
        }
    }

    // Unit label
    p.setPen(QColor(0x60, 0x80, 0x90));
    QFont small = font;
    small.setPixelSize(11);
    p.setFont(small);
    p.drawText(rect().adjusted(0, 0, -4, -2), Qt::AlignRight | Qt::AlignBottom, "MHz");
}

// ─── Mouse & wheel events ─────────────────────────────────────────────────────

void FrequencyDial::mousePressEvent(QMouseEvent* ev)
{
    m_activeColumn = columnAtX(ev->pos().x());
    setFocus();
    update();
}

void FrequencyDial::mouseDoubleClickEvent(QMouseEvent*)
{
    startEditing();
}

void FrequencyDial::wheelEvent(QWheelEvent* ev)
{
    if (m_activeColumn < 0) return;

    const int steps = ev->angleDelta().y() / 120;
    if (steps == 0) return;

    const double delta = placeValueMhz(m_activeColumn) * steps;
    setFrequency(m_frequency + delta);
    ev->accept();
}

void FrequencyDial::keyPressEvent(QKeyEvent* ev)
{
    if (m_editing) {
        if (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter) {
            commitEditing();
        } else if (ev->key() == Qt::Key_Escape) {
            m_editing = false;
            m_editBuffer.clear();
            update();
        } else if (ev->key() == Qt::Key_Backspace) {
            m_editBuffer.chop(1);
            update();
        } else {
            const QString text = ev->text();
            if (!text.isEmpty() && (text[0].isDigit() || text[0] == '.'))
                m_editBuffer += text;
            update();
        }
        return;
    }

    if (m_activeColumn < 0) return;
    const double place = placeValueMhz(m_activeColumn);

    switch (ev->key()) {
    case Qt::Key_Up:   setFrequency(m_frequency + place); break;
    case Qt::Key_Down: setFrequency(m_frequency - place); break;
    case Qt::Key_Left:
        m_activeColumn = qMax(0, m_activeColumn - 1);
        update();
        break;
    case Qt::Key_Right:
        m_activeColumn = qMin(NUM_DIGITS - 1, m_activeColumn + 1);
        update();
        break;
    default:
        QWidget::keyPressEvent(ev);
    }
}

void FrequencyDial::focusOutEvent(QFocusEvent* ev)
{
    if (m_editing) commitEditing();
    m_activeColumn = -1;
    update();
    QWidget::focusOutEvent(ev);
}

// ─── Direct-entry editing ─────────────────────────────────────────────────────

void FrequencyDial::startEditing()
{
    m_editing    = true;
    m_editBuffer = QString::number(m_frequency, 'f', 6);
    update();
}

void FrequencyDial::commitEditing()
{
    bool ok = false;
    const double mhz = m_editBuffer.toDouble(&ok);
    if (ok && mhz >= MIN_FREQ && mhz <= MAX_FREQ)
        setFrequency(mhz);
    m_editing    = false;
    m_editBuffer.clear();
    update();
}

} // namespace AetherSDR
