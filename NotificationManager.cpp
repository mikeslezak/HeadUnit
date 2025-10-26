#include "NotificationManager.h"
#include <QDebug>
#include <QSettings>
#include <QRandomGenerator>

#ifndef Q_OS_WIN
// ANCS Service UUID: 7905F431-B5CE-4E99-A40F-4B1E122D00D0
const QBluetoothUuid NotificationManager::ANCS_SERVICE_UUID(
    QString("{7905F431-B5CE-4E99-A40F-4B1E122D00D0}"));

// ANCS Characteristic UUIDs
const QBluetoothUuid NotificationManager::ANCS_NOTIFICATION_SOURCE_UUID(
    QString("{9FBF120D-6301-42D9-8C58-25E699A21DBD}"));
const QBluetoothUuid NotificationManager::ANCS_CONTROL_POINT_UUID(
    QString("{69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9}"));
const QBluetoothUuid NotificationManager::ANCS_DATA_SOURCE_UUID(
    QString("{22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFF}"));
#endif

/**
 * CONSTRUCTOR
 */
NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
    , m_isConnected(false)
    , m_deviceAddress("")
    , m_platform("unknown")
    , m_doNotDisturb(false)
    , m_showPreviews(true)
    , m_autoDismissAfter(30)  // 30 seconds default
    , m_dismissTimer(new QTimer(this))
#ifndef Q_OS_WIN
    , m_bleController(nullptr)
    , m_ancsService(nullptr)
#endif
{
    // Default quick replies
    m_quickReplies = {
        "OK",
        "Thanks",
        "I'm driving, will respond later",
        "On my way",
        "Can't talk now",
        "Yes",
        "No"
    };

    // Setup auto-dismiss timer
    m_dismissTimer->setInterval(1000);  // Check every second
    connect(m_dismissTimer, &QTimer::timeout,
            this, &NotificationManager::onNotificationTimeout);

    // Load saved settings
    loadSettings();

#ifdef Q_OS_WIN
    // ========== MOCK MODE (Windows) ==========
    m_mockMode = true;
    qDebug() << "NotificationManager: Running in MOCK mode";

    // Simulate connection and notifications
    QTimer::singleShot(1500, this, [this]() {
        m_isConnected = true;
        m_platform = "ios";
        emit connectionChanged();
        emit platformChanged();

        // Generate mock notifications
        generateMockNotifications();
    });
#else
    // ========== REAL MODE (Embedded Linux) ==========
    m_mockMode = false;
    qDebug() << "NotificationManager: Real Bluetooth LE mode";
#endif
}

/**
 * DESTRUCTOR
 */
NotificationManager::~NotificationManager()
{
    saveSettings();

    // Clean up snooze timers
    for (QTimer *timer : m_snoozeTimers.values()) {
        timer->stop();
        delete timer;
    }
    m_snoozeTimers.clear();

#ifndef Q_OS_WIN
    if (m_ancsService) {
        delete m_ancsService;
    }
    if (m_bleController) {
        m_bleController->disconnectFromDevice();
        delete m_bleController;
    }
#endif
}

// ========================================================================
// PROPERTY GETTERS
// ========================================================================

/**
 * HAS UNREAD
 *
 * Returns true if there are any unread notifications
 */
bool NotificationManager::hasUnread() const
{
    for (const QVariant &n : m_notifications) {
        QVariantMap notif = n.toMap();
        if (!notif["read"].toBool()) {
            return true;
        }
    }
    return false;
}

// ========================================================================
// CONNECTION MANAGEMENT
// ========================================================================

/**
 * CONNECT TO DEVICE
 */
