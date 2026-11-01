#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// What the error screen says when a file cannot play, and the report behind "Copy details": it reads like a clear
// exception report a person (or a chatbot) understands, in English whatever the UI language: what happened in one
// sentence, at which step, the cause in mpv's words, what to try, then the technical block. The Windows user name is
// redacted from every path. EchoPlay itself sends nothing: the user copies it or opens a prefilled GitHub issue.
class ErrorReport : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit ErrorReport(QObject *parent = nullptr) : QObject(parent) {}

    // The kind of failure from mpv_error_string's text: "format", "load", "empty", "audio", "video" or "other". The
    // UI's own words for each are in lang/*.json (error_<kind>_title, error_<kind>_body).
    Q_INVOKABLE static QString kind(const QString &mpvError);
    Q_INVOKABLE static QString report(const QString &path, const QString &mpvError, const QStringList &log,
                                      const QString &appVersion, const QString &mpvVersion);
    // homeFolder is the path whose user name is redacted (a parameter so the tests do not depend on the machine).
    static QString report(const QString &path, const QString &mpvError, const QStringList &log, const QString &appVersion,
                          const QString &mpvVersion, const QString &homeFolder);
    // The new-issue page with this report filled in. GitHub refuses very long URLs: the log lines go first.
    Q_INVOKABLE static QString issueUrl(const QString &report);
    Q_INVOKABLE static void copy(const QString &text); // to the clipboard
};
