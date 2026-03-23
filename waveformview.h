#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QList>
#include <QVector>
#include <QWidget>

struct SegmentResult;

class WaveformView : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformView(QWidget *parent = nullptr);

    bool loadAudio(const QString &audioPath);
    void setSegments(const QList<SegmentResult> &segments);
    void clearData();
    void zoomIn();
    void zoomOut();
    void resetZoom();

signals:
    void segmentClicked(int index);
    void cursorSelected(double sec);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int segmentAtX(int x) const;
    double timeAtX(int x) const;
    double durationSec() const;
    double viewStartNorm() const;
    double viewSpanNorm() const;
    void zoomByFactor(double scale, int anchorX);

    QVector<float> m_peaks;
    QList<SegmentResult> m_segments;
    int m_sampleRate;
    int m_totalFrames;
    double m_zoomFactor;
    double m_centerNorm;
    double m_playbackSec;
    bool m_playbackActive;
    double m_selectedCursorSec;
    bool m_isPanning;
    bool m_panMoved;
    int m_panStartX;
    double m_panStartCenterNorm;

public:
    void setPlaybackPositionSec(double sec);
    void setPlaybackActive(bool active);
};

#endif // WAVEFORMVIEW_H
