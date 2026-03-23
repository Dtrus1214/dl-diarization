#include "diarizationengine.h"

#include <QtConcurrent>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QtMath>

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

double absMean(const QByteArray &pcm, int byteOffset, int bytesPerFrame, int frames, int channelCount)
{
    if (frames <= 0 || bytesPerFrame <= 0 || channelCount <= 0) {
        return 0.0;
    }

    qint64 sum = 0;
    const int totalSamples = frames * channelCount;

    for (int i = 0; i < frames; ++i) {
        const int frameOffset = byteOffset + (i * bytesPerFrame);
        for (int c = 0; c < channelCount; ++c) {
            const int sampleOffset = frameOffset + (c * 2);
            const qint16 sample = static_cast<qint16>(
                        static_cast<unsigned char>(pcm[sampleOffset]) |
                        (static_cast<unsigned char>(pcm[sampleOffset + 1]) << 8));
            sum += qAbs(sample);
        }
    }

    return static_cast<double>(sum) / static_cast<double>(totalSamples) / 32768.0;
}

QString speakerLabelForEnergy(double energy, double threshold)
{
    return energy >= threshold ? QStringLiteral("SPEAKER_00") : QStringLiteral("SPEAKER_01");
}

}

DiarizationEngine::DiarizationEngine(QObject *parent)
    : QObject(parent)
    , m_running(false)
{
    connect(&m_watcher, &QFutureWatcher<WorkerOutput>::finished,
            this, &DiarizationEngine::onWorkerFinished);
}

bool DiarizationEngine::isRunning() const
{
    return m_running;
}

void DiarizationEngine::run(const QString &audioPath, int sensitivity)
{
    if (m_running) {
        emit failed(QStringLiteral("Diarization is already running."));
        return;
    }

    const QFileInfo audioInfo(audioPath);
    if (!audioInfo.exists() || !audioInfo.isFile()) {
        emit failed(QStringLiteral("Audio file not found: %1").arg(audioPath));
        return;
    }

    m_running = true;
    emit runningChanged(true);
    emit logMessage(QStringLiteral("Running native diarization (standalone C++ mode)."));
    m_watcher.setFuture(QtConcurrent::run([this, audioPath, sensitivity]() {
        return runInternal(audioPath, sensitivity);
    }));
}

void DiarizationEngine::cancel()
{
    if (!m_running) {
        return;
    }
    emit logMessage(QStringLiteral("Cancel requested. Native worker cannot be interrupted immediately."));
}

void DiarizationEngine::onWorkerFinished()
{
    const WorkerOutput result = m_watcher.result();

    for (const QString &line : result.logs) {
        if (!line.isEmpty()) {
            emit logMessage(line);
        }
    }

    m_running = false;
    emit runningChanged(false);

    if (!result.ok) {
        emit failed(result.error);
        return;
    }

    emit finished(result.segments, result.rawJson);
}

