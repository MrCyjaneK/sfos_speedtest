#include "speedtest.h"

#include <QByteArray>
#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkReply>
#include <QVariantMap>
#include <QNetworkRequest>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

static const char kDown[] = "https://speed.cloudflare.com/__down?bytes=%1";
static const char kUp[] = "https://speed.cloudflare.com/__up";
static const char kRipe[] = "https://stat.ripe.net/data/as-overview/data.json?resource=AS%1";
static const int kFinishMs = 1000;
static const int kMinBwMs = 10;
static const int kLoadedMinMs = 250;
static const int kLoadThrottleMs = 400;
static const int kLoadedMax = 20;

SpeedTest::SpeedTest(QObject *parent)
    : QObject(parent)
    , m_reply(0)
    , m_loadReply(0)
    , m_ripeReply(0)
    , m_jobI(-1)
    , m_left(0)
    , m_busy(false)
    , m_downFinished(false)
    , m_upFinished(false)
    , m_loadingDown(false)
    , m_keepLoad(false)
    , m_downText(QStringLiteral("…"))
    , m_upText(QStringLiteral("…"))
    , m_pingText(QStringLiteral("…"))
    , m_jitterText(QStringLiteral("…"))
{
    m_loadTimer.setInterval(kLoadThrottleMs);
    connect(&m_loadTimer, SIGNAL(timeout()), this, SLOT(onLoadTick()));
}

void SpeedTest::buildJobs()
{
    m_jobs.clear();
    m_jobs << Job(Locate, 0, 1)
           << Job(Latency, 0, 2)
           << Job(Download, 100000, 1, true)
           << Job(Latency, 0, 20)
           << Job(Download, 100000, 9)
           << Job(Latency, 0, 2)
           << Job(Download, 1000000, 8)
           << Job(Latency, 0, 2)
           << Job(Upload, 100000, 8)
           << Job(Latency, 0, 2)
           << Job(Upload, 1000000, 6)
           << Job(Latency, 0, 2)
           << Job(Download, 10000000, 6)
           << Job(Latency, 0, 2)
           << Job(Upload, 10000000, 4)
           << Job(Latency, 0, 2)
           << Job(Download, 25000000, 4)
           << Job(Latency, 0, 2)
           << Job(Upload, 25000000, 4)
           << Job(Latency, 0, 2)
           << Job(Download, 100000000, 3)
           << Job(Latency, 0, 2)
           << Job(Upload, 50000000, 3)
           << Job(Latency, 0, 2)
           << Job(Download, 250000000, 2);
}

void SpeedTest::start()
{
    if (m_busy)
        return;
    m_busy = true;
    m_error.clear();
    m_copyHint.clear();
    m_server.clear();
    m_ip.clear();
    m_proto.clear();
    m_asn.clear();
    m_network.clear();
    m_downText = m_upText = m_pingText = m_jitterText = QStringLiteral("…");
    m_pingRange.clear();
    m_jitterRange.clear();
    m_aimStreaming = m_aimGaming = m_aimRtc = QString();
    m_idleLatencyStats = m_downLatencyStats = m_upLatencyStats = QString();
    m_downGroups.clear();
    m_upGroups.clear();
    m_idleRtt.clear();
    m_downLoaded.clear();
    m_upLoaded.clear();
    m_pendingLoaded.clear();
    m_downSamples.clear();
    m_upSamples.clear();
    m_downFinished = m_upFinished = m_keepLoad = false;
    m_jobI = -1;
    m_left = 0;
    m_phase = tr("Locating…");
    buildJobs();
    emit changed();
    next();
}

void SpeedTest::next()
{
    if (m_left > 0) {
        fire();
        return;
    }
    for (;;) {
        m_jobI++;
        if (m_jobI >= m_jobs.size()) {
            finish();
            return;
        }
        const Job &j = m_jobs.at(m_jobI);
        if (j.kind == Download && m_downFinished)
            continue;
        if (j.kind == Upload && m_upFinished)
            continue;
        m_left = j.count;
        if (j.kind == Locate)
            m_phase = tr("Locating…");
        else if (j.kind == Latency)
            m_phase = tr("Ping %1").arg(m_idleRtt.size() + 1);
        else if (j.kind == Download)
            m_phase = tr("Download %1").arg(sizeLabel(j.bytes));
        else
            m_phase = tr("Upload %1").arg(sizeLabel(j.bytes));
        emit changed();
        fire();
        return;
    }
}

