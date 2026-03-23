#include "mainwindow.h"
#include "appconfig.h"
#include "diarizationengine.h"
#include "waveformview.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QMouseEvent>
#include <QKeySequence>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_windowSurface(nullptr)
    , m_rootLayout(nullptr)
    , m_statusLabel(nullptr)
    , m_versionLabel(nullptr)
    , m_titleBarLabel(nullptr)
    , m_inLayoutMenuBar(nullptr)
    , m_minimizeButton(nullptr)
    , m_maximizeButton(nullptr)
    , m_closeButton(nullptr)
    , m_importAction(nullptr)
    , m_runAction(nullptr)
    , m_exportAction(nullptr)
    , m_cutAction(nullptr)
    , m_copyAction(nullptr)
    , m_pasteAction(nullptr)
    , m_saveEditedAudioAction(nullptr)
    , m_aboutAction(nullptr)
    , m_sensitivitySlider(nullptr)
    , m_logsView(nullptr)
    , m_timelineLayout(nullptr)
    , m_timelineCard(nullptr)
    , m_waveformView(nullptr)
    , m_playPauseButton(nullptr)
    , m_waveCutButton(nullptr)
    , m_waveCopyButton(nullptr)
    , m_wavePasteButton(nullptr)
    , m_waveSaveButton(nullptr)
    , m_isDragging(false)
    , m_engine(new DiarizationEngine(this))
    , m_player(new QMediaPlayer(this))
    , m_segmentStopTimer(new QTimer(this))
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMinimumSize(1100, 700);
    setWindowTitle(QStringLiteral("%1 %2")
                   .arg(AppConfig::productName(), AppConfig::versionLabel()));

    createActions();
    buildUi();
    applyModernStyle();
    updateStatus(QStringLiteral("Ready. Import audio to begin."));

    connect(m_engine, &DiarizationEngine::logMessage, this, &MainWindow::onEngineLog);
    connect(m_engine, &DiarizationEngine::finished, this, &MainWindow::onEngineFinished);
    connect(m_engine, &DiarizationEngine::failed, this, &MainWindow::onEngineFailed);
    connect(m_engine, &DiarizationEngine::runningChanged, this, &MainWindow::onEngineRunningChanged);

    m_segmentStopTimer->setSingleShot(true);
    connect(m_segmentStopTimer, &QTimer::timeout, this, &MainWindow::stopSegmentPlayback);
    connect(m_player, &QMediaPlayer::positionChanged, this, &MainWindow::onPlayerPositionChanged);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &MainWindow::onPlayerPlaybackStateChanged);
#else
    connect(m_player, &QMediaPlayer::stateChanged, this, &MainWindow::onPlayerStateChanged);
#endif
}

MainWindow::~MainWindow()
{
}

void MainWindow::createActions()
{
    m_importAction = new QAction(QStringLiteral("&Import Audio..."), this);
    m_runAction = new QAction(QStringLiteral("&Run Diarization"), this);
    m_exportAction = new QAction(QStringLiteral("&Export Result..."), this);
    m_cutAction = new QAction(QStringLiteral("Cu&t"), this);
    m_copyAction = new QAction(QStringLiteral("&Copy"), this);
    m_pasteAction = new QAction(QStringLiteral("&Paste"), this);
    m_saveEditedAudioAction = new QAction(QStringLiteral("&Save Edited Audio..."), this);
    m_aboutAction = new QAction(QStringLiteral("&About"), this);

    m_cutAction->setShortcut(QKeySequence::Cut);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_saveEditedAudioAction->setShortcut(QKeySequence::Save);
    m_saveEditedAudioAction->setEnabled(false);

    connect(m_importAction, &QAction::triggered, this, &MainWindow::onImportAudio);
    connect(m_runAction, &QAction::triggered, this, &MainWindow::onRunDiarization);
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::onExportResult);
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::onWaveCut);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::onWaveCopy);
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::onWavePaste);
    connect(m_saveEditedAudioAction, &QAction::triggered, this, &MainWindow::onSaveEditedAudio);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onShowAbout);
}