void NotificationManager::connectToDevice(const QString &deviceAddress, const QString &platform)
{
    m_deviceAddress = deviceAddress;
    m_platform = platform.toLower();
    emit platformChanged();

#ifdef Q_OS_WIN
    // Mock mode - already connected
    m_isConnected = true;
    emit connectionChanged();
    qDebug() << "Notifications connected (mock):" << m_platform;
#else
    // Real mode - connect to ANCS (iOS) or Android service

    if (m_platform == "ios") {
        // Connect to ANCS via Bluetooth LE
        QBluetoothAddress address(deviceAddress);

        if (m_bleController) {
            m_bleController->deleteLater();
        }

        // Qt 6.2 compatibility: BLE API changed, needs QBluetoothDeviceInfo
        // TODO: Implement device discovery first, then connect
        qWarning() << "BLE notifications require Qt 6.3+ or device discovery implementation";
        m_bleController = nullptr;
        // m_bleController = QLowEnergyController::createCentral(address, this);

        connect(m_bleController, &QLowEnergyController::serviceDiscovered,
                this, &NotificationManager::onRemoteServiceDiscovered);
        connect(m_bleController, &QLowEnergyController::connected,
                this, [this]() {
                    qDebug() << "BLE connected, discovering services...";
                    m_bleController->discoverServices();
                });
        connect(m_bleController, &QLowEnergyController::disconnected,
                this, [this]() {
                    m_isConnected = false;
                    emit connectionChanged();
                });

        m_bleController->connectToDevice();

    } else if (m_platform == "android") {
        // Android uses different protocol
        // Typically requires companion app on phone
        qDebug() << "Android notification support requires companion app";
        emit error("Android notifications require companion app");
    }
#endif
}

/**
 * DISCONNECT
 */
void NotificationManager::disconnect()
{
#ifdef Q_OS_WIN
    m_isConnected = false;
    emit connectionChanged();
#else
    if (m_bleController) {
        m_bleController->disconnectFromDevice();
    }
    m_isConnected = false;
    emit connectionChanged();
#endif
}

// ========================================================================
// NOTIFICATION ACTIONS
// ========================================================================

/**
 * DISMISS NOTIFICATION
 */
void NotificationManager::dismissNotification(const QString &notificationId)
{
    qDebug() << "Dismissing notification:" << notificationId;

#ifdef Q_OS_WIN
    // Mock mode - just remove from list
    removeNotification(notificationId);
#else
    // Real mode - send dismiss command to phone
    if (m_platform == "ios") {
        sendAncsCommand(2, notificationId);  // Command ID 2 = Perform Action (Dismiss)
    }
    removeNotification(notificationId);
#endif

    emit notificationDismissed(notificationId);
}

/**
 * DISMISS ALL
 */
void NotificationManager::dismissAll()
{
    qDebug() << "Dismissing all notifications";

    // Get list of IDs before clearing
    QStringList ids;
    for (const QVariant &notif : m_notifications) {
        QVariantMap n = notif.toMap();
        ids.append(n["id"].toString());
    }

    // Dismiss each one
    for (const QString &id : ids) {
        dismissNotification(id);
    }
}

/**
 * MARK AS READ
 */
void NotificationManager::markAsRead(const QString &notificationId)
{
    qDebug() << "Marking as read:" << notificationId;

    // Find and update notification
    for (int i = 0; i < m_notifications.size(); ++i) {
        QVariantMap notif = m_notifications[i].toMap();
        if (notif["id"].toString() == notificationId) {
            notif["read"] = true;
            m_notifications[i] = notif;
            emit notificationsChanged();
            emit notificationUpdated(notificationId);
            emit hasUnreadChanged();
            break;
        }
    }

#ifndef Q_OS_WIN
    // Send to phone (if supported)
    if (m_platform == "ios") {
        // ANCS doesn't have explicit "mark as read", but can dismiss
    }
#endif
}

/**
 * REPLY TO NOTIFICATION
 */
void NotificationManager::replyToNotification(const QString &notificationId, const QString &message)
{
    qDebug() << "Replying to" << notificationId << "with:" << message;

#ifdef Q_OS_WIN
    // Mock mode - simulate reply
    QVariantMap reply{
        {"id", "reply_" + notificationId},
        {"appId", "com.apple.MobileSMS"},
        {"appName", "Messages"},
        {"title", "You"},
        {"message", message},
        {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)},
        {"category", (int)Other},
        {"priority", (int)Silent},
        {"read", true}
    };

    addNotification(reply);
    dismissNotification(notificationId);
    emit replySent(notificationId, message);
#else
    // Real mode - send reply via ANCS or companion app
    if (m_platform == "ios") {
        // iOS reply requires iOS 9+ and specific action IDs
        // This is complex and may not be supported on all apps
        qDebug() << "iOS reply via ANCS (limited support)";
    }
    emit replySent(notificationId, message);
#endif
}

/**
 * OPEN NOTIFICATION
 */
void NotificationManager::openNotification(const QString &notificationId)
{
    qDebug() << "Opening notification on phone:" << notificationId;

#ifndef Q_OS_WIN
    if (m_platform == "ios") {
        sendAncsCommand(0, notificationId);  // Command ID 0 = Positive Action (Open)
    }
#endif

    // Remove from head unit after opening
    removeNotification(notificationId);
}

