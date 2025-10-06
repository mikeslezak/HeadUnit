#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <QDateTime>
#include <QTimer>
#include <QMap>

#ifndef Q_OS_WIN
#include <QBluetoothUuid>
#include <QBluetoothAddress>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>
#endif

/**
 * NotificationManager - Phone Notification System
 *
 * Handles notifications from iPhone (ANCS) and Android phones
 */
class NotificationManager : public QObject
{
    Q_OBJECT

    // ========== PROPERTIES ==========

    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString platform READ platform NOTIFY platformChanged)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)
    Q_PROPERTY(int notificationCount READ notificationCount NOTIFY notificationCountChanged)
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY doNotDisturbChanged)
    Q_PROPERTY(QStringList allowedApps READ allowedApps WRITE setAllowedApps NOTIFY allowedAppsChanged)
    Q_PROPERTY(QStringList blockedApps READ blockedApps WRITE setBlockedApps NOTIFY blockedAppsChanged)
    Q_PROPERTY(bool showPreviews READ showPreviews WRITE setShowPreviews NOTIFY showPreviewsChanged)
    Q_PROPERTY(int autoDismissAfter READ autoDismissAfter WRITE setAutoDismissAfter NOTIFY autoDismissAfterChanged)
    Q_PROPERTY(QStringList quickReplies READ quickReplies WRITE setQuickReplies NOTIFY quickRepliesChanged)
    Q_PROPERTY(bool hasUnread READ hasUnread NOTIFY hasUnreadChanged)

public:
    // ========== NOTIFICATION CATEGORIES ==========
    enum NotificationCategory {
        Other = 0,
        IncomingCall = 1,
        MissedCall = 2,
        Voicemail = 3,
        Social = 4,
        Schedule = 5,
        Email = 6,
        News = 7,
        HealthAndFitness = 8,
        BusinessAndFinance = 9,
        Location = 10,
        Entertainment = 11
    };
    Q_ENUM(NotificationCategory)

    // ========== NOTIFICATION PRIORITY ==========
    enum NotificationPriority {
        Silent = -1,
        Low = 0,
        Normal = 1,
        High = 2,
        Urgent = 3
    };
    Q_ENUM(NotificationPriority)

    explicit NotificationManager(QObject *parent = nullptr);
    ~NotificationManager();

    // ========== PROPERTY GETTERS ==========

    bool isConnected() const { return m_isConnected; }
    QString platform() const { return m_platform; }
    QVariantList notifications() const { return m_notifications; }
    int notificationCount() const { return m_notifications.count(); }
    bool doNotDisturb() const { return m_doNotDisturb; }
    QStringList allowedApps() const { return m_allowedApps; }
    QStringList blockedApps() const { return m_blockedApps; }
    bool showPreviews() const { return m_showPreviews; }
    int autoDismissAfter() const { return m_autoDismissAfter; }
    QStringList quickReplies() const { return m_quickReplies; }
    bool hasUnread() const;

public slots:
    // ========== CONNECTION ==========

    void connectToDevice(const QString &deviceAddress, const QString &platform = "ios");
    void disconnect();

    // ========== NOTIFICATION ACTIONS ==========

    void dismissNotification(const QString &notificationId);
    void dismissAll();
    void markAsRead(const QString &notificationId);
    void openNotification(const QString &notificationId);
    void replyToNotification(const QString &notificationId, const QString &replyText);
    void snoozeNotification(const QString &notificationId, int minutes);
    void performAction(const QString &notificationId, const QString &actionId);

    // ========== QUICK REPLIES ==========

    void sendQuickReply(const QString &notificationId, int replyIndex);

    // ========== SETTINGS ==========

    void setDoNotDisturb(bool enabled);
    void setAllowedApps(const QStringList &apps);
    void setBlockedApps(const QStringList &apps);
    void allowApp(const QString &appId);
    void blockApp(const QString &appId);
    void setShowPreviews(bool enabled);
    void setAutoDismissAfter(int seconds);
    void setQuickReplies(const QStringList &replies);

    // ========== QUERIES ==========

    QVariantMap getNotification(const QString &notificationId) const;
    QVariantList getNotificationsFromApp(const QString &appId) const;
    QVariantList getNotificationsByCategory(NotificationCategory category) const;
    QVariantList getNotificationHistory() const;
    QStringList getQuickReplies() const;
    void clearHistory();

signals:
    // ========== SIGNALS ==========

    void connectionChanged();
    void platformChanged();
    void notificationsChanged();
    void notificationCountChanged();
    void hasUnreadChanged();
    void doNotDisturbChanged();
    void allowedAppsChanged();
    void blockedAppsChanged();
    void showPreviewsChanged();
    void autoDismissAfterChanged();
    void quickRepliesChanged();

    void notificationReceived(const QVariantMap &notification);
    void notificationDismissed(const QString &notificationId);
    void notificationUpdated(const QString &notificationId);
    void notificationRead(const QString &notificationId);
    void urgentNotification(const QVariantMap &notification);
    void replySent(const QString &notificationId, const QString &replyText);

    void error(const QString &message);

private slots:
    void onNotificationTimeout();

#ifndef Q_OS_WIN
    void onServiceDiscovered(const QBluetoothUuid &uuid);
    void onServiceStateChanged(QLowEnergyService::ServiceState state);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &value);
    void onCharacteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &value);
#endif

private:
    // ========== HELPER METHODS ==========

    void addNotification(const QVariantMap &notification);
    void removeNotification(const QString &notificationId);
    bool isAppAllowed(const QString &appId) const;
    bool shouldShowNotification(NotificationPriority priority) const;
    void parseAncsNotification(const QByteArray &data);
    void sendAncsCommand(quint8 commandId, const QString &notificationId);
    void loadSettings();
    void saveSettings();
    void generateMockNotifications();

    // ========== MEMBER VARIABLES ==========

    bool m_isConnected;
    QString m_deviceAddress;
    QString m_platform;  // "ios" or "android"

    QVariantList m_notifications;
    QVariantList m_history;

    bool m_doNotDisturb;
    QStringList m_allowedApps;
    QStringList m_blockedApps;
    bool m_showPreviews;
    int m_autoDismissAfter;  // seconds
    QStringList m_quickReplies;

    QTimer *m_dismissTimer;
    QMap<QString, QTimer*> m_snoozeTimers;

    bool m_mockMode;

#ifndef Q_OS_WIN
    // ========== BLUETOOTH LE (iOS ANCS) ==========

    // ANCS Service and Characteristic UUIDs
    static const QBluetoothUuid ANCS_SERVICE_UUID;
    static const QBluetoothUuid ANCS_NOTIFICATION_SOURCE_UUID;
    static const QBluetoothUuid ANCS_CONTROL_POINT_UUID;
    static const QBluetoothUuid ANCS_DATA_SOURCE_UUID;

    QLowEnergyController *m_bleController;
    QLowEnergyService *m_ancsService;
    QLowEnergyCharacteristic m_notificationSource;
    QLowEnergyCharacteristic m_controlPoint;
    QLowEnergyCharacteristic m_dataSource;
#endif
};

#endif // NOTIFICATIONMANAGER_H