void MainWindow::createMenus(QMenuBar *menuBar)
{
    if (!menuBar) {
        return;
    }

    QMenu *fileMenu = menuBar->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(m_importAction);
    fileMenu->addAction(m_exportAction);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), qApp, &QApplication::quit);

    QMenu *runMenu = menuBar->addMenu(QStringLiteral("&Run"));
    runMenu->addAction(m_runAction);

    QMenu *editMenu = menuBar->addMenu(QStringLiteral("&Edit"));
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addSeparator();
    editMenu->addAction(m_saveEditedAudioAction);

    QMenu *helpMenu = menuBar->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(m_aboutAction);
}

void MainWindow::buildUi()
{
    QWidget *outer = new QWidget(this);
    QVBoxLayout *outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(10, 10, 10, 10);
    outerLayout->setSpacing(0);

    m_windowSurface = new QWidget(outer);
    m_windowSurface->setObjectName(QStringLiteral("WindowSurface"));
    outerLayout->addWidget(m_windowSurface);

    m_centralWidget = m_windowSurface;
    m_rootLayout = new QVBoxLayout(m_windowSurface);
    m_rootLayout->setContentsMargins(24, 20, 24, 20);
    m_rootLayout->setSpacing(16);

    m_rootLayout->addWidget(buildTitleBar());
    m_rootLayout->addWidget(buildMainMenuBar());
    m_rootLayout->addWidget(buildHeader());
    m_rootLayout->addWidget(buildContentArea(), 1);
    m_rootLayout->addWidget(buildStatusStrip());

    setCentralWidget(outer);
    applyRoundedMask();
}

QWidget *MainWindow::buildMainMenuBar()
{
    QWidget *menuHost = new QWidget(m_centralWidget);
    menuHost->setObjectName(QStringLiteral("MenuHost"));

    QVBoxLayout *layout = new QVBoxLayout(menuHost);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_inLayoutMenuBar = new QMenuBar(menuHost);
    m_inLayoutMenuBar->setObjectName(QStringLiteral("InLayoutMenuBar"));
    createMenus(m_inLayoutMenuBar);
    layout->addWidget(m_inLayoutMenuBar);

    return menuHost;
}

QWidget *MainWindow::buildContentArea()
{
    QWidget *content = new QWidget(m_centralWidget);
    content->setObjectName(QStringLiteral("ContentArea"));

    QHBoxLayout *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(16);
    contentLayout->addWidget(buildControls(), 1);
    contentLayout->addWidget(buildResultPane(), 2);
    return content;
}

QWidget *MainWindow::buildTitleBar()
{
    QWidget *titleBar = new QWidget(m_centralWidget);
    titleBar->setObjectName(QStringLiteral("CustomTitleBar"));
    titleBar->installEventFilter(this);

    QHBoxLayout *layout = new QHBoxLayout(titleBar);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(6);

    m_titleBarLabel = new QLabel(
                QStringLiteral("%1 %2").arg(AppConfig::productName(), AppConfig::versionLabel()),
                titleBar);
    m_titleBarLabel->setObjectName(QStringLiteral("TitleBarLabel"));
    m_titleBarLabel->installEventFilter(this);

    m_minimizeButton = new QPushButton(QStringLiteral("-"), titleBar);
    m_maximizeButton = new QPushButton(QStringLiteral("[]"), titleBar);
    m_closeButton = new QPushButton(QStringLiteral("x"), titleBar);

    m_minimizeButton->setObjectName(QStringLiteral("TitleBarButton"));
    m_maximizeButton->setObjectName(QStringLiteral("TitleBarButton"));
    m_closeButton->setObjectName(QStringLiteral("TitleBarCloseButton"));

    connect(m_minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximizeButton, &QPushButton::clicked, this, &MainWindow::toggleMaximizeRestore);
    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::close);

    layout->addWidget(m_titleBarLabel);
    layout->addStretch();
    layout->addWidget(m_minimizeButton);
    layout->addWidget(m_maximizeButton);
    layout->addWidget(m_closeButton);

    return titleBar;
}

