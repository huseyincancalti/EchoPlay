#include <QElapsedTimer>
#include <QFileInfo>
#include <QCoreApplication>
#include <QSignalSpy>
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <vector>
#include <windows.h>
#include <psapi.h>
#include "Thumbnailer.h"

// Latency bench of the seek-bar preview (tools/thumbs-bench.sh runs it): how long the hidden libmpv player
// takes to open a file and show the first picture (cold), then to serve a keyframe picture and the exact
// one at 8 positions in the order a pointer might visit them, and how much memory the player adds.
// Usage: bench_thumbnailer <video> <duration-seconds>. No window is ever shown.

namespace {

constexpr int TimeoutMs = 15000;
constexpr double Fractions[] = {0.11, 0.59, 0.30, 0.84, 0.42, 0.21, 0.73, 0.04};

// A negative value stands for a timeout.
QByteArray num(double ms) { return ms < 0 ? QByteArray("TIMEOUT") : QByteArray::number(qRound(ms)); }

struct Stat { double median, max; }; // both -1 when any value timed out

Stat summarize(std::vector<double> v)
{
    if (std::any_of(v.begin(), v.end(), [](double x) { return x < 0; })) return {-1, -1};
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return {n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2, v.back()};
}

QByteArray json(const Stat &s)
{
    const QByteArray median = s.median < 0 ? "\"TIMEOUT\"" : num(s.median), max = s.max < 0 ? "\"TIMEOUT\"" : num(s.max);
    return "{\"median\":" + median + ",\"max\":" + max + "}";
}

double privateMb()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof(counters));
    return counters.PrivateUsage / (1024.0 * 1024.0);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C"); // libmpv requires it
    const double duration = argc >= 3 ? QByteArray(argv[2]).toDouble() : 0;
    if (!(duration > 0)) {
        std::printf("usage: bench_thumbnailer <video> <duration-seconds>\n");
        return 2;
    }
    const QFileInfo video(QString::fromLocal8Bit(argv[1]));

    const double memoryBefore = privateMb();
    Thumbnailer t(nullptr);
    t.setFile(video.absoluteFilePath());
    QSignalSpy pictures(&t, &Thumbnailer::imageChanged); // after setFile: its clearing signal is no picture
    auto nextPicture = [&](const QElapsedTimer &since) { // ms from `since`, -1 on timeout
        return pictures.wait(TimeoutMs) ? since.nsecsElapsed() / 1e6 : -1.0;
    };
    bool failed = false;

    QElapsedTimer clock;
    clock.start();
    t.request(0.37 * duration); // creates the hidden player and opens the file
    const double cold = nextPicture(clock);
    if (cold < 0) {
        std::printf("cold: TIMEOUT\n");
        return 1;
    }
    const double coldExact = nextPicture(clock); // also the exact one, so the player is idle again
    std::printf("cold: first picture %s ms, exact picture %s ms\n", num(cold).constData(), num(coldExact).constData());
    failed |= coldExact < 0;

    std::vector<double> keyframe, exact;
    for (const double fraction : Fractions) {
        const double seconds = fraction * duration;
        clock.restart();
        t.request(seconds);
        const double key = nextPicture(clock);
        const double exactFromRequest = key < 0 ? -1 : nextPicture(clock);
        // The exact seek only starts SettleMs after the keyframe picture: that wait is not the seek's cost.
        const double exactSeek = exactFromRequest < 0 ? -1 : exactFromRequest - Thumbnailer::SettleMs;
        std::printf("%7.2f s: keyframe %s ms, exact %s ms (%s ms without the %d ms wait)\n", seconds,
                    num(key).constData(), num(exactFromRequest).constData(), num(exactSeek).constData(),
                    Thumbnailer::SettleMs);
        keyframe.push_back(key);
        exact.push_back(exactSeek);
    }
    const double memory = privateMb() - memoryBefore;
    const QSize size = t.image().size();
    pictures.wait(Thumbnailer::IdleMs + 1000); // no request: the hidden player closes the file
    const double memoryIdle = privateMb() - memoryBefore;
    std::printf("memory: %.1f MB private while open, %.1f MB after %d ms idle, picture %dx%d\n", memory,
                memoryIdle, Thumbnailer::IdleMs, size.width(), size.height());

    const Stat keyStat = summarize(keyframe), exactStat = summarize(exact);
    failed |= keyStat.median < 0 || exactStat.median < 0;
    std::printf("THUMBS {\"file\":\"%s\",\"cold_ms\":%s,\"keyframe_ms\":%s,\"exact_ms\":%s,\"picture\":\"%dx%d\",\"memory_mb\":%d,\"memory_idle_mb\":%d}\n",
                qPrintable(video.fileName()), num(cold).constData(), json(keyStat).constData(),
                json(exactStat).constData(), size.width(), size.height(), qRound(memory), qRound(memoryIdle));
    std::fflush(stdout);
    return failed ? 1 : 0;
}