/**
 * SNOOZE NOTIFICATION
 */
void NotificationManager::snoozeNotification(const QString &notificationId, int minutes)
{
    qDebug() << "Snoozing notification" << notificationId << "for" << minutes << "minutes";

    // Find notification
    QVariantMap notification;
    for (const QVariant &n : m_notifications) {
        QVariantMap notif = n.toMap();
        if (notif["id"].toString() == notificationId) {
            notification = notif;
            break;
        }
    }

    if (notification.isEmpty()) {
        qWarning() << "Notification not found for snooze";
        return;
    }

    // Remove from active list
    removeNotification(notificationId);

    // Create snooze timer
    QTimer *snoozeTimer = new QTimer(this);
    snoozeTimer->setSingleShot(true);
    snoozeTimer->setInterval(minutes * 60 * 1000);

    connect(snoozeTimer, &QTimer::timeout, this, [this, notification]() {
        // Re-add notification after snooze
        QVariantMap snoozed = notification;
        snoozed["snoozed"] = true;
        addNotification(snoozed);

        // Clean up timer
        QString id = notification["id"].toString();
        if (m_snoozeTimers.contains(id)) {
            m_snoozeTimers[id]->deleteLater();
            m_snoozeTimers.remove(id);
        }
    });

    m_snoozeTimers[notificationId] = snoozeTimer;
    snoozeTimer->start();
}

/**
 * PERFORM ACTION
 */
void NotificationManager::performAction(const QString &notificationId, const QString &actionId)
{
    qDebug() << "Performing action" << actionId << "on" << notificationId;

#ifndef Q_OS_WIN
    // Send action to phone
    // Action IDs are app-specific
#endif
}

// ========================================================================
// FILTERING & SETTINGS
// ========================================================================

/**
 * SET DO NOT DISTURB
 */
void NotificationManager::setDoNotDisturb(bool enabled)
{
    if (m_doNotDisturb != enabled) {
        m_doNotDisturb = enabled;
        emit doNotDisturbChanged();
        saveSettings();

        qDebug() << "Do Not Disturb:" << (enabled ? "enabled" : "disabled");

        // Clear non-urgent notifications when enabling DND
        if (enabled) {
            QStringList toRemove;
            for (const QVariant &n : m_notifications) {
                QVariantMap notif = n.toMap();
                int priority = notif["priority"].toInt();
                if (priority != Urgent) {
                    toRemove.append(notif["id"].toString());
                }
            }
            for (const QString &id : toRemove) {
                removeNotification(id);
            }
        }
    }
}

/**
 * SET ALLOWED APPS
 */
void NotificationManager::setAllowedApps(const QStringList &apps)
{
    m_allowedApps = apps;
    emit allowedAppsChanged();
    saveSettings();
}

/**
 * SET BLOCKED APPS
 */
void NotificationManager::setBlockedApps(const QStringList &apps)
{
    m_blockedApps = apps;
    emit blockedAppsChanged();
    saveSettings();
}

/**
 * ALLOW APP
 */
void NotificationManager::allowApp(const QString &appId)
{
    if (!m_allowedApps.contains(appId)) {
        m_allowedApps.append(appId);
        emit allowedAppsChanged();
        saveSettings();
    }

    // Remove from blocked if present
    if (m_blockedApps.contains(appId)) {
        m_blockedApps.removeAll(appId);
        emit blockedAppsChanged();
    }
}

/**
 * BLOCK APP
 */
void NotificationManager::blockApp(const QString &appId)
{
    if (!m_blockedApps.contains(appId)) {
        m_blockedApps.append(appId);
        emit blockedAppsChanged();
        saveSettings();
    }

    // Remove from allowed if present
    if (m_allowedApps.contains(appId)) {
        m_allowedApps.removeAll(appId);
        emit allowedAppsChanged();
    }

    // Remove existing notifications from this app
    QStringList toRemove;
    for (const QVariant &n : m_notifications) {
        QVariantMap notif = n.toMap();
        if (notif["appId"].toString() == appId) {
            toRemove.append(notif["id"].toString());
        }
    }
    for (const QString &id : toRemove) {
        removeNotification(id);
    }
}

/**
 * SET SHOW PREVIEWS
 */
