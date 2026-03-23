#include "waveformview.h"
#include "diarizationengine.h"

#include <QByteArray>
#include <QFile>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace {

quint16 readU16LE(const QByteArray &data, int offset)
{
    return static_cast<quint16>(static_cast<unsigned char>(data[offset]) |
                                (static_cast<unsigned char>(data[offset + 1]) << 8));
}

quint32 readU32LE(const QByteArray &data, int offset)
{
    return static_cast<quint32>(static_cast<unsigned char>(data[offset]) |
                                (static_cast<unsigned char>(data[offset + 1]) << 8) |
                                (static_cast<unsigned char>(data[offset + 2]) << 16) |
                                (static_cast<unsigned char>(data[offset + 3]) << 24));
}

}

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
    , m_sampleRate(0)
    , m_totalFrames(0)
    , m_zoomFactor(1.0)
    , m_centerNorm(0.5)
    , m_playbackSec(0.0)
    , m_playbackActive(false)
    , m_selectedCursorSec(0.0)
    , m_isPanning(false)
    , m_panMoved(false)
    , m_panStartX(0)
    , m_panStartCenterNorm(0.5)
{
    setMinimumHeight(140);
}

bool WaveformView::loadAudio(const QString &audioPath)
{
    clearData();

    QFile file(audioPath);
    if (!file.open(QIODevice::ReadOnly)) {
        update();
        return false;
    }

    const QByteArray wav = file.readAll();
    file.close();

    if (wav.size() < 44 || !wav.startsWith("RIFF") || wav.mid(8, 4) != "WAVE") {
        update();
        return false;
    }

    const int fmtOffset = wav.indexOf("fmt ");
    const int dataTagOffset = wav.indexOf("data");
    if (fmtOffset < 0 || dataTagOffset < 0) {
        update();
        return false;
    }

    const quint32 fmtSize = readU32LE(wav, fmtOffset + 4);
    const int fmtDataOffset = fmtOffset + 8;
    if (fmtDataOffset + static_cast<int>(fmtSize) > wav.size()) {
        update();
        return false;
    }

    const quint16 audioFormat = readU16LE(wav, fmtDataOffset);
    const quint16 channels = readU16LE(wav, fmtDataOffset + 2);
    m_sampleRate = static_cast<int>(readU32LE(wav, fmtDataOffset + 4));
    const quint16 bitsPerSample = readU16LE(wav, fmtDataOffset + 14);
    if (audioFormat != 1 || bitsPerSample != 16 || channels == 0 || m_sampleRate <= 0) {
        update();
        return false;
    }

    const quint32 dataSize = readU32LE(wav, dataTagOffset + 4);
    const int dataStart = dataTagOffset + 8;
    if (dataStart + static_cast<int>(dataSize) > wav.size()) {
        update();
        return false;
    }

    const int bytesPerFrame = static_cast<int>(channels) * 2;
    m_totalFrames = static_cast<int>(dataSize) / bytesPerFrame;
    if (m_totalFrames <= 0) {
        update();
        return false;
    }

    const int peakCount = 1200;
    m_peaks.resize(peakCount);
    for (int i = 0; i < peakCount; ++i) {
        m_peaks[i] = 0.0f;
    }

    const int framesPerBucket = qMax(1, m_totalFrames / peakCount);
    for (int b = 0; b < peakCount; ++b) {
        const int startFrame = b * framesPerBucket;
        const int endFrame = qMin(m_totalFrames, startFrame + framesPerBucket);
        float maxAbs = 0.0f;

        for (int frame = startFrame; frame < endFrame; ++frame) {
            const int frameOffset = dataStart + (frame * bytesPerFrame);
            const qint16 sample = static_cast<qint16>(
                        static_cast<unsigned char>(wav[frameOffset]) |
                        (static_cast<unsigned char>(wav[frameOffset + 1]) << 8));
            const float normalized = qAbs(static_cast<float>(sample)) / 32768.0f;
            if (normalized > maxAbs) {
                maxAbs = normalized;
            }
        }
        m_peaks[b] = maxAbs;
    }

    update();
    return true;
}

void WaveformView::setSegments(const QList<SegmentResult> &segments)
{
    m_segments = segments;
    update();
}

