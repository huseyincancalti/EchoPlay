#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class AppState;

// The whole look as tokens (colors, radii, blur, spacing, fonts, opacity, motion) in one JSON file. The
// shipped defaults are embedded in the exe; theme.json next to the exe overrides single tokens, read once
// at start. Tokens only, like a constrained userChrome.css: a user file can change the value of a token
// the defaults have, never add one or change its type.
// On top of the tokens, the choices made in Settings > Appearance, saved in AppState: an empty choice means
// the token's value. Surfaces that follow the mode (start screen, settings) read `colors`; chrome over the
// video stays dark in both modes and reads tokens.glass.
// QML: color: Theme.colors.text, radius: Theme.tokens.radius.lg, font.family: Theme.fontFamily
class Theme : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantMap tokens READ tokens CONSTANT)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY modeChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)                 // "dark", "light", "system"
    Q_PROPERTY(bool dark READ dark NOTIFY modeChanged)                                  // the mode in effect
    Q_PROPERTY(QString accentChoice READ accentChoice WRITE setAccentChoice NOTIFY accentChanged) // empty = the token
    Q_PROPERTY(QColor accent READ accent NOTIFY accentChanged)
    Q_PROPERTY(QColor onAccent READ onAccent NOTIFY accentChanged)
    Q_PROPERTY(QString barStyle READ barStyle WRITE setBarStyle NOTIFY barStyleChanged) // "island", "full"
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontChanged)
    Q_PROPERTY(int glassStrength READ glassStrength WRITE setGlassStrength NOTIFY glassChanged) // 0 less video .. 2 more
    Q_PROPERTY(double barOpacity READ barOpacity NOTIFY glassChanged)
    Q_PROPERTY(double popoverOpacity READ popoverOpacity NOTIFY glassChanged)
    Q_PROPERTY(QStringList fonts READ fonts NOTIFY fontsChanged)                        // bundled, then the user's

public:
    // fontsDir: where "Load font" keeps the user's font files.
    Theme(AppState *state, const QString &defaultsPath, const QString &userPath, const QString &fontsDir,
          QObject *parent = nullptr);

    QVariantMap tokens() const { return m_tokens; }
    QVariantMap colors() const { return m_tokens.value(dark() ? "dark" : "light").toMap(); }

    QString mode() const;
    void setMode(const QString &mode); // anything else is ignored
    bool dark() const;
    void setSystemDark(bool dark); // follows Windows' app mode once startGui() ran

    QString accentChoice() const;
    void setAccentChoice(const QString &color); // empty or a color name; anything else is ignored
    QColor accent() const;
    // The token color with the better contrast on accent: any color can be picked.
    QColor onAccent() const;

    QString barStyle() const;
    void setBarStyle(const QString &style); // anything else is ignored

    QString fontFamily() const;
    void setFontFamily(const QString &family); // loads a bundled font the first time it is chosen
    QStringList fonts() const;

    int glassStrength() const;
    void setGlassStrength(int strength);  // clamped to 0..2
    double barOpacity() const { return glassOpacity("bar"); }
    double popoverOpacity() const { return glassOpacity("popover"); }

    // Needs QGuiApplication, so tests stay guiless: loads the icon fonts and the chosen font, and follows
    // Windows' dark or light mode from now on.
    void startGui();
    // Settings > Appearance > Load font: copies the file to fontsDir and returns its family, empty when it
    // is not a font.
    Q_INVOKABLE QString loadFontFile(const QUrl &file);

    static Theme *create(QQmlEngine *, QJSEngine *)
    {
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }
    static void setInstance(Theme *theme) { s_instance = theme; } // main.cpp, before QML loads

signals:
    void modeChanged();
    void accentChanged();
    void barStyleChanged();
    void fontChanged();
    void glassChanged();
    void fontsChanged();

private:
    double glassOpacity(const char *token) const;
    QString loadBundledFont(const QString &family);
    void scanUserFonts();

    static inline Theme *s_instance = nullptr;
    AppState *m_state;
    QVariantMap m_tokens;
    QString m_userDir;  // the folder of the user's theme.json: a font.file token is relative to it
    QString m_fontsDir;
    QStringList m_userFonts;
    bool m_guiStarted = false;
    bool m_systemDark = true;
};
