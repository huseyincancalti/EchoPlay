#include "I18n.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

I18n::I18n(const QString &dir, QObject *parent) : QObject(parent)
{
    const QFileInfoList files = QDir(dir).entryInfoList({"*.json"}, QDir::Files, QDir::Name);
    for (const QFileInfo &info : files) {
        QFile f(info.filePath());
        QJsonObject obj;
        if (f.open(QIODevice::ReadOnly)) obj = QJsonDocument::fromJson(f.readAll()).object();
        if (obj.value("lang_name").toString().isEmpty()) {
            qWarning("I18n: skipping %s (not a JSON object with lang_name)", qPrintable(info.fileName()));
            continue;
        }
        QVariantMap strings; // only strings: QML calls .arg() on them
        for (auto it = obj.begin(); it != obj.end(); ++it)
            if (it.value().isString()) strings.insert(it.key(), it.value().toString());
        m_files.insert(info.completeBaseName(), strings);
    }
    for (auto it = m_files.cbegin(); it != m_files.cend(); ++it)
        m_languages.append(QVariantMap{{"code", it.key()}, {"name", it.value().value("lang_name")}});
}

void I18n::setLanguage(const QString &code)
{
    QString use = code;
    if (!m_files.contains(use)) {
        use = QLocale::system().name().left(2);
        if (!m_files.contains(use)) use = "en";
    }
    if (use == m_language) return;
    m_language = use;
    m_strings = m_files.value("en");
    const QVariantMap current = m_files.value(use);
    for (auto it = current.cbegin(); it != current.cend(); ++it) m_strings.insert(it.key(), it.value());
    emit languageChanged();
}