void SpeedTest::fire()
{
    const Job &j = m_jobs.at(m_jobI);
    if (j.kind == Locate || j.kind == Latency || j.kind == Download)
        getDown(j.kind == Download ? j.bytes : 0);
    else
        postUp(j.bytes);
}

void SpeedTest::getDown(qint64 bytes)
{
    QNetworkRequest req(QUrl(QString::fromLatin1(kDown).arg(bytes)));
    req.setRawHeader("Accept", "application/octet-stream");
    m_timer.start();
    m_reply = m_nam.get(req);
    connect(m_reply, SIGNAL(readyRead()), this, SLOT(drain()));
    connect(m_reply, SIGNAL(finished()), this, SLOT(onFinished()));
    if (bytes > 0)
        startLoaded();
}

void SpeedTest::drain()
{
    if (m_reply)
        m_reply->readAll();
}

void SpeedTest::postUp(qint64 bytes)
{
    QNetworkRequest req(QUrl(QString::fromLatin1(kUp)));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/octet-stream"));
    m_timer.start();
    m_reply = m_nam.post(req, QByteArray(int(bytes), 'x'));
    connect(m_reply, SIGNAL(finished()), this, SLOT(onFinished()));
    if (bytes > 0)
        startLoaded();
}

void SpeedTest::addLoaded(QList<qint64> &dst, const QList<qint64> &more)
{
    dst.append(more);
    while (dst.size() > kLoadedMax)
        dst.removeFirst();
}

void SpeedTest::startLoaded()
{
    m_pendingLoaded.clear();
    m_keepLoad = false;
    m_loadingDown = (m_jobs.at(m_jobI).kind == Download);
    m_loadTimer.start();
    onLoadTick();
}

void SpeedTest::stopLoaded(qint64 bwMs)
{
    m_loadTimer.stop();
    m_keepLoad = (bwMs >= kLoadedMinMs);
    if (m_keepLoad)
        addLoaded(m_loadingDown ? m_downLoaded : m_upLoaded, m_pendingLoaded);
    m_pendingLoaded.clear();
}

void SpeedTest::onLoadTick()
{
    if (m_loadReply)
        return;
    QNetworkRequest req(QUrl(QString::fromLatin1(kDown).arg(0)));
    m_loadReply = m_loadNam.get(req);
    m_loadReply->setProperty("t0", QDateTime::currentMSecsSinceEpoch());
    connect(m_loadReply, SIGNAL(finished()), this, SLOT(onLoadedPing()));
}

void SpeedTest::onLoadedPing()
{
    QNetworkReply *r = qobject_cast<QNetworkReply *>(sender());
    if (!r)
        return;
    const qint64 ms = QDateTime::currentMSecsSinceEpoch() - r->property("t0").toLongLong();
    if (r == m_loadReply)
        m_loadReply = 0;
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError || ms < 0)
        return;
    if (m_loadTimer.isActive()) {
        m_pendingLoaded.append(ms);
        return;
    }
    if (!m_keepLoad)
        return;
    addLoaded(m_loadingDown ? m_downLoaded : m_upLoaded, QList<qint64>() << ms);
    m_keepLoad = false;
    refreshLatency();
    if (!m_busy)
        refreshAim();
    emit changed();
}

void SpeedTest::fail(const QString &msg)
{
    stopLoaded(0);
    m_error = msg;
    m_phase.clear();
    m_busy = false;
    m_reply = 0;
    emit changed();
}

QByteArray SpeedTest::hdr(QNetworkReply *r, const char *a, const char *b)
{
    QByteArray v = r->rawHeader(a);
    if (v.isEmpty() && b)
        v = r->rawHeader(b);
    return v;
}