QWidget *MainWindow::buildHeader()
{
    QWidget *header = new QWidget(m_centralWidget);
    header->setObjectName(QStringLiteral("HeaderBar"));

    QHBoxLayout *layout = new QHBoxLayout(header);
    layout->setContentsMargins(18, 14, 18, 14);

    QLabel *title = new QLabel(AppConfig::productName(), header);
    title->setObjectName(QStringLiteral("AppTitle"));

    QLabel *subtitle = new QLabel(QStringLiteral("Speaker Diarization Workbench"), header);
    subtitle->setObjectName(QStringLiteral("AppSubtitle"));

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    titleLayout->addWidget(title);
    titleLayout->addWidget(subtitle);

    m_versionLabel = new QLabel(
                QStringLiteral("%1 (%2)")
                .arg(AppConfig::versionLabel(), AppConfig::buildLabel()),
                header);
    m_versionLabel->setObjectName(QStringLiteral("VersionChip"));

    layout->addLayout(titleLayout);
    layout->addStretch();
    layout->addWidget(m_versionLabel);
    return header;
}

QWidget *MainWindow::buildControls()
{
    QWidget *panel = new QWidget(m_centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QGroupBox *sourceCard = new QGroupBox(QStringLiteral("Input Source"), panel);
    QVBoxLayout *sourceLayout = new QVBoxLayout(sourceCard);
    QPushButton *importButton = new QPushButton(QStringLiteral("Import Audio"), sourceCard);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportAudio);
    sourceLayout->addWidget(new QLabel(QStringLiteral("WAV / FLAC / MP3 supported"), sourceCard));
    sourceLayout->addWidget(importButton);
    layout->addWidget(sourceCard);

    QGroupBox *configCard = new QGroupBox(QStringLiteral("Diarization Settings"), panel);
    QVBoxLayout *configLayout = new QVBoxLayout(configCard);
    configLayout->addWidget(new QLabel(QStringLiteral("Speaker Sensitivity"), configCard));
    m_sensitivitySlider = new QSlider(Qt::Horizontal, configCard);
    m_sensitivitySlider->setRange(0, 100);
    m_sensitivitySlider->setValue(60);
    configLayout->addWidget(m_sensitivitySlider);

    QPushButton *runButton = new QPushButton(QStringLiteral("Run Diarization"), configCard);
    runButton->setObjectName(QStringLiteral("PrimaryButton"));
    connect(runButton, &QPushButton::clicked, this, &MainWindow::onRunDiarization);
    configLayout->addWidget(runButton);
    layout->addWidget(configCard);

    layout->addWidget(createCard(QStringLiteral("Pipeline"),
                                 QStringLiteral("VAD -> Embeddings -> Clustering\n"
                                                "Runtime profile: Balanced")));
    layout->addStretch();

    return panel;
}