DiarizationEngine::WorkerOutput DiarizationEngine::runInternal(const QString &audioPath, int sensitivity) const
{
    WorkerOutput output;
    output.ok = false;

    QFile file(audioPath);
    if (!file.open(QIODevice::ReadOnly)) {
        output.error = QStringLiteral("Failed to open audio file.");
        return output;
    }

    const QByteArray wav = file.readAll();
    file.close();

    if (wav.size() < 44 || !wav.startsWith("RIFF") || wav.mid(8, 4) != "WAVE") {
        output.error = QStringLiteral("Only PCM WAV files are supported in standalone mode.");
        return output;
    }

    int fmtOffset = wav.indexOf("fmt ");
    int dataOffsetTag = wav.indexOf("data");
    if (fmtOffset < 0 || dataOffsetTag < 0) {
        output.error = QStringLiteral("Invalid WAV structure.");
        return output;
    }

    const int fmtSizeOffset = fmtOffset + 4;
    const quint32 fmtSize = readU32LE(wav, fmtSizeOffset);
    const int fmtDataOffset = fmtOffset + 8;
    if (fmtDataOffset + static_cast<int>(fmtSize) > wav.size()) {
        output.error = QStringLiteral("Corrupt WAV fmt chunk.");
        return output;
    }

    const quint16 audioFormat = readU16LE(wav, fmtDataOffset);
    const quint16 channelCount = readU16LE(wav, fmtDataOffset + 2);
    const quint32 sampleRate = readU32LE(wav, fmtDataOffset + 4);
    const quint16 bitsPerSample = readU16LE(wav, fmtDataOffset + 14);

    if (audioFormat != 1 || bitsPerSample != 16) {
        output.error = QStringLiteral("Standalone mode supports 16-bit PCM WAV only.");
        return output;
    }

    const quint32 dataSize = readU32LE(wav, dataOffsetTag + 4);
    const int dataStart = dataOffsetTag + 8;
    if (dataStart + static_cast<int>(dataSize) > wav.size()) {
        output.error = QStringLiteral("Corrupt WAV data chunk.");
        return output;
    }

    const int bytesPerFrame = static_cast<int>(channelCount) * 2;
    if (bytesPerFrame <= 0 || sampleRate == 0) {
        output.error = QStringLiteral("Invalid WAV header values.");
        return output;
    }

    const int totalFrames = static_cast<int>(dataSize) / bytesPerFrame;
    if (totalFrames <= 0) {
        output.error = QStringLiteral("Audio has no samples.");
        return output;
    }

    const int windowMs = 500;
    const int windowFrames = qMax(1, static_cast<int>((sampleRate * windowMs) / 1000));
    const int windowBytes = windowFrames * bytesPerFrame;
    const int windowCount = qMax(1, totalFrames / windowFrames);
    const double speakerThreshold = 0.08 + (static_cast<double>(sensitivity) / 100.0) * 0.2;
    const double vadThreshold = 0.015;

    output.logs << QStringLiteral("Mode: standalone native diarization")
                << QStringLiteral("Sample rate: %1 Hz, channels: %2").arg(sampleRate).arg(channelCount)
                << QStringLiteral("Sensitivity: %1").arg(sensitivity);

    QList<SegmentResult> segments;
    bool inSpeech = false;
    SegmentResult currentSegment{};

    for (int w = 0; w < windowCount; ++w) {
        const int offset = dataStart + (w * windowBytes);
        const int remainingBytes = (dataStart + static_cast<int>(dataSize)) - offset;
        if (remainingBytes <= 0) {
            break;
        }

        const int bytesThisWindow = qMin(windowBytes, remainingBytes);
        const int framesThisWindow = bytesThisWindow / bytesPerFrame;
        if (framesThisWindow <= 0) {
            continue;
        }

        const double energy = absMean(wav, offset, bytesPerFrame, framesThisWindow, channelCount);
        const double start = static_cast<double>(w * windowFrames) / static_cast<double>(sampleRate);
        const double end = static_cast<double>((w * windowFrames) + framesThisWindow) / static_cast<double>(sampleRate);

        if (energy < vadThreshold) {
            if (inSpeech) {
                currentSegment.endSec = end;
                segments.append(currentSegment);
                inSpeech = false;
            }
            continue;
        }

        const QString speaker = speakerLabelForEnergy(energy, speakerThreshold);
        if (!inSpeech) {
            inSpeech = true;
            currentSegment.startSec = start;
            currentSegment.endSec = end;
            currentSegment.speaker = speaker;
        } else if (currentSegment.speaker == speaker) {
            currentSegment.endSec = end;
        } else {
            segments.append(currentSegment);
            currentSegment.startSec = start;
            currentSegment.endSec = end;
            currentSegment.speaker = speaker;
        }
    }

    if (inSpeech) {
        segments.append(currentSegment);
    }

    if (segments.isEmpty()) {
        segments.append({0.0, qMin(5.0, static_cast<double>(totalFrames) / static_cast<double>(sampleRate)),
                        QStringLiteral("SPEAKER_00")});
        output.logs << QStringLiteral("No speech windows detected; emitted fallback segment.");
    }

    output.ok = true;
    output.segments = segments;
    output.rawJson = buildRawJson(segments);
    output.logs << QStringLiteral("Completed with %1 segments.").arg(segments.size());
    return output;
}

QString DiarizationEngine::buildRawJson(const QList<SegmentResult> &segments)
{
    QJsonArray segArray;
    for (const SegmentResult &s : segments) {
        QJsonObject item;
        item.insert(QStringLiteral("start"), s.startSec);
        item.insert(QStringLiteral("end"), s.endSec);
        item.insert(QStringLiteral("speaker"), s.speaker);
        segArray.append(item);
    }

    QJsonObject root;
    root.insert(QStringLiteral("backend"), QStringLiteral("native-cpp-wav-v1"));
    root.insert(QStringLiteral("segments"), segArray);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}