void NotificationManager::setShowPreviews(bool enabled)
{
    if (m_showPreviews != enabled) {
        m_showPreviews = enabled;
        emit showPreviewsChanged();
        saveSettings();
    }
}

/**
 * SET AUTO DISMISS AFTER
 */
void NotificationManager::setAutoDismissAfter(int seconds)
{
    m_autoDismissAfter = seconds;
    emit autoDismissAfterChanged();
    saveSettings();

    if (seconds > 0) {
        m_dismissTimer->start();
    } else {
        m_dismissTimer->stop();
    }
}

// ========================================================================
// NOTIFICATION QUERIES
// ========================================================================

/**
 * GET NOTIFICATION
 */
QVariantMap NotificationManager::getNotification(const QString &notificationId) const
{
    for (const QVariant &n : m_notifications) {
        QVariantMap notif = n.toMap();
        if (notif["id"].toString() == notificationId) {
            return notif;
        }
    }
    return QVariantMap();
}

/**
 * GET NOTIFICATIONS FROM APP
 */
QVariantList NotificationManager::getNotificationsFromApp(const QString &appId) const
{
    QVariantList result;
    for (const QVariant &n : m_notifications) {
        QVariantMap notif = n.toMap();
        if (notif["appId"].toString() == appId) {
            result.append(notif);
        }
    }
    return result;
}

/**
 * GET NOTIFICATIONS BY CATEGORY
 */
QVariantList NotificationManager::getNotificationsByCategory(NotificationCategory category) const
{
    QVariantList result;
    for (const QVariant &n : m_notifications) {
        QVariantMap notif = n.toMap();
        if (notif["category"].toInt() == category) {
            result.append(notif);
        }
    }
    return result;
}

/**
 * GET NOTIFICATION HISTORY
 */
QVariantList NotificationManager::getNotificationHistory() const
{
    return m_history;
}

/**
 * CLEAR HISTORY
 */
void NotificationManager::clearHistory()
{
    m_history.clear();
    qDebug() << "Notification history cleared";
}

// ========================================================================
// QUICK REPLIES
// ========================================================================

/**
 * GET QUICK REPLIES
 */
QStringList NotificationManager::getQuickReplies() const
{
    return m_quickReplies;
}

/**
 * SET QUICK REPLIES
 */
void NotificationManager::setQuickReplies(const QStringList &replies)
{
    m_quickReplies = replies;
    emit quickRepliesChanged();
    saveSettings();
}

/**
 * SEND QUICK REPLY
 */
void NotificationManager::sendQuickReply(const QString &notificationId, int replyIndex)
{
    if (replyIndex < 0 || replyIndex >= m_quickReplies.size()) {
        qWarning() << "Invalid quick reply index";
        return;
    }

    QString reply = m_quickReplies.at(replyIndex);
    replyToNotification(notificationId, reply);
}

// ========================================================================
// HELPER METHODS
// ========================================================================

/**
 * ADD NOTIFICATION
 */
void NotificationManager::addNotification(const QVariantMap &notification)
{
    QString appId = notification["appId"].toString();
    int priority = notification["priority"].toInt();

    // Check if app is allowed
    if (!isAppAllowed(appId)) {
        qDebug() << "Notification blocked from app:" << appId;
        return;
    }

    // Check if should show based on DND
    if (!shouldShowNotification((NotificationPriority)priority)) {
        qDebug() << "Notification suppressed by DND mode";
        return;
    }

    // Add to list
    m_notifications.prepend(notification);
    emit notificationsChanged();
    emit notificationCountChanged();
    emit hasUnreadChanged();
    emit notificationReceived(notification);

    // Check if urgent
    if (priority == Urgent) {
        emit urgentNotification(notification);
    }

    qDebug() << "Notification added:" << notification["title"].toString();
}

/**
 * REMOVE NOTIFICATION
 */