QWidget *MainWindow::buildResultPane()
{
    QWidget *panel = new QWidget(m_centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_timelineCard = new QGroupBox(QStringLiteral("Session Timeline"), panel);
    m_timelineLayout = new QVBoxLayout(m_timelineCard);
    QHBoxLayout *waveControls = new QHBoxLayout();
    QPushButton *zoomInBtn = new QPushButton(QStringLiteral("Zoom In +"), m_timelineCard);
    QPushButton *zoomOutBtn = new QPushButton(QStringLiteral("Zoom Out -"), m_timelineCard);
    QPushButton *zoomResetBtn = new QPushButton(QStringLiteral("Reset"), m_timelineCard);
    m_playPauseButton = new QPushButton(QStringLiteral("Play"), m_timelineCard);
    m_waveCutButton = new QPushButton(QStringLiteral("Cut"), m_timelineCard);
    m_waveCopyButton = new QPushButton(QStringLiteral("Copy"), m_timelineCard);
    m_wavePasteButton = new QPushButton(QStringLiteral("Paste"), m_timelineCard);
    m_waveSaveButton = new QPushButton(QStringLiteral("Save Edited"), m_timelineCard);
    m_waveSaveButton->setEnabled(false);
    waveControls->addStretch();
    waveControls->addWidget(m_waveCutButton);
    waveControls->addWidget(m_waveCopyButton);
    waveControls->addWidget(m_wavePasteButton);
    waveControls->addWidget(m_waveSaveButton);
    waveControls->addWidget(m_playPauseButton);
    waveControls->addWidget(zoomOutBtn);
    waveControls->addWidget(zoomInBtn);
    waveControls->addWidget(zoomResetBtn);
    m_timelineLayout->addLayout(waveControls);

    m_waveformView = new WaveformView(m_timelineCard);
    connect(m_waveformView, &WaveformView::segmentClicked, this, &MainWindow::onWaveformSegmentClicked);
    connect(m_waveformView, &WaveformView::cursorSelected, this, &MainWindow::onWaveformCursorSelected);
    connect(m_waveformView, &WaveformView::contextMenuRequested, this, &MainWindow::onWaveformContextMenuRequested);
    connect(m_waveformView, &WaveformView::loadFinished, this, &MainWindow::onWaveformLoadFinished);
    connect(m_waveformView, &WaveformView::editedChanged, this, &MainWindow::onWaveEditedChanged);
    connect(zoomInBtn, &QPushButton::clicked, m_waveformView, &WaveformView::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, m_waveformView, &WaveformView::zoomOut);
    connect(zoomResetBtn, &QPushButton::clicked, m_waveformView, &WaveformView::resetZoom);
    connect(m_playPauseButton, &QPushButton::clicked, this, &MainWindow::onToggleWaveformPlayback);
    connect(m_waveCutButton, &QPushButton::clicked, this, &MainWindow::onWaveCut);
    connect(m_waveCopyButton, &QPushButton::clicked, this, &MainWindow::onWaveCopy);
    connect(m_wavePasteButton, &QPushButton::clicked, this, &MainWindow::onWavePaste);
    connect(m_waveSaveButton, &QPushButton::clicked, this, &MainWindow::onSaveEditedAudio);
    m_timelineLayout->addWidget(m_waveformView);
    m_timelineLayout->addWidget(new QLabel(QStringLiteral("Click colored region to listen segment"), m_timelineCard));
    m_timelineLayout->addWidget(new QLabel(QStringLiteral("No diarization run yet."), m_timelineCard));
    layout->addWidget(m_timelineCard);

    QGroupBox *logsCard = new QGroupBox(QStringLiteral("Processing Logs"), panel);
    QVBoxLayout *logLayout = new QVBoxLayout(logsCard);
    m_logsView = new QPlainTextEdit(logsCard);
    m_logsView->setReadOnly(true);
    m_logsView->setPlainText(QStringLiteral("[init] ready\n[model] awaiting input file"));
    logLayout->addWidget(m_logsView);
    layout->addWidget(logsCard, 1);

    QHBoxLayout *actionsLayout = new QHBoxLayout();
    actionsLayout->addStretch();
    QPushButton *exportButton = new QPushButton(QStringLiteral("Export JSON"), panel);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExportResult);
    actionsLayout->addWidget(exportButton);
    layout->addLayout(actionsLayout);

    return panel;
}

QWidget *MainWindow::buildStatusStrip()
{
    QFrame *strip = new QFrame(m_centralWidget);
    strip->setObjectName(QStringLiteral("StatusStrip"));
    QHBoxLayout *layout = new QHBoxLayout(strip);
    layout->setContentsMargins(14, 10, 14, 10);

    m_statusLabel = new QLabel(QStringLiteral(""), strip);
    m_statusLabel->setObjectName(QStringLiteral("StatusText"));
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    QLabel *footnote = new QLabel(QStringLiteral("Qt Dynamic UI"), strip);
    footnote->setObjectName(QStringLiteral("StatusMuted"));
    layout->addWidget(footnote);
    return strip;
}