void WaveformView::clearData()
{
    m_peaks.clear();
    m_segments.clear();
    m_sampleRate = 0;
    m_totalFrames = 0;
    m_zoomFactor = 1.0;
    m_centerNorm = 0.5;
    m_selectedCursorSec = 0.0;
    m_isPanning = false;
    m_panMoved = false;
    m_panStartX = 0;
    m_panStartCenterNorm = 0.5;
}

void WaveformView::zoomIn()
{
    zoomByFactor(1.35, width() / 2);
}

void WaveformView::zoomOut()
{
    zoomByFactor(1.0 / 1.35, width() / 2);
}

void WaveformView::resetZoom()
{
    m_zoomFactor = 1.0;
    m_centerNorm = 0.5;
    update();
}

void WaveformView::setPlaybackPositionSec(double sec)
{
    m_playbackSec = qMax(0.0, sec);
    m_selectedCursorSec = m_playbackSec;
    update();
}

void WaveformView::setPlaybackActive(bool active)
{
    m_playbackActive = active;
    update();
}

void WaveformView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#111723"));

    if (m_peaks.isEmpty()) {
        painter.setPen(QColor("#7f8ca4"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Waveform preview (WAV 16-bit PCM)"));
        return;
    }

    const QRect area = rect().adjusted(8, 8, -8, -8);
    painter.setPen(Qt::NoPen);

    const double duration = durationSec();
    const double startNorm = viewStartNorm();
    const double spanNorm = viewSpanNorm();
    const double endNorm = startNorm + spanNorm;

    if (duration > 0.0 && !m_segments.isEmpty()) {
        for (int i = 0; i < m_segments.size(); ++i) {
            const SegmentResult &s = m_segments.at(i);
            const double segStartNorm = s.startSec / duration;
            const double segEndNorm = s.endSec / duration;
            if (segEndNorm < startNorm || segStartNorm > endNorm) {
                continue;
            }

            const double clampedStart = qMax(startNorm, segStartNorm);
            const double clampedEnd = qMin(endNorm, segEndNorm);
            const double xStartNorm = (clampedStart - startNorm) / spanNorm;
            const double xEndNorm = (clampedEnd - startNorm) / spanNorm;
            const int x1 = area.left() + static_cast<int>(xStartNorm * area.width());
            const int x2 = area.left() + static_cast<int>(xEndNorm * area.width());
            QColor c = (i % 2 == 0) ? QColor(QStringLiteral("#2d4677")) : QColor(QStringLiteral("#2f6a4f"));
            c.setAlpha(110);
            painter.fillRect(QRect(x1, area.top(), qMax(2, x2 - x1), area.height()), c);
        }
    }

    painter.setPen(QColor("#6f94ff"));
    const int midY = area.center().y();
    const int points = m_peaks.size();
    const int startIndex = qBound(0, static_cast<int>(startNorm * points), qMax(0, points - 1));
    const int endIndex = qBound(startIndex + 1, static_cast<int>(endNorm * points), points);
    const int visibleCount = qMax(1, endIndex - startIndex);

    for (int i = 0; i < visibleCount; ++i) {
        const int sampleIndex = startIndex + i;
        const int x = area.left() + (i * area.width()) / qMax(1, visibleCount - 1);
        const int halfHeight = static_cast<int>(m_peaks.at(sampleIndex) * (area.height() / 2.0));
        painter.drawLine(x, midY - halfHeight, x, midY + halfHeight);
    }

    if (duration > 0.0 && m_playbackActive) {
        const double playNorm = qBound(0.0, m_playbackSec / duration, 1.0);
        if (playNorm >= startNorm && playNorm <= endNorm) {
            const double local = (playNorm - startNorm) / spanNorm;
            const int x = area.left() + static_cast<int>(local * area.width());
            painter.setPen(QPen(QColor("#ffd166"), 2));
            painter.drawLine(x, area.top(), x, area.bottom());
        }
    }

    if (duration > 0.0) {
        const double selectedNorm = qBound(0.0, m_selectedCursorSec / duration, 1.0);
        if (selectedNorm >= startNorm && selectedNorm <= endNorm) {
            const double local = (selectedNorm - startNorm) / spanNorm;
            const int x = area.left() + static_cast<int>(local * area.width());
            painter.setPen(QPen(QColor("#66d9ef"), 1, Qt::DashLine));
            painter.drawLine(x, area.top(), x, area.bottom());
        }
    }
}

void WaveformView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isPanning = (m_zoomFactor > 1.0);
        m_panMoved = false;
        m_panStartX = event->x();
        m_panStartCenterNorm = m_centerNorm;
    }
    QWidget::mousePressEvent(event);
}

