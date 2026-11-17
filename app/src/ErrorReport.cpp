#include "ErrorReport.h"

#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QSysInfo>
#include <QUrl>

namespace {

constexpr auto IssuesUrl = "https://github.com/huseyincancalti/EchoPlay/issues/new";
constexpr int MaxBodyLength = 5000; // encoded, about GitHub's limit for a prefilled URL with room to spare

struct Wording
{
    const char *kind;
    const char *happened;
    const char *step;
    const char *tryThis;
};

const Wording Words[] = {
    {"format", "the file has no video or audio EchoPlay can read.",
     "opening the file (not reading video, reading audio or the audio device).",
     "if it is still recording, wait until it finishes. If the recording was cut off while saving, the file is damaged; try another copy of it."},
    {"load", "the file could not be read.", "opening the file.",
     "check that the file is still there and that no other program holds it, then open it again."},
    {"empty", "the file opened, but no video or audio came out of it.", "reading video and audio.",
     "the file may be damaged or use a format this version cannot decode; try another copy of it."},
    {"audio", "the sound device could not start. The file itself may be fine.", "the audio device.",
     "check that the device is connected and not held by another program, or pick another one in Settings, Audio."},
    {"video", "the picture could not start. The file itself may be fine.", "reading video.",
     "update the graphics driver, then open the file again."},
    {"other", "something went wrong while playing the file.", "playing the file.", "open the file again; if it keeps happening, report it."},
};

const Wording &wording(const QString &kind)
{
    for (const Wording &words : Words)
        if (kind == QLatin1String(words.kind)) return words;
    return Words[std::size(Words) - 1];
}

// C:\Users\name becomes C:\Users\<user>, with either slash and in any case.
QString redacted(QString text, const QString &homeFolder)
{
    const QString home = QDir::cleanPath(homeFolder);
    const int cut = home.lastIndexOf('/');
    if (cut <= 0) return text;
    const QString safe = home.left(cut) + "/<user>";
    text.replace(home, safe, Qt::CaseInsensitive);
    text.replace(QString(home).replace('/', '\\'), QString(safe).replace('/', '\\'), Qt::CaseInsensitive);
    return text;
}

} // namespace

QString ErrorReport::kind(const QString &mpvError)
{
    if (mpvError == "unrecognized file format") return "format";
    if (mpvError == "loading failed") return "load";
    if (mpvError == "no audio or video data played") return "empty";
    if (mpvError == "audio output initialization failed") return "audio";
    if (mpvError == "video output initialization failed") return "video";
    return "other";
}

QString ErrorReport::report(const QString &path, const QString &mpvError, const QStringList &log, const QString &appVersion,
                            const QString &mpvVersion)
{
    return report(path, mpvError, log, appVersion, mpvVersion, QDir::homePath());
}

QString ErrorReport::report(const QString &path, const QString &mpvError, const QStringList &log, const QString &appVersion,
                            const QString &mpvVersion, const QString &homeFolder)
{
    const Wording &words = wording(kind(mpvError));
    const QFileInfo file(path);
    QString size;
    if (file.exists()) size = ", " + QLocale::c().formattedDataSize(file.size(), 1, QLocale::DataSizeTraditionalFormat);

    QStringList lines = {
        "EchoPlay could not play this file.",
        "",
        QString("What happened: ") + words.happened,
        QString("Step: ") + words.step,
        "Cause, from the mpv engine: " + mpvError + ".",
        QString("What to try: ") + words.tryThis,
        "",
        "Technical details",
        "File: " + file.fileName() + " (." + file.suffix() + size + ")",
        "Folder: " + QDir::toNativeSeparators(file.absolutePath()),
        "EchoPlay " + appVersion + ", " + mpvVersion + ", " + QSysInfo::prettyProductName(),
    };
    if (!log.isEmpty()) lines << "Last log lines:" << log;
    return redacted(lines.join('\n'), homeFolder);
}

QString ErrorReport::issueUrl(const QString &report)
{
    QString body = report;
    while (QUrl::toPercentEncoding(body).size() > MaxBodyLength) {
        const int cut = body.lastIndexOf('\n');
        if (cut <= 0) break;
        body.truncate(cut);
    }
    const QByteArray title = QUrl::toPercentEncoding(body.section('\n', 2, 2));
    return QString(IssuesUrl) + "?title=" + QString::fromLatin1(title) + "&body=" + QString::fromLatin1(QUrl::toPercentEncoding(body));
}

void ErrorReport::copy(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}
