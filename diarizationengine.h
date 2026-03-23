#ifndef DIARIZATIONENGINE_H
#define DIARIZATIONENGINE_H

#include <QFutureWatcher>
#include <QObject>
#include <QString>

struct SegmentResult {
    double startSec;
    double endSec;
    QString speaker;
};

class DiarizationEngine : public QObject
{
    Q_OBJECT
public:
    explicit DiarizationEngine(QObject *parent = nullptr);

    bool isRunning() const;
    void run(const QString &audioPath, int sensitivity);
    void cancel();

signals:
    void logMessage(const QString &message);
    void finished(const QList<SegmentResult> &segments, const QString &rawJson);
    void failed(const QString &error);
    void runningChanged(bool running);

private slots:
    void onWorkerFinished();

private:
    struct WorkerOutput {
        bool ok;
        QString error;
        QList<SegmentResult> segments;
        QString rawJson;
        QStringList logs;
    };

    WorkerOutput runInternal(const QString &audioPath, int sensitivity) const;
    static QString buildRawJson(const QList<SegmentResult> &segments);

    QFutureWatcher<WorkerOutput> m_watcher;
    bool m_running;
};

#endif // DIARIZATIONENGINE_H