QWidget *MainWindow::createCard(const QString &title, const QString &bodyText)
{
    QGroupBox *card = new QGroupBox(title, m_centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(card);
    QLabel *body = new QLabel(bodyText, card);
    body->setWordWrap(true);
    layout->addWidget(body);
    return card;
}

void MainWindow::applyModernStyle()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: transparent; }"
        "#WindowSurface { background-color: #10131a; border: 1px solid #2a3346; border-radius: 16px; }"
        "#CustomTitleBar { background: #131826; border: 1px solid #2a3346; border-radius: 10px; }"
        "#MenuHost { background: transparent; }"
        "#InLayoutMenuBar { background: #161b26; color: #e6e8ef; border: 1px solid #2a3346; border-radius: 10px; padding: 5px; }"
        "#InLayoutMenuBar::item { background: transparent; padding: 6px 10px; border-radius: 6px; }"
        "#InLayoutMenuBar::item:selected { background: #2d3548; }"
        "#TitleBarLabel { color: #e6edff; font-size: 12px; font-weight: 600; }"
        "#TitleBarButton { background: #25314a; color: #dce6ff; border: none; border-radius: 6px;"
        "                  min-width: 28px; min-height: 22px; }"
        "#TitleBarButton:hover { background: #2f3f5d; }"
        "#TitleBarCloseButton { background: #6e2636; color: #ffe5eb; border: none; border-radius: 6px;"
        "                      min-width: 28px; min-height: 22px; }"
        "#TitleBarCloseButton:hover { background: #9b2d45; }"
        "QMenu { background: #191f2d; color: #e6e8ef; border: 1px solid #2a3346; }"
        "QMenu::item:selected { background: #2d3548; }"
        "#HeaderBar { background: #161b26; border: 1px solid #2a3346; border-radius: 12px; }"
        "#AppTitle { color: #eef2ff; font-size: 22px; font-weight: 700; }"
        "#AppSubtitle { color: #9ca9bf; font-size: 12px; }"
        "#VersionChip { background: #2d3548; color: #d7dff1; border-radius: 9px; padding: 6px 10px; }"
        "QGroupBox { background: #161b26; border: 1px solid #2a3346; border-radius: 10px;"
        "           margin-top: 10px; padding: 8px; color: #dfe5f5; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QLabel { color: #d4dceb; }"
        "QPlainTextEdit { background: #111723; color: #b8c4d9; border: 1px solid #2a3346;"
        "                 border-radius: 8px; selection-background-color: #3758a8; }"
        "WaveformView { border: 1px solid #2a3346; border-radius: 8px; background: #111723; }"
        "QPushButton { background: #25314a; color: #e8eefc; border: none; border-radius: 8px;"
        "              padding: 8px 14px; }"
        "QPushButton:hover { background: #2c3d5d; }"
        "QPushButton:pressed { background: #1f2b40; }"
        "#PrimaryButton { background: #3a67ff; color: white; font-weight: 600; }"
        "#PrimaryButton:hover { background: #5880ff; }"
        "QSlider::groove:horizontal { height: 6px; background: #2a3346; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 14px; margin: -4px 0; border-radius: 7px;"
        "                             background: #6f94ff; }"
        "#StatusStrip { background: #161b26; border: 1px solid #2a3346; border-radius: 10px; }"
        "#StatusText { color: #cfd7e6; }"
        "#StatusMuted { color: #8e9ab0; }"
    ));
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    const bool draggable = (watched && watched->objectName() == QStringLiteral("CustomTitleBar"))
            || (watched && watched->objectName() == QStringLiteral("TitleBarLabel"));
    if (!draggable) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        toggleMaximizeRestore();
        return true;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_isDragging = true;
            m_dragOffset = mouseEvent->globalPos() - frameGeometry().topLeft();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        if (!m_isDragging || isMaximized()) {
            return false;
        }
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        move(mouseEvent->globalPos() - m_dragOffset);
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease) {
        m_isDragging = false;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    applyRoundedMask();
}

void MainWindow::applyRoundedMask()
{
    if (isMaximized()) {
        clearMask();
        return;
    }

    QRegion rounded(rect(), QRegion::Rectangle);
    const int cornerRadius = 16;
    const int diameter = cornerRadius * 2;

    rounded -= QRegion(0, 0, cornerRadius, cornerRadius);
    rounded -= QRegion(width() - cornerRadius, 0, cornerRadius, cornerRadius);
    rounded -= QRegion(0, height() - cornerRadius, cornerRadius, cornerRadius);
    rounded -= QRegion(width() - cornerRadius, height() - cornerRadius, cornerRadius, cornerRadius);

    rounded += QRegion(0, 0, diameter, diameter, QRegion::Ellipse);
    rounded += QRegion(width() - diameter, 0, diameter, diameter, QRegion::Ellipse);
    rounded += QRegion(0, height() - diameter, diameter, diameter, QRegion::Ellipse);
    rounded += QRegion(width() - diameter, height() - diameter, diameter, diameter, QRegion::Ellipse);
    setMask(rounded);
}

void MainWindow::toggleMaximizeRestore()
{
    if (isMaximized()) {
        showNormal();
        if (m_maximizeButton) {
            m_maximizeButton->setText(QStringLiteral("[]"));
        }
    } else {
        showMaximized();
        if (m_maximizeButton) {
            m_maximizeButton->setText(QStringLiteral("o"));
        }
    }
    applyRoundedMask();
}

void MainWindow::updateStatus(const QString &text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }

    if (statusBar()) {
        statusBar()->showMessage(text, 5000);
    }
}

