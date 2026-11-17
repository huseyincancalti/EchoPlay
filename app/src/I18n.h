#pragma once

#include <QMap>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include "Language.h"

// Languages are one flat JSON file each in a folder next to the exe (key lang_name = the language's own
// name). Adding a language = dropping a file there; the folder is scanned once at start. QML reads every
// visible string from `strings` (placeholders are Qt's %1, %2: I18n.strings.tracks_on.arg(2).arg(3)).
class I18n : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QVariantMap strings READ strings NOTIFY languageChanged)

public:
    explicit I18n(const QString &dir, QObject *parent = nullptr);

    QVariantList languages() const { return m_languages; } // [{code, name}], sorted by code
    QString language() const { return m_language; }
    // An empty or unknown code picks the system language, else English.
    void setLanguage(const QString &code);
    Q_INVOKABLE QString trackLanguage(const QString &code) const { return languageName(code); } // Language.h
    // English with the current language on top: a key missing in a translation shows English, never nothing.
    QVariantMap strings() const { return m_strings; }

    static I18n *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(I18n *i18n) { s_instance = i18n; } // main.cpp, before QML loads

signals:
    void languageChanged();

private:
    static inline I18n *s_instance = nullptr;
    QMap<QString, QVariantMap> m_files; // code -> its strings, sorted by code
    QVariantList m_languages;
    QString m_language;
    QVariantMap m_strings;
};