void SpeedTest::noteMeta(QNetworkReply *r)
{
    QString colo = QString::fromLatin1(hdr(r, "colo", "cf-meta-colo"));
    if (colo.isEmpty()) {
        const QString ray = QString::fromLatin1(r->rawHeader("cf-ray"));
        const int dash = ray.lastIndexOf(QLatin1Char('-'));
        if (dash > 0)
            colo = ray.mid(dash + 1);
    }
    const QString city = QString::fromLatin1(hdr(r, "city", "cf-meta-city"));
    const QString country = QString::fromLatin1(hdr(r, "country", "cf-meta-country"));
    m_ip = QString::fromLatin1(hdr(r, "cf-meta-ip", "ip"));
    m_asn = QString::fromLatin1(hdr(r, "asn", "cf-meta-asn"));
    m_proto = m_ip.contains(QLatin1Char(':')) ? QStringLiteral("IPv6")
                                              : QStringLiteral("IPv4");
    QStringList parts;
    if (!colo.isEmpty())
        parts << colo;
    if (!city.isEmpty())
        parts << (country.isEmpty() ? city : city + QStringLiteral(", ") + country);
    m_server = parts.isEmpty() ? QStringLiteral("speed.cloudflare.com")
                               : parts.join(QStringLiteral(" · "));
    if (!m_asn.isEmpty())
        lookupAsn();
}

void SpeedTest::lookupAsn()
{
    if (m_ripeReply)
        return;
    QNetworkRequest req(QUrl(QString::fromLatin1(kRipe).arg(m_asn)));
    m_ripeReply = m_nam.get(req);
    connect(m_ripeReply, SIGNAL(finished()), this, SLOT(onRipe()));
}

void SpeedTest::onRipe()
{
    QNetworkReply *r = qobject_cast<QNetworkReply *>(sender());
    if (!r)
        return;
    if (r == m_ripeReply)
        m_ripeReply = 0;
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError)
        return;
    const QString holder = QJsonDocument::fromJson(r->readAll()).object()
                               .value(QStringLiteral("data")).toObject()
                               .value(QStringLiteral("holder")).toString();
    if (!holder.isEmpty()) {
        m_network = holder;
        emit changed();
    }
}

QString SpeedTest::sizeLabel(qint64 bytes)
{
    if (bytes >= 1000000 && bytes % 1000000 == 0)
        return QString::number(bytes / 1000000) + QStringLiteral(" MB");
    if (bytes >= 1000 && bytes % 1000 == 0)
        return QString::number(bytes / 1000) + QStringLiteral(" kB");
    return QString::number(bytes) + QStringLiteral(" B");
}

QString SpeedTest::fmtMbps(double mbps)
{
    if (mbps >= 1000.0)
        return QString::number(mbps / 1000.0, 'f', mbps >= 10000.0 ? 0 : 1) + QStringLiteral(" Gbps");
    return QString::number(mbps, 'f', mbps >= 100.0 ? 0 : 1);
}

QString SpeedTest::fmtSpeed(double mbps)
{
    const QString t = fmtMbps(mbps);
    return t.contains(QStringLiteral("Gbps")) ? t : t + QStringLiteral(" Mbps");
}

double SpeedTest::p90Mbps(const QList<Sample> &samples)
{
    QList<double> bps;
    for (int i = 0; i < samples.size(); ++i) {
        if (samples.at(i).ms >= kMinBwMs)
            bps << samples.at(i).mbps;
    }
    return bps.isEmpty() ? -1.0 : percentile(bps, 0.9);
}

QString SpeedTest::fmtMs(double ms)
{
    if (ms >= 100.0)
        return QString::number(ms, 'f', 0);
    if (ms >= 10.0)
        return QString::number(ms, 'f', 1);
    return QString::number(ms, 'f', 2);
}

double SpeedTest::percentile(QList<double> v, double p)
{
    if (v.isEmpty())
        return 0;
    std::sort(v.begin(), v.end());
    if (v.size() == 1)
        return v.at(0);
    const double idx = p * (v.size() - 1);
    const int i = int(std::floor(idx));
    const double f = idx - i;
    if (i + 1 >= v.size())
        return v.at(i);
    return v.at(i) * (1.0 - f) + v.at(i + 1) * f;
}

double SpeedTest::meanAbsDelta(const QList<qint64> &v)
{
    if (v.size() < 2)
        return 0;
    double s = 0;
    for (int i = 1; i < v.size(); ++i)
        s += qAbs(v.at(i) - v.at(i - 1));
    return s / (v.size() - 1);
}

