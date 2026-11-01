#include "UpdateCheck.h"

#include <QJsonArray>
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

constexpr auto DevVersion = "0.0.0-dev";
constexpr auto SetupAssetName = "EchoPlay-Setup.exe";
constexpr int MaxNotes = 4000; // characters kept in state.json: a small card, not an archive
// The only place a setup is downloaded from: this repo's releases.
constexpr auto SetupUrlPrefix = "https://github.com/huseyincancalti/EchoPlay/releases/download/";

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

void UpdateCheck::start()
{
    checkNow();
}

void UpdateCheck::checkNow()
{
    if (m_current != DevVersion) check(); // nothing meaningful to compare against, and it would nag
}

void UpdateCheck::check()
{
    if (m_reply) return; // one request at a time
    // The time is stored before the request: a check that fails still shows when it was tried.
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
    const QJsonObject release = QJsonDocument::fromJson(json).object();
    const QJsonValue tag = release.value("tag_name");
    if (!tag.isString()) return;
    QString latest = tag.toString();
    if (latest.startsWith('v')) latest.remove(0, 1);

    m_latest = latest;
    m_updateAvailable = isNewer(latest, m_current);
    readSetup(release.value("assets").toArray());
    // Not over the running version's own notes while they wait to be shown: a hotfix found a few seconds after an update
    // would take them away unseen.
    if (m_updateAvailable && m_state->notesVersion() != m_current) {
        // Saved now: the new version shows them on its first start, which may have no network. The card has its own
        // title, so a heading on top ("## What's new in v2.1.0") goes.
        static const QRegularExpression heading("^#[^\n]*");
        QString notes = release.value("body").toString().trimmed();
        notes.remove(heading);
        m_state->setReleaseNotes(latest, notes.trimmed().left(MaxNotes));
    }
    emit changed();
    if (m_updateAvailable) emit newVersionFound(latest);
}

QString UpdateCheck::whatsNew() const
{
    return m_state->notesVersion() == m_current ? m_state->notesText() : QString();
}

void UpdateCheck::closeWhatsNew()
{
    m_state->setReleaseNotes(QString(), QString());
    emit changed();
}

// The setup is the asset named exactly EchoPlay-Setup.exe. A foreign download host or a missing or odd
// digest leaves the matching field empty; nothing from an earlier reply stays.
void UpdateCheck::readSetup(const QJsonArray &assets)
{
    m_setupUrl.clear();
    m_setupSha256.clear();
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        if (asset.value("name").toString() != SetupAssetName) continue;
        const QString url = asset.value("browser_download_url").toString();
        if (!url.startsWith(SetupUrlPrefix)) return;
        m_setupUrl = url;
        static const QRegularExpression digest("^sha256:([0-9a-fA-F]{64})$");
        const QRegularExpressionMatch m = digest.match(asset.value("digest").toString());
        if (m.hasMatch()) m_setupSha256 = m.captured(1).toLower();
        return;
    }
}
