#include "Theme.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

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

} // namespace

Theme::Theme(const QString &defaultsPath, const QString &userPath, QObject *parent) : QObject(parent)
{
    m_tokens = merged(readObject(defaultsPath, true), readObject(userPath, false)).toVariantMap();
}