QString SpeedTest::statsLine(const QList<double> &v, const QString &unit)
{
    if (v.isEmpty())
        return QString();
    QList<double> s = v;
    std::sort(s.begin(), s.end());
    double sum = 0;
    for (int i = 0; i < s.size(); ++i)
        sum += s.at(i);
    auto fmt = [&](double x) {
        if (unit == QLatin1String("ms"))
            return fmtMs(x);
        if (x >= 1000.0)
            return QString::number(x / 1000.0, 'f', x >= 10000.0 ? 0 : 1);
        return QString::number(x, 'f', x >= 100.0 ? 0 : 1);
    };
    QString line = tr("min %1 · max %2 · avg %3 · med %4 · p25 %5 · p75 %6")
                       .arg(fmt(s.first()))
                       .arg(fmt(s.last()))
                       .arg(fmt(sum / s.size()))
                       .arg(fmt(percentile(s, 0.5)))
                       .arg(fmt(percentile(s, 0.25)))
                       .arg(fmt(percentile(s, 0.75)));
    if (unit == QLatin1String("ms"))
        line += QStringLiteral(" ms");
    return line;
}

int SpeedTest::aimScale(const double *domain, const int *range, int nDomain, double value)
{
    int i = 0;
    while (i < nDomain && value >= domain[i])
        ++i;
    return range[i];
}

QString SpeedTest::aimLabel(int idx)
{
    static const char *names[] = { "Bad", "Poor", "Average", "Good", "Great" };
    if (idx < 0)
        idx = 0;
    if (idx > 4)
        idx = 4;
    QString stars;
    for (int i = 0; i < 5; ++i)
        stars += (i <= idx) ? QString::fromUtf8("★") : QString::fromUtf8("☆");
    return QString::fromLatin1(names[idx]) + QLatin1Char(' ') + stars;
}

void SpeedTest::refreshHero()
{
    const double down = p90Mbps(m_downSamples);
    const double up = p90Mbps(m_upSamples);
    if (down >= 0)
        m_downText = fmtSpeed(down);
    if (up >= 0)
        m_upText = fmtSpeed(up);

    if (m_idleRtt.isEmpty())
        return;
    QList<double> p;
    for (int i = 0; i < m_idleRtt.size(); ++i)
        p << double(m_idleRtt.at(i));
    m_pingText = fmtMs(percentile(p, 0.5));
    QList<qint64> s = m_idleRtt;
    std::sort(s.begin(), s.end());
    m_pingRange = tr("%1 – %2 ms").arg(fmtMs(s.first())).arg(fmtMs(s.last()));
    if (m_idleRtt.size() < 2)
        return;
    m_jitterText = fmtMs(meanAbsDelta(m_idleRtt));
    QList<double> d;
    for (int i = 1; i < m_idleRtt.size(); ++i)
        d << double(qAbs(m_idleRtt.at(i) - m_idleRtt.at(i - 1)));
    std::sort(d.begin(), d.end());
    m_jitterRange = tr("%1 – %2 ms").arg(fmtMs(d.first())).arg(fmtMs(d.last()));
}

void SpeedTest::refreshLatency()
{
    QList<double> idle, down, up;
    for (int i = 0; i < m_idleRtt.size(); ++i)
        idle << double(m_idleRtt.at(i));
    for (int i = 0; i < m_downLoaded.size(); ++i)
        down << double(m_downLoaded.at(i));
    for (int i = 0; i < m_upLoaded.size(); ++i)
        up << double(m_upLoaded.at(i));
    m_idleLatencyStats = idle.isEmpty() ? QString()
        : tr("Unloaded (%1)  %2").arg(idle.size()).arg(statsLine(idle, QStringLiteral("ms")));
    m_downLatencyStats = down.isEmpty() ? QString()
        : tr("During download (%1)  %2").arg(down.size()).arg(statsLine(down, QStringLiteral("ms")));
    m_upLatencyStats = up.isEmpty() ? QString()
        : tr("During upload (%1)  %2").arg(up.size()).arg(statsLine(up, QStringLiteral("ms")));
}

