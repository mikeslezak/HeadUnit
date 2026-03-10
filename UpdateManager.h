#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

/**
 * UpdateManager - Handles OTA updates via git pull + rebuild.
 *
 * Checks GitHub for new commits on the main branch,
 * pulls changes, rebuilds the project, and restarts the app.
 *
 * Exposed to QML for the Settings > About > Updates UI.
 */
class UpdateManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString currentCommit READ currentCommit NOTIFY commitChanged)
    Q_PROPERTY(QString latestCommit READ latestCommit NOTIFY updateInfoChanged)
    Q_PROPERTY(QString latestMessage READ latestMessage NOTIFY updateInfoChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateInfoChanged)
    Q_PROPERTY(bool isChecking READ isChecking NOTIFY checkingChanged)
    Q_PROPERTY(bool isUpdating READ isUpdating NOTIFY updatingChanged)
    Q_PROPERTY(int updateProgress READ updateProgress NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)

public:
    explicit UpdateManager(QObject *parent = nullptr);

    QString currentVersion() const { return m_currentVersion; }
    QString currentCommit() const { return m_currentCommit; }
    QString latestCommit() const { return m_latestCommit; }
    QString latestMessage() const { return m_latestMessage; }
    bool updateAvailable() const { return m_updateAvailable; }
    bool isChecking() const { return m_isChecking; }
    bool isUpdating() const { return m_isUpdating; }
    int updateProgress() const { return m_updateProgress; }
    QString statusMessage() const { return m_statusMessage; }

public slots:
    void checkForUpdates();
    void startUpdate();

signals:
    void commitChanged();
    void updateInfoChanged();
    void checkingChanged();
    void updatingChanged();
    void progressChanged();
    void statusChanged();
    void updateComplete(bool success);
    void restartRequired();

private slots:
    void onCheckReply();

private:
    void loadCurrentCommit();
    void setStatus(const QString &msg);
    void setProgress(int pct);
    void runGitPull();
    void runBuild();

    QString m_currentVersion;
    QString m_currentCommit;
    QString m_latestCommit;
    QString m_latestMessage;
    bool m_updateAvailable;
    bool m_isChecking;
    bool m_isUpdating;
    int m_updateProgress;
    QString m_statusMessage;
    QString m_projectDir;

    QNetworkAccessManager *m_network;
    QProcess *m_process;

    static constexpr const char* GITHUB_API =
        "https://api.github.com/repos/mikeslezak/HeadUnit/commits/main";
};

#endif // UPDATEMANAGER_H