void MainWindow::onImportAudio()
{
    const QString selected = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("Select audio file"),
                QString(),
                QStringLiteral("Audio Files (*.wav *.flac *.mp3)"));

    if (selected.isEmpty()) {
        updateStatus(QStringLiteral("Audio import canceled."));
        return;
    }

    m_selectedAudioPath = selected;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_player->setSource(QUrl::fromLocalFile(m_selectedAudioPath));
#else
    m_player->setMedia(QUrl::fromLocalFile(m_selectedAudioPath));
#endif
    m_player->setPosition(0);
    if (m_waveformView && m_waveformView->loadAudio(selected)) {
        m_waveformView->setPlaybackPositionSec(0.0);
        m_waveformView->setPlaybackActive(false);
        appendLog(QStringLiteral("[wave] loading waveform..."));
    } else {
        appendLog(QStringLiteral("[wave] waveform loader is busy"));
    }
    appendLog(QStringLiteral("[input] %1").arg(selected));
    updateStatus(QStringLiteral("Loaded audio: %1").arg(selected));
}

void MainWindow::onRunDiarization()
{
    if (m_selectedAudioPath.isEmpty()) {
        updateStatus(QStringLiteral("Please import an audio file first."));
        return;
    }

    appendLog(QStringLiteral("[run] diarization started"));
    updateStatus(QStringLiteral("Diarization running..."));
    m_engine->run(m_selectedAudioPath, m_sensitivitySlider ? m_sensitivitySlider->value() : 60);
}

void MainWindow::onExportResult()
{
    const QString outPath = QFileDialog::getSaveFileName(
                this,
                QStringLiteral("Export diarization result"),
                QStringLiteral("diarization_result.json"),
                QStringLiteral("JSON (*.json)"));

    if (outPath.isEmpty()) {
        updateStatus(QStringLiteral("Export canceled."));
        return;
    }

    if (m_lastRawJson.isEmpty()) {
        updateStatus(QStringLiteral("No diarization result available yet."));
        return;
    }

    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        updateStatus(QStringLiteral("Failed to write export file."));
        return;
    }

    QTextStream stream(&outFile);
    stream << m_lastRawJson;
    outFile.close();

    appendLog(QStringLiteral("[export] %1").arg(outPath));
    updateStatus(QStringLiteral("Result exported to: %1").arg(outPath));
}

void MainWindow::onShowAbout()
{
    QMessageBox::about(this,
                       QStringLiteral("About %1").arg(AppConfig::productName()),
                       QStringLiteral("%1\nVersion: %2\nBuild: %3\n\n"
                                      "Modern Qt Widgets UI built dynamically in C++.")
                       .arg(AppConfig::productName(),
                            AppConfig::version(),
                            AppConfig::buildLabel()));
}

void MainWindow::onEngineLog(const QString &message)
{
    appendLog(QStringLiteral("[engine] %1").arg(message));
}

void MainWindow::onEngineFinished(const QList<SegmentResult> &segments, const QString &rawJson)
{
    m_lastRawJson = rawJson;
    m_lastSegments = segments;
    refreshTimeline(segments);
    appendLog(QStringLiteral("[run] completed: %1 segments").arg(segments.size()));
    updateStatus(QStringLiteral("Diarization completed successfully."));
}

void MainWindow::onEngineFailed(const QString &error)
{
    appendLog(QStringLiteral("[error] %1").arg(error));
    updateStatus(QStringLiteral("Diarization failed."));
}

void MainWindow::onEngineRunningChanged(bool running)
{
    if (m_runAction) {
        m_runAction->setEnabled(!running);
    }
    if (m_importAction) {
        m_importAction->setEnabled(!running);
    }
}

void MainWindow::refreshTimeline(const QList<SegmentResult> &segments)
{
    if (!m_timelineLayout || !m_timelineCard) {
        return;
    }

    if (m_waveformView) {
        m_waveformView->setSegments(segments);
    }

    while (m_timelineLayout->count() > 3) {
        QLayoutItem *item = m_timelineLayout->takeAt(3);
        if (item) {
            delete item->widget();
            delete item;
        }
    }

    if (segments.isEmpty()) {
        m_timelineLayout->addWidget(new QLabel(QStringLiteral("No segments returned."), m_timelineCard));
        return;
    }

    for (int i = 0; i < segments.size(); ++i) {
        const SegmentResult &segment = segments.at(i);
        const QString line = QStringLiteral("%1 - %2  |  %3")
                .arg(formatTime(segment.startSec),
                     formatTime(segment.endSec),
                     segment.speaker);
        QLabel *lbl = new QLabel(QStringLiteral("[%1] %2").arg(i + 1).arg(line), m_timelineCard);
        m_timelineLayout->addWidget(lbl);
    }
    m_timelineLayout->addStretch();
}