QVariantList SpeedTest::sizeGroups(Kind dir) const
{
    const QList<Sample> &src = (dir == Download) ? m_downSamples : m_upSamples;
    QMap<qint64, QList<Sample> > by;
    for (int i = 0; i < src.size(); ++i)
        by[src.at(i).bytes].append(src.at(i));

    QList<qint64> planned;
    QMap<qint64, int> want;
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs.at(i).kind != dir)
            continue;
        const qint64 b = m_jobs.at(i).bytes;
        if (!want.contains(b))
            planned << b;
        want[b] += m_jobs.at(i).count;
    }

    QVariantList out;
    for (int i = 0; i < planned.size(); ++i) {
        const qint64 b = planned.at(i);
        const QList<Sample> samples = by.value(b);
        if (samples.isEmpty() && ((dir == Download && m_downFinished) || (dir == Upload && m_upFinished)))
            continue;
        QVariantMap g;
        g.insert(QStringLiteral("title"),
                 QStringLiteral("%1 (%2/%3)").arg(sizeLabel(b)).arg(samples.size()).arg(want.value(b)));
        QList<double> mbps;
        QVariantList runs;
        for (int n = 0; n < samples.size(); ++n) {
            mbps << samples.at(n).mbps;
            QVariantMap run;
            const QString speed = fmtSpeed(samples.at(n).mbps);
            run.insert(QStringLiteral("n"), n + 1);
            run.insert(QStringLiteral("ms"), samples.at(n).ms);
            run.insert(QStringLiteral("mbps"), speed);
            runs << run;
        }
        g.insert(QStringLiteral("stats"), statsLine(mbps, QStringLiteral("Mbps")));
        g.insert(QStringLiteral("runs"), runs);
        out << g;
    }
    return out;
}

void SpeedTest::refreshGroups()
{
    m_downGroups = sizeGroups(Download);
    m_upGroups = sizeGroups(Upload);
}

void SpeedTest::refreshAim()
{
    if (m_idleRtt.isEmpty())
        return;
    QList<double> pingMs;
    for (int i = 0; i < m_idleRtt.size(); ++i)
        pingMs << double(m_idleRtt.at(i));
    const double latency = percentile(pingMs, 0.5);
    const double jitter = meanAbsDelta(m_idleRtt);

    const double downMbps = p90Mbps(m_downSamples);
    const double download = downMbps < 0 ? 0 : downMbps * 1e6;

    QList<double> loaded;
    for (int i = 0; i < m_downLoaded.size(); ++i)
        loaded << double(m_downLoaded.at(i));
    for (int i = 0; i < m_upLoaded.size(); ++i)
        loaded << double(m_upLoaded.at(i));
    double loadedInc = 0;
    const bool haveLoaded = !loaded.isEmpty();
    if (haveLoaded)
        loadedInc = percentile(loaded, 0.5) - latency;

    static const double latD[] = { 10, 20, 50, 100, 500 };
    static const int latR[] = { 20, 10, 5, 0, -10, -20 };
    static const double jitD[] = { 10, 20, 100, 500 };
    static const int jitR[] = { 10, 5, 0, -10, -20 };
    static const double bwD[] = { 1e6, 10e6, 50e6, 100e6 };
    static const int bwR[] = { 0, 5, 10, 20, 30 };
    static const double streamTh[] = { 15, 20, 40, 60 };
    static const double gameTh[] = { 5, 15, 25, 30 };
    static const double rtcTh[] = { 5, 15, 25, 40 };
    static const int classR[] = { 0, 1, 2, 3, 4 };

    const int pLoss = 10;
    const int pLat = aimScale(latD, latR, 5, latency);
    const int pJit = aimScale(jitD, jitR, 4, jitter);
    const int pDown = downMbps < 0 ? 0 : aimScale(bwD, bwR, 4, download);
    const int pInc = haveLoaded ? aimScale(latD, latR, 5, loadedInc) : 0;

    if (downMbps >= 0 && haveLoaded)
        m_aimStreaming = aimLabel(aimScale(streamTh, classR, 4, qMax(0, pLat + pLoss + pDown + pInc)));
    if (haveLoaded)
        m_aimGaming = aimLabel(aimScale(gameTh, classR, 4, qMax(0, pLat + pLoss + pInc)));
    if (haveLoaded)
        m_aimRtc = aimLabel(aimScale(rtcTh, classR, 4, qMax(0, pLat + pJit + pLoss + pInc)));
}