void NotificationManager::removeNotification(const QString &notificationId)
{
    for (int i = 0; i < m_notifications.size(); ++i) {
        QVariantMap notif = m_notifications[i].toMap();
        if (notif["id"].toString() == notificationId) {
            // Move to history
            notif["dismissedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            m_history.prepend(notif);

            // Limit history size
            while (m_history.size() > 100) {
                m_history.removeLast();
            }

            // Remove from active
            m_notifications.removeAt(i);
            emit notificationsChanged();
            emit notificationCountChanged();
            emit hasUnreadChanged();
            return;
        }
    }
}

/**
 * IS APP ALLOWED
 */
bool NotificationManager::isAppAllowed(const QString &appId) const
{
    // Check blocked list first
    if (m_blockedApps.contains(appId)) {
        return false;
    }

    // If allowed list is empty, all apps are allowed
    if (m_allowedApps.isEmpty()) {
        return true;
    }

    // Otherwise, must be in allowed list
    return m_allowedApps.contains(appId);
}

/**
 * SHOULD SHOW NOTIFICATION
 */
bool NotificationManager::shouldShowNotification(NotificationPriority priority) const
{
    if (!m_doNotDisturb) {
        return true;  // Show all when DND is off
    }

    // In DND mode, only show urgent
    return (priority == Urgent);
}

/**
 * ON NOTIFICATION TIMEOUT
 */
void NotificationManager::onNotificationTimeout()
{
    if (m_autoDismissAfter <= 0) {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QStringList toRemove;

    for (const QVariant &n : m_notifications) {
        QVariantMap notif = n.toMap();
        QDateTime timestamp = QDateTime::fromString(notif["timestamp"].toString(), Qt::ISODate);

        qint64 age = timestamp.secsTo(now);
        if (age >= m_autoDismissAfter) {
            toRemove.append(notif["id"].toString());
        }
    }

    for (const QString &id : toRemove) {
        qDebug() << "Auto-dismissing old notification:" << id;
        removeNotification(id);
    }
}

// ========================================================================
// SETTINGS PERSISTENCE
// ========================================================================

/**
 * LOAD SETTINGS
 */
void NotificationManager::loadSettings()
{
    QSettings settings;

    m_doNotDisturb = settings.value("notifications/doNotDisturb", false).toBool();
    m_allowedApps = settings.value("notifications/allowedApps").toStringList();
    m_blockedApps = settings.value("notifications/blockedApps").toStringList();
    m_showPreviews = settings.value("notifications/showPreviews", true).toBool();
    m_autoDismissAfter = settings.value("notifications/autoDismissAfter", 30).toInt();
    m_quickReplies = settings.value("notifications/quickReplies", m_quickReplies).toStringList();

    qDebug() << "Notification settings loaded";
}

/**
 * SAVE SETTINGS
 */
void NotificationManager::saveSettings()
{
    QSettings settings;

    settings.setValue("notifications/doNotDisturb", m_doNotDisturb);
    settings.setValue("notifications/allowedApps", m_allowedApps);
    settings.setValue("notifications/blockedApps", m_blockedApps);
    settings.setValue("notifications/showPreviews", m_showPreviews);
    settings.setValue("notifications/autoDismissAfter", m_autoDismissAfter);
    settings.setValue("notifications/quickReplies", m_quickReplies);

    qDebug() << "Notification settings saved";
}

// ========================================================================
// MOCK DATA GENERATION
// ========================================================================

/**
 * GENERATE MOCK NOTIFICATIONS
 */
void NotificationManager::generateMockNotifications()
{
    QList<QVariantMap> mockNotifs = {
        {
            {"id", "notif_1"},
            {"appId", "com.whatsapp"},
            {"appName", "WhatsApp"},
            {"title", "Mom"},
            {"message", "Don't forget dinner tonight!"},
            {"timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)},
            {"category", (int)Social},
            {"priority", (int)Normal},
            {"read", false}
        },
        {
            {"id", "notif_2"},
            {"appId", "com.google.Gmail"},
            {"appName", "Gmail"},
            {"title", "New Email"},
            {"message", "Meeting reminder: Team sync at 3 PM"},
            {"timestamp", QDateTime::currentDateTime().addSecs(-300).toString(Qt::ISODate)},
            {"category", (int)Email},
            {"priority", (int)Normal},
            {"read", false}
        },
        {
            {"id", "notif_3"},
            {"appId", "com.apple.mobilecal"},
            {"appName", "Calendar"},
            {"title", "Event in 15 minutes"},
            {"message", "Dentist Appointment"},
            {"timestamp", QDateTime::currentDateTime().addSecs(-60).toString(Qt::ISODate)},
            {"category", (int)Schedule},
            {"priority", (int)Urgent},
            {"read", false}
        }
    };

    for (const QVariantMap &notif : mockNotifs) {
        addNotification(notif);
    }

    qDebug() << "Generated" << mockNotifs.size() << "mock notifications";
}

#ifndef Q_OS_WIN
// ========================================================================
// BLUETOOTH LE / ANCS (iOS) - Real Mode Only
// ========================================================================

void NotificationManager::onRemoteServiceDiscovered(const QBluetoothUuid &uuid)
{
    if (uuid == ANCS_SERVICE_UUID) {
        qDebug() << "ANCS service discovered!";

        m_ancsService = m_bleController->createServiceObject(ANCS_SERVICE_UUID, this);

        if (!m_ancsService) {
            qWarning() << "Failed to create ANCS service object";
            return;
        }

        connect(m_ancsService, &QLowEnergyService::stateChanged,
                this, &NotificationManager::onServiceStateChanged);
        connect(m_ancsService, &QLowEnergyService::characteristicChanged,
                this, &NotificationManager::onCharacteristicChanged);
        connect(m_ancsService, &QLowEnergyService::characteristicRead,
                this, &NotificationManager::onCharacteristicRead);

        m_ancsService->discoverDetails();
    }
}

void NotificationManager::onServiceStateChanged(QLowEnergyService::ServiceState state)
{
    if (state == QLowEnergyService::RemoteServiceDiscovered) {
        qDebug() << "ANCS service details discovered";

        // Get characteristics
        m_notificationSource = m_ancsService->characteristic(ANCS_NOTIFICATION_SOURCE_UUID);
        m_controlPoint = m_ancsService->characteristic(ANCS_CONTROL_POINT_UUID);
        m_dataSource = m_ancsService->characteristic(ANCS_DATA_SOURCE_UUID);

        // Enable notifications on Notification Source
        if (m_notificationSource.isValid()) {
            QLowEnergyDescriptor notificationDesc = m_notificationSource.descriptor(
                QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);

            if (notificationDesc.isValid()) {
                m_ancsService->writeDescriptor(notificationDesc, QByteArray::fromHex("0100"));
                m_isConnected = true;
                emit connectionChanged();
                qDebug() << "ANCS notifications enabled";
            }
        }
    }
}

void NotificationManager::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic,
                                                  const QByteArray &value)
{
    if (characteristic.uuid() == ANCS_NOTIFICATION_SOURCE_UUID) {
        // Parse ANCS notification
        QVariantMap notification = parseAncsNotification(value);
        if (!notification.isEmpty()) {
            addNotification(notification);
        }
    }
}

void NotificationManager::onCharacteristicRead(const QLowEnergyCharacteristic &characteristic,
                                               const QByteArray &value)
{
    qDebug() << "Characteristic read:" << characteristic.uuid() << value.toHex();
}

QVariantMap NotificationManager::parseAncsNotification(const QByteArray &data)
{
    if (data.size() < 8) {
        qWarning() << "Invalid ANCS notification data";
        return QVariantMap();
    }

    QVariantMap notification;

    // Parse ANCS notification format
    quint8 eventId = data[0];
    quint8 eventFlags = data[1];
    quint8 categoryId = data[2];
    quint8 categoryCount = data[3];
    quint32 notificationUID = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + 4));

    notification["id"] = QString("ancs_%1").arg(notificationUID);
    notification["category"] = (int)categoryId;
    notification["priority"] = (eventFlags & 0x02) ? Urgent : Normal;  // Flag 0 = Silent
    notification["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    notification["read"] = false;

    // Request full notification details
    // This requires sending command to Data Source characteristic
    // For now, return basic notification
    notification["appName"] = "Unknown App";
    notification["appId"] = "unknown";
    notification["title"] = "New Notification";
    notification["message"] = "Tap for details";

    return notification;
}

void NotificationManager::sendAncsCommand(quint8 command, const QString &notificationId)
{
    if (!m_controlPoint.isValid()) {
        qWarning() << "Control point not available";
        return;
    }

    // Extract UID from notification ID
    bool ok;
    quint32 uid = notificationId.mid(5).toUInt(&ok);  // Remove "ancs_" prefix
    if (!ok) {
        qWarning() << "Invalid notification ID format";
        return;
    }

    // Build command packet
    QByteArray packet;
    packet.append(command);  // Command ID
    packet.append(reinterpret_cast<const char*>(&uid), 4);  // Notification UID

    m_ancsService->writeCharacteristic(m_controlPoint, packet);

    qDebug() << "Sent ANCS command:" << command << "for UID:" << uid;
}

#endif
