#include "UpdateCheck.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <array>
#include <optional>
#include <utility>
#include "AppState.h"

namespace {

constexpr qint64 CheckIntervalSeconds = 24 * 60 * 60;
constexpr auto DevVersion = "0.0.0-dev";

std::optional<std::array<qint64, 3>> parseSemver(const QString &v)
{
    static const QRegularExpression re("^(\\d+)\\.(\\d+)\\.(\\d+)");
    const QRegularExpressionMatch m = re.match(v);
    if (!m.hasMatch()) return std::nullopt;
    std::array<qint64, 3> parts{};
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        parts[i] = m.captured(i + 1).toLongLong(&ok);
        if (!ok) return std::nullopt;
    }
    return parts;
}

} // namespace

UpdateCheck::UpdateCheck(AppState *state, const QString &currentVersion, QObject *parent)
    : QObject(parent), m_state(state), m_current(currentVersion)
{
}

QDateTime UpdateCheck::lastChecked() const
{
    const qint64 at = m_state->updateLastChecked();
    return at > 0 ? QDateTime::fromSecsSinceEpoch(at) : QDateTime();
}

QString UpdateCheck::releasesUrl() const
{
    return "https://github.com/huseyincancalti/EchoPlay/releases/latest";
}

bool UpdateCheck::isNewer(const QString &a, const QString &b)
{
    const auto pa = parseSemver(a);
    const auto pb = parseSemver(b);
    return pa && pb && *pa > *pb; // std::array compares lexicographically: major, then minor, then patch
}

bool UpdateCheck::due(qint64 nowEpochSeconds) const
{
    if (m_current == DevVersion) return false; // nothing meaningful to compare against, and it would nag
    const qint64 last = m_state->updateLastChecked();
    return last == 0 || nowEpochSeconds - last >= CheckIntervalSeconds;
}

void UpdateCheck::start()
{
    if (due(QDateTime::currentSecsSinceEpoch())) check();
}

void UpdateCheck::checkNow()
{
    if (m_current != DevVersion) check();
}

void UpdateCheck::check()
{
    if (m_reply) return; // one request at a time
    // The time is stored before the request, like v1: a check that fails still counts as today's attempt.
    m_state->setUpdateLastChecked(QDateTime::currentSecsSinceEpoch());

    QNetworkRequest request(QUrl("https://api.github.com/repos/huseyincancalti/EchoPlay/releases/latest"));
    request.setRawHeader("User-Agent", "EchoPlay");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(10000);
    m_reply = m_net.get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = std::exchange(m_reply, nullptr);
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) handleReply(reply->readAll());
        emit changed(); // checking is false again, lastChecked moved
    });
    emit changed();
}

void UpdateCheck::handleReply(const QByteArray &json)
{
    const QJsonValue tag = QJsonDocument::fromJson(json).object().value("tag_name");
    if (!tag.isString()) return;
    QString latest = tag.toString();
    if (latest.startsWith('v')) latest.remove(0, 1);

    m_latest = latest;
    m_updateAvailable = isNewer(latest, m_current);
    emit changed();
    if (m_updateAvailable && latest != m_state->updateSeenVersion()) {
        m_state->setUpdateSeenVersion(latest);
        emit newVersionFound(latest);
    }
}
