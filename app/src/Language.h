#pragma once

#include <QLocale>
#include <QRegularExpression>
#include <QString>

// A track's language tag (mpv's "lang": eng, tur, en-US, ...) as people read it, in that language: English, Türkçe,
// 日本語. Empty for none or "und"; an unknown tag comes back as it is, upper-case. Header only: the mixer's unit tests
// use it without the rest of I18n.
inline QString languageName(const QString &code)
{
    const QString base = code.section(QRegularExpression("[-_]"), 0, 0).trimmed().toLower();
    if (base.isEmpty() || base == "und" || base == "mul" || base == "zxx" || base == "mis") return QString();
    if (base == "nor" || base == "no") return QStringLiteral("Norsk"); // Qt knows only Bokmål and Nynorsk
    const QLocale::Language language = QLocale::codeToLanguage(base);
    if (language == QLocale::AnyLanguage || language == QLocale::C) return code.toUpper();
    // Qt names some by their default country ("American English", "español de España", "简体中文"); a track's tag
    // says only the language.
    QString name;
    switch (language) {
    case QLocale::English: name = QStringLiteral("English"); break;
    case QLocale::Spanish: name = QStringLiteral("Español"); break;
    case QLocale::Chinese: name = QStringLiteral("中文"); break;
    case QLocale::Indonesian: name = QStringLiteral("Bahasa Indonesia"); break;
    default: name = QLocale(language).nativeLanguageName();
    }
    if (name.isEmpty()) name = QLocale::languageToString(language);
    return name.left(1).toUpper() + name.mid(1);
}
