#ifndef SPEEDTEST_H
#define SPEEDTEST_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

class QNetworkReply;

class SpeedTest : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString server READ server NOTIFY changed)
    Q_PROPERTY(QString ip READ ip NOTIFY changed)
    Q_PROPERTY(QString proto READ proto NOTIFY changed)
    Q_PROPERTY(QString asn READ asn NOTIFY changed)
    Q_PROPERTY(QString network READ network NOTIFY changed)
    Q_PROPERTY(QString downText READ downText NOTIFY changed)
    Q_PROPERTY(QString upText READ upText NOTIFY changed)
    Q_PROPERTY(QString pingText READ pingText NOTIFY changed)
    Q_PROPERTY(QString pingRange READ pingRange NOTIFY changed)
    Q_PROPERTY(QString jitterText READ jitterText NOTIFY changed)
    Q_PROPERTY(QString jitterRange READ jitterRange NOTIFY changed)
    Q_PROPERTY(QString aimStreaming READ aimStreaming NOTIFY changed)
    Q_PROPERTY(QString aimGaming READ aimGaming NOTIFY changed)
    Q_PROPERTY(QString aimRtc READ aimRtc NOTIFY changed)
    Q_PROPERTY(QString idleLatencyStats READ idleLatencyStats NOTIFY changed)
    Q_PROPERTY(QString downLatencyStats READ downLatencyStats NOTIFY changed)
    Q_PROPERTY(QString upLatencyStats READ upLatencyStats NOTIFY changed)
    Q_PROPERTY(QVariantList downGroups READ downGroups NOTIFY changed)
    Q_PROPERTY(QVariantList upGroups READ upGroups NOTIFY changed)
    Q_PROPERTY(QString copyHint READ copyHint NOTIFY changed)

public:
    explicit SpeedTest(QObject *parent = 0);

    bool busy() const { return m_busy; }
    QString phase() const { return m_phase; }
    QString error() const { return m_error; }
    QString server() const { return m_server; }
    QString ip() const { return m_ip; }
    QString proto() const { return m_proto; }
    QString asn() const { return m_asn; }
    QString network() const { return m_network; }
    QString downText() const { return m_downText; }
    QString upText() const { return m_upText; }
    QString pingText() const { return m_pingText; }
    QString pingRange() const { return m_pingRange; }
    QString jitterText() const { return m_jitterText; }
    QString jitterRange() const { return m_jitterRange; }
    QString aimStreaming() const { return m_aimStreaming; }
    QString aimGaming() const { return m_aimGaming; }
    QString aimRtc() const { return m_aimRtc; }
    QString idleLatencyStats() const { return m_idleLatencyStats; }
    QString downLatencyStats() const { return m_downLatencyStats; }
    QString upLatencyStats() const { return m_upLatencyStats; }
    QVariantList downGroups() const { return m_downGroups; }
    QVariantList upGroups() const { return m_upGroups; }
    QString copyHint() const { return m_copyHint; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void copyResults();

signals:
    void changed();

private slots:
    void onFinished();
    void drain();
    void onLoadedPing();
    void onRipe();
    void onLoadTick();

private:
    enum Kind { Locate, Latency, Download, Upload };

    struct Job {
        Kind kind;
        qint64 bytes;
        int count;
        bool bypass;
        Job() : kind(Locate), bytes(0), count(1), bypass(false) {}
        Job(Kind k, qint64 b, int c, bool bp = false)
            : kind(k), bytes(b), count(c), bypass(bp) {}
    };

    struct Sample {
        qint64 bytes;
        qint64 ms;
        double mbps;
    };

    void buildJobs();
    void next();
    void fire();
    void getDown(qint64 bytes);
    void postUp(qint64 bytes);
    void fail(const QString &msg);
    void finish();
    void noteMeta(QNetworkReply *r);
    void lookupAsn();
    void startLoaded();
    void stopLoaded(qint64 bwMs);
    void addLoaded(QList<qint64> &dst, const QList<qint64> &more);
    void refreshHero();
    void refreshGroups();
    void refreshLatency();
    void refreshAim();
    QVariantList sizeGroups(Kind dir) const;
    static QByteArray hdr(QNetworkReply *r, const char *a, const char *b = 0);
    static QString sizeLabel(qint64 bytes);
    static QString fmtMbps(double mbps);
    static QString fmtSpeed(double mbps);
    static QString fmtMs(double ms);
    static double p90Mbps(const QList<Sample> &samples);
    static QString statsLine(const QList<double> &v, const QString &unit);
    static double percentile(QList<double> v, double p);
    static double meanAbsDelta(const QList<qint64> &v);
    static int aimScale(const double *domain, const int *range, int nDomain, double value);
    static QString aimLabel(int idx);

    QNetworkAccessManager m_nam;
    QNetworkAccessManager m_loadNam;
    QTimer m_loadTimer;
    QElapsedTimer m_timer;
    QNetworkReply *m_reply;
    QNetworkReply *m_loadReply;
    QNetworkReply *m_ripeReply;
    QList<Job> m_jobs;
    int m_jobI;
    int m_left;
    bool m_busy;
    bool m_downFinished;
    bool m_upFinished;
    bool m_loadingDown;
    bool m_keepLoad;
    QList<qint64> m_idleRtt;
    QList<qint64> m_downLoaded;
    QList<qint64> m_upLoaded;
    QList<qint64> m_pendingLoaded;
    QList<Sample> m_downSamples;
    QList<Sample> m_upSamples;
    QString m_phase;
    QString m_error;
    QString m_server;
    QString m_ip;
    QString m_proto;
    QString m_asn;
    QString m_network;
    QString m_downText;
    QString m_upText;
    QString m_pingText;
    QString m_pingRange;
    QString m_jitterText;
    QString m_jitterRange;
    QString m_aimStreaming;
    QString m_aimGaming;
    QString m_aimRtc;
    QString m_idleLatencyStats;
    QString m_downLatencyStats;
    QString m_upLatencyStats;
    QVariantList m_downGroups;
    QVariantList m_upGroups;
    QString m_copyHint;
};

#endif
