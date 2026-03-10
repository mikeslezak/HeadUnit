#include "UpdateManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QDebug>

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_currentVersion(QStringLiteral(APP_VERSION))
    , m_updateAvailable(false)
    , m_isChecking(false)
    , m_isUpdating(false)
    , m_updateProgress(0)
    , m_network(new QNetworkAccessManager(this))
    , m_process(new QProcess(this))
{
    // Project dir is one level up from the build directory
    m_projectDir = QCoreApplication::applicationDirPath() + "/..";
    m_projectDir = QDir(m_projectDir).canonicalPath();

    qDebug() << "UpdateManager: project dir =" << m_projectDir;

    loadCurrentCommit();
}

void UpdateManager::loadCurrentCommit()
{
    QProcess git;
    git.setWorkingDirectory(m_projectDir);
    git.start("git", {"rev-parse", "--short", "HEAD"});
    git.waitForFinished(5000);

    if (git.exitCode() == 0) {
        m_currentCommit = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
        emit commitChanged();
        qDebug() << "UpdateManager: current commit =" << m_currentCommit;
    } else {
        m_currentCommit = "unknown";
        qWarning() << "UpdateManager: failed to get current commit";
    }
}

void UpdateManager::checkForUpdates()
{
    if (m_isChecking || m_isUpdating) return;

    m_isChecking = true;
    emit checkingChanged();
    setStatus("Checking for updates...");

    // First, fetch from remote to update refs
    QProcess *fetch = new QProcess(this);
    fetch->setWorkingDirectory(m_projectDir);

    connect(fetch, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, fetch](int exitCode, QProcess::ExitStatus) {
        fetch->deleteLater();

        if (exitCode != 0) {
            qWarning() << "UpdateManager: git fetch failed:" << fetch->readAllStandardError();
            // Fall back to GitHub API
        }

        // Check remote HEAD vs local HEAD
        QProcess *localHead = new QProcess(this);
        localHead->setWorkingDirectory(m_projectDir);
        localHead->start("git", {"rev-parse", "--short", "HEAD"});

        connect(localHead, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, localHead](int, QProcess::ExitStatus) {
            QString local = QString::fromUtf8(localHead->readAllStandardOutput()).trimmed();
            localHead->deleteLater();

            QProcess *remoteHead = new QProcess(this);
            remoteHead->setWorkingDirectory(m_projectDir);
            remoteHead->start("git", {"rev-parse", "--short", "origin/main"});

            connect(remoteHead, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, local, remoteHead](int exitCode, QProcess::ExitStatus) {
                QString remote = QString::fromUtf8(remoteHead->readAllStandardOutput()).trimmed();
                remoteHead->deleteLater();

                if (exitCode != 0 || remote.isEmpty()) {
                    setStatus("Failed to check remote");
                    m_isChecking = false;
                    emit checkingChanged();
                    return;
                }

                m_currentCommit = local;
                emit commitChanged();

                // Get the remote commit message
                QProcess *logProc = new QProcess(this);
                logProc->setWorkingDirectory(m_projectDir);
                logProc->start("git", {"log", "origin/main", "-1", "--format=%s"});

                connect(logProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, local, remote, logProc](int, QProcess::ExitStatus) {
                    m_latestMessage = QString::fromUtf8(logProc->readAllStandardOutput()).trimmed();
                    logProc->deleteLater();

                    m_latestCommit = remote;
                    m_updateAvailable = (local != remote);

                    if (m_updateAvailable) {
                        // Count commits behind
                        QProcess *countProc = new QProcess(this);
                        countProc->setWorkingDirectory(m_projectDir);
                        countProc->start("git", {"rev-list", "--count", "HEAD..origin/main"});

                        connect(countProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                                this, [this, countProc](int, QProcess::ExitStatus) {
                            QString count = QString::fromUtf8(countProc->readAllStandardOutput()).trimmed();
                            countProc->deleteLater();

                            setStatus("Update available (" + count + " new commits)");
                            m_isChecking = false;
                            emit checkingChanged();
                            emit updateInfoChanged();
                        });
                    } else {
                        setStatus("Up to date");
                        m_isChecking = false;
                        emit checkingChanged();
                        emit updateInfoChanged();
                    }
                });
            });
        });
    });

    fetch->start("git", {"fetch", "origin", "main"});
}

