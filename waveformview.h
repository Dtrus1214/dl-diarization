#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QList>
#include <QFutureWatcher>
#include <QVector>
#include <QWidget>

struct SegmentResult;

class WaveformView : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformView(QWidget *parent = nullptr);
    ~WaveformView();

    bool loadAudio(const QString &audioPath);
    void setSegments(const QList<SegmentResult> &segments);
    void clearData();
    void zoomIn();
    void zoomOut();
    void resetZoom();

signals:
    void segmentClicked(int index);
    void cursorSelected(double sec);
    void loadFinished(bool ok, const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct LoadResult {
        bool ok;
        QString error;
        QVector<float> peaks;
        int sampleRate;
        qint64 totalFrames;
    };

    int segmentAtX(int x) const;
    double timeAtX(int x) const;
    double durationSec() const;
    double viewStartNorm() const;
    double viewSpanNorm() const;
    void zoomByFactor(double scale, int anchorX);
    static LoadResult loadPeaksFromWav(const QString &audioPath);
    void onLoadTaskFinished();

    QVector<float> m_peaks;
    QList<SegmentResult> m_segments;
    int m_sampleRate;
    qint64 m_totalFrames;
    double m_zoomFactor;
    double m_centerNorm;
    double m_playbackSec;
    bool m_playbackActive;
    double m_selectedCursorSec;
    bool m_isPanning;
    bool m_panMoved;
    int m_panStartX;
    double m_panStartCenterNorm;
    bool m_loading;
    QFutureWatcher<LoadResult> m_loadWatcher;

public:
    void setPlaybackPositionSec(double sec);
    void setPlaybackActive(bool active);
};

#endif // WAVEFORMVIEW_H
