#include "Updater.h"

#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <utility>

Updater::Updater(const QString &downloadDir, std::function<bool(const QString &setupPath)> launch,
                 QObject *parent)
    : QObject(parent), m_dir(downloadDir), m_launch(std::move(launch))
{
}

Updater::~Updater()
{
    discard(); // an app closed mid-download leaves no half file
}

void Updater::install(const QString &url, const QString &sha256, const QString &version)
{
    if (m_state == "downloading" || m_state == "installing") return;
    const bool waiting = m_state == "ready";
    m_onClose = false;
    if (waiting) launch();
    else download(url, sha256, version);
}

void Updater::installOnClose(const QString &url, const QString &sha256, const QString &version)
{
    if (m_state == "downloading" || m_state == "installing" || m_state == "ready") return;
    m_onClose = true;
    download(url, sha256, version);
}

void Updater::download(const QString &url, const QString &sha256, const QString &version)
{
    m_received = 0;
    m_total = 0;
    m_sha256 = sha256;
    m_hash.reset();

    // The version comes from a release tag and ends up in a file name: plain characters only.
    static const QRegularExpression plain("^[\\w.+-]+$");
    if (!plain.match(version).hasMatch()) {
        fail();
        return;
    }
    m_setupPath = m_dir + "/EchoPlay-Setup-" + version + ".exe";
    m_file = std::make_unique<QFile>(m_setupPath + ".part");
    if (!QDir().mkpath(m_dir) || !m_file->open(QIODevice::WriteOnly)) {
        fail();
        return;
    }

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "EchoPlay");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    // No limit on the whole download (about 50 MB on a slow line), but a transfer that stalls fails.
    request.setTransferTimeout(30000);
    m_reply = m_net.get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (!store()) fail();
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64, qint64 total) {
        if (total > 0 && total != m_total) {
            m_total = total;
            emit changed();
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, &Updater::finish);
    setState("downloading");
}

void Updater::cancel()
{
    if (m_state != "downloading") return;
    discard();
    m_onClose = false;
    setState("idle");
}

void Updater::setState(const QString &state)
{
    if (state == m_state) return;
    m_state = state;
    emit changed();
}

// Streams what has arrived to the .part file and the hash: the setup is never held in memory.
// False when the disk write failed.
bool Updater::store()
{
    const QByteArray chunk = m_reply->readAll();
    if (chunk.isEmpty()) return true;
    if (m_file->write(chunk) != chunk.size()) return false;
    m_hash.addData(chunk);
    m_received += chunk.size();
    emit changed();
    return true;
}

void Updater::finish()
{
    if (!store() || m_reply->error() != QNetworkReply::NoError || m_received == 0
        || (!m_sha256.isEmpty() && QString::fromLatin1(m_hash.result().toHex()) != m_sha256)) {
        fail();
        return;
    }
    // An older download of this version is replaced. QFile::rename closes the .part first.
    QFile::remove(m_setupPath);
    if (!m_file->rename(m_setupPath)) {
        fail();
        return;
    }
    m_file.reset();
    std::exchange(m_reply, nullptr)->deleteLater();

    if (m_onClose) setState("ready");
    else launch();
}

void Updater::launch()
{
    setState("installing");
    if (m_launch(m_setupPath)) {
        emit quitRequested();
    } else {
        QFile::remove(m_setupPath);
        fail();
    }
}

void Updater::fail()
{
    discard();
    const bool quiet = std::exchange(m_onClose, false);
    setState(quiet ? "idle" : "failed");
}

// Stops the transfer and removes the partial file.
void Updater::discard()
{
    if (m_reply) {
        m_reply->disconnect(this); // abort() would otherwise run finish() on a download that is gone
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file) {
        m_file->remove(); // closes it first
        m_file.reset();
    }
}