void UpdateManager::startUpdate()
{
    if (m_isUpdating || !m_updateAvailable) return;

    m_isUpdating = true;
    emit updatingChanged();
    setProgress(0);
    setStatus("Pulling changes...");

    runGitPull();
}

void UpdateManager::runGitPull()
{
    setProgress(10);

    QProcess *pull = new QProcess(this);
    pull->setWorkingDirectory(m_projectDir);

    connect(pull, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, pull](int exitCode, QProcess::ExitStatus) {
        pull->deleteLater();

        if (exitCode != 0) {
            QString err = QString::fromUtf8(pull->readAllStandardError());
            setStatus("Pull failed: " + err.left(100));
            m_isUpdating = false;
            emit updatingChanged();
            emit updateComplete(false);
            return;
        }

        setProgress(30);
        setStatus("Building...");
        runBuild();
    });

    pull->start("git", {"pull", "origin", "main"});
}

void UpdateManager::runBuild()
{
    QString buildDir = m_projectDir + "/build";

    // Run cmake first
    QProcess *cmake = new QProcess(this);
    cmake->setWorkingDirectory(buildDir);

    connect(cmake, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, buildDir, cmake](int exitCode, QProcess::ExitStatus) {
        cmake->deleteLater();

        if (exitCode != 0) {
            QString err = QString::fromUtf8(cmake->readAllStandardError());
            setStatus("CMake failed: " + err.left(100));
            m_isUpdating = false;
            emit updatingChanged();
            emit updateComplete(false);
            return;
        }

        setProgress(40);
        setStatus("Compiling...");

        // Run make
        QProcess *make = new QProcess(this);
        make->setWorkingDirectory(buildDir);

        // Track build progress from make output
        connect(make, &QProcess::readyReadStandardOutput, this, [this, make]() {
            QString output = QString::fromUtf8(make->readAllStandardOutput());
            // Parse percentage from make output like "[ 52%]"
            QRegularExpression re(R"(\[\s*(\d+)%\])");
            auto match = re.match(output);
            if (match.hasMatch()) {
                int pct = match.captured(1).toInt();
                // Map 0-100% build to 40-90% overall progress
                setProgress(40 + (pct * 50 / 100));
            }
        });

        connect(make, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, make](int exitCode, QProcess::ExitStatus) {
            make->deleteLater();

            if (exitCode != 0) {
                QString err = QString::fromUtf8(make->readAllStandardError());
                setStatus("Build failed: " + err.left(100));
                m_isUpdating = false;
                emit updatingChanged();
                emit updateComplete(false);
                return;
            }

            setProgress(95);
            setStatus("Build complete — restart to apply");

            // Reload current commit
            loadCurrentCommit();
            m_updateAvailable = false;
            emit updateInfoChanged();

            setProgress(100);
            m_isUpdating = false;
            emit updatingChanged();
            emit updateComplete(true);
            emit restartRequired();
        });

        // Use nproc to determine core count
        QProcess nproc;
        nproc.start("nproc", {});
        nproc.waitForFinished(2000);
        QString cores = QString::fromUtf8(nproc.readAllStandardOutput()).trimmed();
        if (cores.isEmpty()) cores = "4";

        make->start("make", {"-j" + cores});
    });

    cmake->start("cmake", {".."});
}

void UpdateManager::setStatus(const QString &msg)
{
    m_statusMessage = msg;
    qDebug() << "UpdateManager:" << msg;
    emit statusChanged();
}

void UpdateManager::setProgress(int pct)
{
    m_updateProgress = pct;
    emit progressChanged();
}

void UpdateManager::onCheckReply()
{
    // Not used in git-based flow, kept for future GitHub API usage
}
