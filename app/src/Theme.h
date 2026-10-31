#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The whole look as tokens (colors, radii, blur, spacing, fonts, opacity, motion) in one JSON file. The
// shipped defaults are embedded in the exe; theme.json next to the exe overrides single tokens, read once
// at start. Tokens only, like a constrained userChrome.css: a user file can change the value of a token
// the defaults have, never add one or change its type.
// QML: color: Theme.tokens.accent, radius: Theme.tokens.radius.lg
class Theme : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantMap tokens READ tokens CONSTANT)

public:
    Theme(const QString &defaultsPath, const QString &userPath, QObject *parent = nullptr);

    QVariantMap tokens() const { return m_tokens; }

    static Theme *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(Theme *theme) { s_instance = theme; } // main.cpp, before QML loads

private:
    static inline Theme *s_instance = nullptr;
    QVariantMap m_tokens;
};