void MainWindow::appendLog(const QString &line)
{
    if (!m_logsView) {
        return;
    }
    m_logsView->appendPlainText(line);
}

QString MainWindow::formatTime(double sec) const
{
    const int total = static_cast<int>(sec);
    const int mm = total / 60;
    const int ss = total % 60;
    return QStringLiteral("%1:%2")
            .arg(mm, 2, 10, QLatin1Char('0'))
            .arg(ss, 2, 10, QLatin1Char('0'));
}

void MainWindow::onWaveformSegmentClicked(int index)
{
    if (index < 0 || index >= m_lastSegments.size()) {
        return;
    }
    if (m_selectedAudioPath.isEmpty()) {
        updateStatus(QStringLiteral("Import audio first."));
        return;
    }

    const SegmentResult &segment = m_lastSegments.at(index);
    const qint64 startMs = static_cast<qint64>(segment.startSec * 1000.0);
    const qint64 durationMs = qMax<qint64>(200, static_cast<qint64>((segment.endSec - segment.startSec) * 1000.0));

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_player->setSource(QUrl::fromLocalFile(m_selectedAudioPath));
#else
    m_player->setMedia(QUrl::fromLocalFile(m_selectedAudioPath));
#endif
    m_player->setPosition(startMs);
    m_player->play();
    if (m_playPauseButton) {
        m_playPauseButton->setText(QStringLiteral("Pause"));
    }
    m_segmentStopTimer->start(static_cast<int>(durationMs));

    appendLog(QStringLiteral("[play] segment %1 (%2 - %3)")
              .arg(index + 1)
              .arg(formatTime(segment.startSec))
              .arg(formatTime(segment.endSec)));
    updateStatus(QStringLiteral("Playing segment %1").arg(index + 1));
}

void MainWindow::stopSegmentPlayback()
{
    if (m_player) {
        m_player->stop();
    }
    if (m_playPauseButton) {
        m_playPauseButton->setText(QStringLiteral("Play"));
    }
    if (m_waveformView) {
        m_waveformView->setPlaybackActive(false);
    }
}

void MainWindow::onWaveformCursorSelected(double sec)
{
    if (m_selectedAudioPath.isEmpty()) {
        return;
    }

    m_segmentStopTimer->stop();
    m_player->setPosition(static_cast<qint64>(sec * 1000.0));
    if (m_waveformView) {
        m_waveformView->setPlaybackPositionSec(sec);
    }
    updateStatus(QStringLiteral("Cursor set to %1").arg(formatTime(sec)));
}

void MainWindow::onWaveformContextMenuRequested(const QPoint &globalPos)
{
    if (!m_waveformView) {
        return;
    }

    if (m_cutAction) {
        m_cutAction->setEnabled(m_waveformView->hasSelection());
    }
    if (m_copyAction) {
        m_copyAction->setEnabled(m_waveformView->hasSelection());
    }
    if (m_saveEditedAudioAction) {
        m_saveEditedAudioAction->setEnabled(m_waveformView->hasEdits());
    }

    QMenu menu(this);
    if (m_cutAction) {
        menu.addAction(m_cutAction);
    }
    if (m_copyAction) {
        menu.addAction(m_copyAction);
    }
    if (m_pasteAction) {
        menu.addAction(m_pasteAction);
    }
    menu.addSeparator();
    if (m_saveEditedAudioAction) {
        menu.addAction(m_saveEditedAudioAction);
    }
    menu.exec(globalPos);
}

void MainWindow::onWaveformLoadFinished(bool ok, const QString &message)
{
    if (ok) {
        appendLog(QStringLiteral("[wave] waveform loaded"));
        return;
    }

    appendLog(QStringLiteral("[wave] failed to load waveform: %1").arg(message));
}