void WaveformView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_isPanning) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QRect area = rect().adjusted(8, 8, -8, -8);
    if (area.width() <= 0) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int dx = event->x() - m_panStartX;
    if (qAbs(dx) > 2) {
        m_panMoved = true;
    }

    const double normDelta = (static_cast<double>(dx) / static_cast<double>(area.width())) * viewSpanNorm();
    m_centerNorm = qBound(0.0, m_panStartCenterNorm - normDelta, 1.0);
    update();
    QWidget::mouseMoveEvent(event);
}

void WaveformView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const bool treatAsClick = !m_panMoved;
    m_isPanning = false;

    if (treatAsClick) {
        const double sec = timeAtX(event->x());
        if (sec >= 0.0) {
            m_selectedCursorSec = sec;
            emit cursorSelected(sec);

            const int idx = segmentAtX(event->x());
            if (idx >= 0) {
                emit segmentClicked(idx);
            }
            update();
        }
    }

    QWidget::mouseReleaseEvent(event);
}

void WaveformView::wheelEvent(QWheelEvent *event)
{
    const int deltaY = event->angleDelta().y();
    if (deltaY == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    const double factor = (deltaY > 0) ? 1.20 : (1.0 / 1.20);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const int anchorX = static_cast<int>(event->position().x());
#else
    const int anchorX = event->x();
#endif
    zoomByFactor(factor, anchorX);
    event->accept();
}

int WaveformView::segmentAtX(int x) const
{
    const double duration = durationSec();
    if (duration <= 0.0 || m_segments.isEmpty()) {
        return -1;
    }

    const QRect area = rect().adjusted(8, 8, -8, -8);
    if (!area.contains(x, area.center().y())) {
        return -1;
    }

    const double t = timeAtX(x);
    if (t < 0.0) {
        return -1;
    }
    for (int i = 0; i < m_segments.size(); ++i) {
        const SegmentResult &s = m_segments.at(i);
        if (t >= s.startSec && t <= s.endSec) {
            return i;
        }
    }
    return -1;
}

double WaveformView::timeAtX(int x) const
{
    const double duration = durationSec();
    if (duration <= 0.0) {
        return -1.0;
    }

    const QRect area = rect().adjusted(8, 8, -8, -8);
    if (!area.contains(x, area.center().y())) {
        return -1.0;
    }

    const double startNorm = viewStartNorm();
    const double spanNorm = viewSpanNorm();
    const double localNorm = static_cast<double>(x - area.left()) / static_cast<double>(qMax(1, area.width()));
    const double t = (startNorm + localNorm * spanNorm) * duration;
    return qBound(0.0, t, duration);
}

double WaveformView::durationSec() const
{
    if (m_sampleRate <= 0 || m_totalFrames <= 0) {
        return 0.0;
    }
    return static_cast<double>(m_totalFrames) / static_cast<double>(m_sampleRate);
}

double WaveformView::viewSpanNorm() const
{
    return 1.0 / qMax(1.0, m_zoomFactor);
}

double WaveformView::viewStartNorm() const
{
    const double span = viewSpanNorm();
    const double half = span / 2.0;
    return qBound(0.0, m_centerNorm - half, 1.0 - span);
}

void WaveformView::zoomByFactor(double scale, int anchorX)
{
    const QRect area = rect().adjusted(8, 8, -8, -8);
    if (area.width() <= 0) {
        return;
    }

    const double oldStart = viewStartNorm();
    const double oldSpan = viewSpanNorm();
    const double localNorm = qBound(0.0,
                                    static_cast<double>(anchorX - area.left()) / static_cast<double>(area.width()),
                                    1.0);
    const double anchorAbsoluteNorm = oldStart + localNorm * oldSpan;

    const double newZoom = qBound(1.0, m_zoomFactor * scale, 16.0);
    const double newSpan = 1.0 / newZoom;
    const double newStart = qBound(0.0, anchorAbsoluteNorm - (localNorm * newSpan), 1.0 - newSpan);

    m_zoomFactor = newZoom;
    m_centerNorm = newStart + (newSpan / 2.0);
    update();
}