void SpeedTest::copyResults()
{
    QStringList lines;
    lines << QStringLiteral("Speedtest");
    if (!m_downText.isEmpty() && m_downText != QStringLiteral("…"))
        lines << tr("Download: %1 (90th percentile)").arg(m_downText);
    if (!m_upText.isEmpty() && m_upText != QStringLiteral("…"))
        lines << tr("Upload: %1 (90th percentile)").arg(m_upText);
    if (!m_pingText.isEmpty() && m_pingText != QStringLiteral("…")) {
        QString lat = tr("Latency: %1 ms").arg(m_pingText);
        if (!m_pingRange.isEmpty())
            lat += QStringLiteral(" (") + m_pingRange + QLatin1Char(')');
        lines << lat;
    }
    if (!m_jitterText.isEmpty() && m_jitterText != QStringLiteral("…")) {
        QString jit = tr("Jitter: %1 ms").arg(m_jitterText);
        if (!m_jitterRange.isEmpty())
            jit += QStringLiteral(" (") + m_jitterRange + QLatin1Char(')');
        lines << jit;
    }
    if (!m_aimStreaming.isEmpty())
        lines << tr("Streaming: %1").arg(m_aimStreaming);
    if (!m_aimGaming.isEmpty())
        lines << tr("Gaming: %1").arg(m_aimGaming);
    if (!m_aimRtc.isEmpty())
        lines << tr("Video calls: %1").arg(m_aimRtc);
    lines << QString();
    if (!m_proto.isEmpty())
        lines << tr("Connected via %1").arg(m_proto);
    if (!m_server.isEmpty())
        lines << tr("Server: %1").arg(m_server);
    if (!m_asn.isEmpty()) {
        if (m_network.isEmpty())
            lines << tr("Network: AS%1").arg(m_asn);
        else
            lines << tr("Network: %1 (AS%2)").arg(m_network).arg(m_asn);
    }
    if (!m_ip.isEmpty())
        lines << tr("Your IP: %1").arg(m_ip);
    if (!m_idleLatencyStats.isEmpty())
        lines << QString() << m_idleLatencyStats;
    if (!m_downLatencyStats.isEmpty())
        lines << m_downLatencyStats;
    if (!m_upLatencyStats.isEmpty())
        lines << m_upLatencyStats;

    for (int pass = 0; pass < 2; ++pass) {
        const QVariantList &groups = pass ? m_upGroups : m_downGroups;
        if (groups.isEmpty())
            continue;
        lines << QString() << (pass ? tr("Upload") : tr("Download"));
        for (int i = 0; i < groups.size(); ++i) {
            const QVariantMap g = groups.at(i).toMap();
            lines << g.value(QStringLiteral("title")).toString();
            const QString stats = g.value(QStringLiteral("stats")).toString();
            if (!stats.isEmpty())
                lines << QStringLiteral("  ") + stats;
        }
    }

    if (QGuiApplication::clipboard())
        QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    m_copyHint = tr("Copied");
    emit changed();
}

void SpeedTest::finish()
{
    stopLoaded(0);
    refreshHero();
    refreshGroups();
    refreshLatency();
    refreshAim();
    m_phase.clear();
    m_busy = false;
    if (!m_asn.isEmpty() && m_network.isEmpty())
        lookupAsn();
    emit changed();
}

void SpeedTest::onFinished()
{
    QNetworkReply *r = qobject_cast<QNetworkReply *>(sender());
    if (!r || r != m_reply)
        return;
    m_reply = 0;
    const qint64 ms = m_timer.elapsed();
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError) {
        fail(r->errorString());
        return;
    }

    const Job &j = m_jobs.at(m_jobI);
    if (j.kind == Locate) {
        noteMeta(r);
        m_left = 0;
        next();
        return;
    }
    if (j.kind == Latency) {
        m_idleRtt.append(ms);
        m_left--;
        refreshHero();
        refreshLatency();
        if (m_left > 0)
            m_phase = tr("Ping %1").arg(m_idleRtt.size() + 1);
        emit changed();
        next();
        return;
    }

    stopLoaded(ms);
    Sample s;
    s.bytes = j.bytes;
    s.ms = ms;
    s.mbps = (ms > 0) ? (double(j.bytes) * 8.0) / (double(ms) / 1000.0) / 1e6 : 0;
    if (j.kind == Download)
        m_downSamples.append(s);
    else
        m_upSamples.append(s);
    m_left--;

    if (m_left == 0 && !j.bypass) {
        qint64 minMs = ms;
        const QList<Sample> &src = (j.kind == Download) ? m_downSamples : m_upSamples;
        for (int i = 0; i < src.size(); ++i) {
            if (src.at(i).bytes == j.bytes)
                minMs = qMin(minMs, src.at(i).ms);
        }
        if (minMs > kFinishMs) {
            if (j.kind == Download)
                m_downFinished = true;
            else
                m_upFinished = true;
        }
    }

    refreshHero();
    refreshGroups();
    refreshLatency();
    emit changed();
    next();
}