void MainWindow::onWaveCut()
{
    if (!m_waveformView || !m_waveformView->cutSelection()) {
        updateStatus(QStringLiteral("Cut failed. Select a region first."));
        return;
    }
    appendLog(QStringLiteral("[edit] cut selection"));
    updateStatus(QStringLiteral("Selection cut."));
}

void MainWindow::onWaveCopy()
{
    if (!m_waveformView || !m_waveformView->copySelection()) {
        updateStatus(QStringLiteral("Copy failed. Select a region first."));
        return;
    }
    appendLog(QStringLiteral("[edit] copied selection"));
    updateStatus(QStringLiteral("Selection copied."));
}

void MainWindow::onWavePaste()
{
    if (!m_waveformView || !m_waveformView->pasteAtCursor()) {
        updateStatus(QStringLiteral("Paste failed. Copy a selection first."));
        return;
    }
    appendLog(QStringLiteral("[edit] pasted at cursor"));
    updateStatus(QStringLiteral("Audio pasted at cursor."));
}

void MainWindow::onSaveEditedAudio()
{
    if (!m_waveformView || !m_waveformView->hasEdits()) {
        updateStatus(QStringLiteral("No unsaved edits."));
        return;
    }

    const QString savePath = QFileDialog::getSaveFileName(
                this,
                QStringLiteral("Save edited audio"),
                QStringLiteral("edited_audio.wav"),
                QStringLiteral("WAV (*.wav)"));
    if (savePath.isEmpty()) {
        return;
    }

    QString error;
    if (!m_waveformView->saveToFile(savePath, &error)) {
        updateStatus(QStringLiteral("Save failed: %1").arg(error));
        return;
    }

    appendLog(QStringLiteral("[edit] saved: %1").arg(savePath));
    updateStatus(QStringLiteral("Edited audio saved."));
}

void MainWindow::onWaveEditedChanged(bool dirty)
{
    if (m_waveSaveButton) {
        m_waveSaveButton->setEnabled(dirty);
    }
    if (m_saveEditedAudioAction) {
        m_saveEditedAudioAction->setEnabled(dirty);
    }
}

void MainWindow::onToggleWaveformPlayback()
{
    if (m_selectedAudioPath.isEmpty()) {
        updateStatus(QStringLiteral("Import audio first."));
        return;
    }
    if (m_waveformView && m_waveformView->hasEdits()) {
        updateStatus(QStringLiteral("You have unsaved edits. Save edited audio before playback."));
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const bool currentlyPlaying = (m_player->playbackState() == QMediaPlayer::PlayingState);
#else
    const bool currentlyPlaying = (m_player->state() == QMediaPlayer::PlayingState);
#endif
    if (currentlyPlaying) {
        m_player->pause();
        if (m_playPauseButton) {
            m_playPauseButton->setText(QStringLiteral("Play"));
        }
        updateStatus(QStringLiteral("Playback paused."));
        return;
    }

    m_segmentStopTimer->stop();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_player->source().isEmpty()) {
        m_player->setSource(QUrl::fromLocalFile(m_selectedAudioPath));
    }
#else
    if (m_player->media().isNull()) {
        m_player->setMedia(QUrl::fromLocalFile(m_selectedAudioPath));
    }
#endif
    m_player->play();
    if (m_playPauseButton) {
        m_playPauseButton->setText(QStringLiteral("Pause"));
    }
    updateStatus(QStringLiteral("Playback running."));
}

void MainWindow::onPlayerPositionChanged(qint64 positionMs)
{
    if (m_waveformView) {
        m_waveformView->setPlaybackPositionSec(static_cast<double>(positionMs) / 1000.0);
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void MainWindow::onPlayerPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    const bool playing = (state == QMediaPlayer::PlayingState);
    if (m_waveformView) {
        m_waveformView->setPlaybackActive(playing);
    }
    if (m_playPauseButton) {
        m_playPauseButton->setText(playing ? QStringLiteral("Pause") : QStringLiteral("Play"));
    }
}
#else
void MainWindow::onPlayerStateChanged(QMediaPlayer::State state)
{
    const bool playing = (state == QMediaPlayer::PlayingState);
    if (m_waveformView) {
        m_waveformView->setPlaybackActive(playing);
    }
    if (m_playPauseButton) {
        m_playPauseButton->setText(playing ? QStringLiteral("Pause") : QStringLiteral("Play"));
    }
}
#endif

