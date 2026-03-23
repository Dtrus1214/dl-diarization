#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QList>
#include <QPoint>
#include <QString>

class QAction;
class DiarizationEngine;
class QLabel;
class QMediaPlayer;
class QMenuBar;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QTimer;
class QVBoxLayout;
class WaveformView;
class QWidget;
class QGroupBox;

struct SegmentResult;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onImportAudio();
    void onRunDiarization();
    void onExportResult();
    void onShowAbout();
    void onEngineLog(const QString &message);
    void onEngineFinished(const QList<SegmentResult> &segments, const QString &rawJson);
    void onEngineFailed(const QString &error);
    void onEngineRunningChanged(bool running);
    void onWaveformSegmentClicked(int index);
    void onWaveformCursorSelected(double sec);
    void onWaveformContextMenuRequested(const QPoint &globalPos);
    void onWaveformLoadFinished(bool ok, const QString &message);
    void onWaveCut();
    void onWaveCopy();
    void onWavePaste();
    void onSaveEditedAudio();
    void onWaveEditedChanged(bool dirty);
    void onToggleWaveformPlayback();
    void onPlayerPositionChanged(qint64 positionMs);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void onPlayerPlaybackStateChanged(QMediaPlayer::PlaybackState state);
#else
    void onPlayerStateChanged(QMediaPlayer::State state);
#endif
    void stopSegmentPlayback();

private:
    void buildUi();
    QWidget *buildHeader();
    QWidget *buildTitleBar();
    QWidget *buildMainMenuBar();
    QWidget *buildContentArea();
    QWidget *buildControls();
    QWidget *buildResultPane();
    QWidget *buildStatusStrip();
    QWidget *createCard(const QString &title, const QString &bodyText);
    void createActions();
    void createMenus(QMenuBar *menuBar);
    void applyModernStyle();
    void updateStatus(const QString &text);
    void refreshTimeline(const QList<SegmentResult> &segments);
    void appendLog(const QString &line);
    QString formatTime(double sec) const;
    void applyRoundedMask();
    void toggleMaximizeRestore();

    QWidget *m_centralWidget;
    QWidget *m_windowSurface;
    QVBoxLayout *m_rootLayout;

    QLabel *m_statusLabel;
    QLabel *m_versionLabel;
    QLabel *m_titleBarLabel;
    QMenuBar *m_inLayoutMenuBar;

    QPushButton *m_minimizeButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_closeButton;

    QAction *m_importAction;
    QAction *m_runAction;
    QAction *m_exportAction;
    QAction *m_cutAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_saveEditedAudioAction;
    QAction *m_aboutAction;

    QSlider *m_sensitivitySlider;
    QPlainTextEdit *m_logsView;
    QVBoxLayout *m_timelineLayout;
    QGroupBox *m_timelineCard;
    WaveformView *m_waveformView;
    QPushButton *m_playPauseButton;
    QPushButton *m_waveCutButton;
    QPushButton *m_waveCopyButton;
    QPushButton *m_wavePasteButton;
    QPushButton *m_waveSaveButton;

    QString m_selectedAudioPath;
    QString m_lastRawJson;
    QList<SegmentResult> m_lastSegments;
    bool m_isDragging;
    QPoint m_dragOffset;

    DiarizationEngine *m_engine;
    QMediaPlayer *m_player;
    QTimer *m_segmentStopTimer;
};

#endif // MAINWINDOW_H
