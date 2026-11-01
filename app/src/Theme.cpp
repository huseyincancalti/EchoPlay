#include "Theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStyleHints>
#include <QUrl>
#include <QtMath>
#include "AppState.h"

namespace {

// The root object of a JSON file. Empty, with a warning that names the file, when it cannot be read or its
// root is not an object. A user file that does not exist is the normal case and stays silent.
QJsonObject readObject(const QString &path, bool mustExist)
{
    QFile f(path);
    if (!f.exists() && !mustExist) return {};
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("Theme: cannot read %s, ignored", qPrintable(path));
        return {};
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (!doc.isObject()) {
        qWarning("Theme: %s is not a JSON object (%s), ignored", qPrintable(path), qPrintable(err.errorString()));
        return {};
    }
    return doc.object();
}

// User values replace the base's key by key, nested objects recursively. A key the base lacks, or a value
// of another JSON type than the base's, is ignored: "radius": "big" must not turn a number token into text.
QJsonObject merged(QJsonObject base, const QJsonObject &user)
{
    for (auto it = user.begin(); it != user.end(); ++it) {
        const QJsonValue current = base.value(it.key());
        if (current.type() != it.value().type()) continue; // also covers a missing key (Undefined)
        base.insert(it.key(), current.isObject() ? QJsonValue(merged(current.toObject(), it.value().toObject()))
                                                 : it.value());
    }
    return base;
}

// WCAG contrast of a color against another (1..21).
double luminance(const QColor &c)
{
    const auto channel = [](double v) { return v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}

double contrast(const QColor &a, const QColor &b)
{
    const double la = luminance(a), lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

const QStringList BundledFonts = {"Geist", "Manrope", "Plus Jakarta Sans"};

// The bundled font's file in the resources: the family name without spaces.
QString bundledFile(const QString &family)
{
    return ":/fonts/" + QString(family).remove(' ') + ".ttf";
}

const QStringList FontFilters = {"*.ttf", "*.otf"};

} // namespace

Theme::Theme(AppState *state, const QString &defaultsPath, const QString &userPath, const QString &fontsDir, QObject *parent)
    : QObject(parent), m_state(state), m_userDir(QFileInfo(userPath).absolutePath()), m_fontsDir(fontsDir)
{
    m_tokens = merged(readObject(defaultsPath, true), readObject(userPath, false)).toVariantMap();
}

QString Theme::mode() const { return m_state->themeMode(); }

void Theme::setMode(const QString &mode)
{
    if (mode == this->mode() || !QStringList{"dark", "light", "system"}.contains(mode)) return;
    m_state->setThemeMode(mode);
    emit modeChanged();
}

bool Theme::dark() const
{
    const QString m = mode();
    return m == "system" ? m_systemDark : m == "dark";
}

void Theme::setSystemDark(bool dark)
{
    if (dark == m_systemDark) return;
    m_systemDark = dark;
    if (mode() == "system") emit modeChanged();
}

QString Theme::accentChoice() const { return m_state->accentColor(); }

void Theme::setAccentChoice(const QString &color)
{
    if (color == accentChoice() || (!color.isEmpty() && !QColor::isValidColorName(color))) return;
    m_state->setAccentColor(color);
    emit accentChanged();
}

QColor Theme::accent() const
{
    const QColor choice(accentChoice());
    return choice.isValid() ? choice : QColor(m_tokens.value("accent").toString());
}

QColor Theme::onAccent() const
{
    const QVariantMap on = m_tokens.value("onAccent").toMap();
    const QColor dark(on.value("dark").toString()), light(on.value("light").toString());
    return contrast(accent(), dark) >= contrast(accent(), light) ? dark : light;
}

QString Theme::barStyle() const
{
    const QString choice = m_state->barStyle();
    return !choice.isEmpty() ? choice : m_tokens.value("bar").toMap().value("style").toString();
}

void Theme::setBarStyle(const QString &style)
{
    if (style == barStyle() || !QStringList{"island", "full"}.contains(style)) return;
    m_state->setBarStyle(style);
    emit barStyleChanged();
}

QString Theme::fontFamily() const
{
    const QString choice = m_state->fontFamily();
    return !choice.isEmpty() ? choice : m_tokens.value("font").toMap().value("family").toString();
}

void Theme::setFontFamily(const QString &family)
{
    if (family == fontFamily() || !fonts().contains(family)) return;
    if (m_guiStarted && BundledFonts.contains(family)) loadBundledFont(family);
    m_state->setFontFamily(family);
    emit fontChanged();
}

QStringList Theme::fonts() const { return BundledFonts + m_userFonts; }

int Theme::glassStrength() const { return m_state->glassStrength(); }

void Theme::setGlassStrength(int strength)
{
    strength = qBound(0, strength, 2);
    if (strength == glassStrength()) return;
    m_state->setGlassStrength(strength);
    emit glassChanged();
}

// Strength 1 is the token's value; more strength shows more of the video, so a lower opacity.
double Theme::glassOpacity(const char *token) const
{
    const QVariantMap glass = m_tokens.value("glass").toMap();
    return qBound(0.0, glass.value(token).toDouble() - (glassStrength() - 1) * glass.value("strengthStep").toDouble(), 1.0);
}

QString Theme::loadBundledFont(const QString &family)
{
    const int id = QFontDatabase::addApplicationFont(bundledFile(family));
    return id < 0 ? QString() : QFontDatabase::applicationFontFamilies(id).value(0);
}

// The user's font files are only read when one of them is in use or Settings lists them: most people never
// have any, and a start must not pay for it.
void Theme::scanUserFonts()
{
    QStringList families;
    const QFileInfoList files = QDir(m_fontsDir).entryInfoList(FontFilters, QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
        const int id = QFontDatabase::addApplicationFont(file.absoluteFilePath());
        if (id >= 0) families += QFontDatabase::applicationFontFamilies(id).value(0);
    }
    families.removeDuplicates();
    families.removeAll(QString());
    if (families == m_userFonts) return;
    m_userFonts = families;
    emit fontsChanged();
}

void Theme::startGui()
{
    m_guiStarted = true;
    QFontDatabase::addApplicationFont(":/fonts/Icons.ttf");
    QFontDatabase::addApplicationFont(":/fonts/IconsFilled.ttf");

    const QString family = fontFamily();
    const QString tokenFile = m_tokens.value("font").toMap().value("file").toString();
    if (!tokenFile.isEmpty() && m_state->fontFamily().isEmpty())
        QFontDatabase::addApplicationFont(QDir(m_userDir).filePath(tokenFile));
    else if (BundledFonts.contains(family)) loadBundledFont(family);
    else scanUserFonts();

    QStyleHints *hints = QGuiApplication::styleHints();
    m_systemDark = hints->colorScheme() != Qt::ColorScheme::Light;
    connect(hints, &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme scheme) { setSystemDark(scheme != Qt::ColorScheme::Light); });
}

QString Theme::loadFontFile(const QUrl &file)
{
    const QString source = file.toLocalFile();
    const int id = QFontDatabase::addApplicationFont(source);
    const QString family = id < 0 ? QString() : QFontDatabase::applicationFontFamilies(id).value(0);
    if (family.isEmpty()) return {};
    QDir().mkpath(m_fontsDir);
    const QString copy = QDir(m_fontsDir).filePath(QFileInfo(source).fileName());
    if (QFileInfo(source) != QFileInfo(copy)) {
        QFile::remove(copy); // a newer file of the same name
        QFile::copy(source, copy);
    }
    scanUserFonts();
    return family;
}
