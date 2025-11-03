#include "MessageManager.h"
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>

#ifndef Q_OS_WIN
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusMetaType>
#endif

// ========== MessageModel Implementation ==========

MessageModel::MessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_messages.count();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.count())
        return QVariant();

    const Message &msg = m_messages.at(index.row());

    switch (role) {
    case IdRole:
        return msg.id;
    case ThreadIdRole:
        return msg.threadId;
    case SenderRole:
        return msg.sender;
    case SenderAddressRole:
        return msg.senderAddress;
    case BodyRole:
        return msg.body;
    case TimestampRole:
        return msg.timestamp;
    case IsIncomingRole:
        return msg.isIncoming;
    case IsReadRole:
        return msg.isRead;
    case TypeRole:
        return msg.type;
    case FormattedTimeRole:
        return formatTimestamp(msg.timestamp);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "messageId";
    roles[ThreadIdRole] = "threadId";
    roles[SenderRole] = "sender";
    roles[SenderAddressRole] = "senderAddress";
    roles[BodyRole] = "body";
    roles[TimestampRole] = "timestamp";
    roles[IsIncomingRole] = "isIncoming";
    roles[IsReadRole] = "isRead";
    roles[TypeRole] = "type";
    roles[FormattedTimeRole] = "formattedTime";
    return roles;
}

void MessageModel::addMessage(const Message &message)
{
    beginInsertRows(QModelIndex(), m_messages.count(), m_messages.count());
    m_messages.append(message);
    endInsertRows();
}

void MessageModel::updateMessage(const QString &id, const Message &message)
{
    for (int i = 0; i < m_messages.count(); ++i) {
        if (m_messages[i].id == id) {
            m_messages[i] = message;
            QModelIndex modelIndex = index(i);
            emit dataChanged(modelIndex, modelIndex);
            return;
        }
    }
}

void MessageModel::removeMessage(const QString &id)
{
    for (int i = 0; i < m_messages.count(); ++i) {
        if (m_messages[i].id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            m_messages.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

void MessageModel::clear()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
}

void MessageModel::sortMessagesByTime()
{
    beginResetModel();
    std::sort(m_messages.begin(), m_messages.end(), [](const Message &a, const Message &b) {
        return a.timestamp < b.timestamp;
    });
    endResetModel();
}

Message* MessageModel::findMessage(const QString &id)
{
    for (int i = 0; i < m_messages.count(); ++i) {
        if (m_messages[i].id == id) {
            return &m_messages[i];
        }
    }
    return nullptr;
}

QString MessageModel::formatTimestamp(const QDateTime &dt) const
{
    if (!dt.isValid())
        return "";

    QDateTime now = QDateTime::currentDateTime();
    qint64 secs = dt.secsTo(now);

    if (secs < 60)
        return "Just now";
    else if (secs < 3600)
        return QString::number(secs / 60) + "m ago";
    else if (secs < 86400)
        return QString::number(secs / 3600) + "h ago";
    else if (dt.date() == now.date().addDays(-1))
        return "Yesterday";
    else if (secs < 604800)
        return dt.toString("dddd");
    else
        return dt.toString("MMM d");
}

// ========== ConversationModel Implementation ==========

ConversationModel::ConversationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConversationModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_conversations.count();
}

QVariant ConversationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_conversations.count())
        return QVariant();

    const Conversation &conv = m_conversations.at(index.row());

    switch (role) {
    case ThreadIdRole:
        return conv.threadId;
    case ContactNameRole:
        return conv.contactName;
    case ContactAddressRole:
        return conv.contactAddress;
    case LastMessageBodyRole:
        return conv.lastMessageBody;
    case LastMessageTimeRole:
        return conv.lastMessageTime;
    case UnreadCountRole:
        return conv.unreadCount;
    case IsPinnedRole:
        return conv.isPinned;
    case FormattedTimeRole:
        return formatTimestamp(conv.lastMessageTime);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ConversationModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ThreadIdRole] = "threadId";
    roles[ContactNameRole] = "contactName";
    roles[ContactAddressRole] = "contactAddress";
    roles[LastMessageBodyRole] = "lastMessageBody";
    roles[LastMessageTimeRole] = "lastMessageTime";
    roles[UnreadCountRole] = "unreadCount";
    roles[IsPinnedRole] = "isPinned";
    roles[FormattedTimeRole] = "formattedTime";
    return roles;
}

void ConversationModel::addConversation(const Conversation &conv)
{
    beginInsertRows(QModelIndex(), m_conversations.count(), m_conversations.count());
    m_conversations.append(conv);
    endInsertRows();
}

void ConversationModel::updateConversation(const QString &threadId, const Conversation &conv)
{
    for (int i = 0; i < m_conversations.count(); ++i) {
        if (m_conversations[i].threadId == threadId) {
            m_conversations[i] = conv;
            QModelIndex modelIndex = index(i);
            emit dataChanged(modelIndex, modelIndex);
            return;
        }
    }
}

void ConversationModel::removeConversation(const QString &threadId)
{
    for (int i = 0; i < m_conversations.count(); ++i) {
        if (m_conversations[i].threadId == threadId) {
            beginRemoveRows(QModelIndex(), i, i);
            m_conversations.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

void ConversationModel::clear()
{
    beginResetModel();
    m_conversations.clear();
    endResetModel();
}

void ConversationModel::sortConversations()
{
    beginResetModel();
    std::sort(m_conversations.begin(), m_conversations.end(), [](const Conversation &a, const Conversation &b) {
        // Pinned conversations first
        if (a.isPinned != b.isPinned)
            return a.isPinned;
        // Then by timestamp (most recent first)
        return a.lastMessageTime > b.lastMessageTime;
    });
    endResetModel();
}

Conversation* ConversationModel::findConversation(const QString &threadId)
{
    for (int i = 0; i < m_conversations.count(); ++i) {
        if (m_conversations[i].threadId == threadId) {
            return &m_conversations[i];
        }
    }
    return nullptr;
}

QString ConversationModel::formatTimestamp(const QDateTime &dt) const
{
    if (!dt.isValid())
        return "";

    QDateTime now = QDateTime::currentDateTime();
    qint64 secs = dt.secsTo(now);

    if (secs < 60)
        return "Now";
    else if (secs < 3600)
        return QString::number(secs / 60) + "m";
    else if (secs < 86400)
        return QString::number(secs / 3600) + "h";
    else if (dt.date() == now.date().addDays(-1))
        return "Yesterday";
    else if (secs < 604800)
        return dt.toString("ddd");
    else
        return dt.toString("MM/dd");
}

// ========== MessageManager Implementation ==========

MessageManager::MessageManager(QObject *parent)
    : QObject(parent)
    , m_conversationModel(new ConversationModel(this))
    , m_messageModel(new MessageModel(this))
    , m_isConnected(false)
    , m_isSyncing(false)
    , m_totalUnreadCount(0)
#ifndef Q_OS_WIN
    , m_mapInterface(nullptr)
#endif
    , m_syncTimer(new QTimer(this))
{
    qDebug() << "MessageManager: Initializing";

    // Load cached messages
    loadMessagesCache();

    // Set up periodic sync timer (every 30 seconds when connected)
    m_syncTimer->setInterval(30000);
    connect(m_syncTimer, &QTimer::timeout, this, &MessageManager::syncMessages);

    setStatusMessage("Ready");
}

MessageManager::~MessageManager()
{
    saveMessagesCache();

#ifndef Q_OS_WIN
    if (m_mapInterface) {
        delete m_mapInterface;
    }
#endif
}

void MessageManager::setCurrentThreadId(const QString &threadId)
{
    if (m_currentThreadId != threadId) {
        m_currentThreadId = threadId;
        emit currentThreadIdChanged();

        // Load messages for this thread
        if (!threadId.isEmpty()) {
            loadConversation(threadId);
        }
    }
}

void MessageManager::connectToDevice(const QString &deviceAddress)
{
    qDebug() << "MessageManager: Connecting to device" << deviceAddress;

    m_deviceAddress = deviceAddress;
    setStatusMessage("Connecting to " + deviceAddress + "...");

#ifdef Q_OS_WIN
    // Windows mock mode - simulate connection
    QTimer::singleShot(500, this, [this]() {
        setIsConnected(true);
        setStatusMessage("Connected (Mock mode)");
        m_syncTimer->start();
    });
#else
    if (connectMAP()) {
        setIsConnected(true);
        setStatusMessage("Connected");
        m_syncTimer->start();

        // Start initial sync
        QTimer::singleShot(1000, this, &MessageManager::syncMessages);
    } else {
        setStatusMessage("Failed to connect");
    }
#endif
}

void MessageManager::disconnect()
{
    qDebug() << "MessageManager: Disconnecting";

    m_syncTimer->stop();
    setIsConnected(false);
    m_deviceAddress.clear();

#ifndef Q_OS_WIN
    if (m_mapInterface) {
        delete m_mapInterface;
        m_mapInterface = nullptr;
    }
#endif

    setStatusMessage("Disconnected");
}

void MessageManager::syncMessages()
{
    if (!m_isConnected) {
        qDebug() << "MessageManager: Not connected, skipping sync";
        return;
    }

    qDebug() << "MessageManager: Syncing messages";
    setIsSyncing(true);
    setStatusMessage("Syncing messages...");

#ifdef Q_OS_WIN
    // Windows mock - simulate sync
    QTimer::singleShot(1000, this, [this]() {
        setIsSyncing(false);
        setStatusMessage("Sync complete");
    });
#else
    pullMessageList();
#endif
}

void MessageManager::loadConversation(const QString &threadId)
{
    qDebug() << "MessageManager: Loading conversation" << threadId;

    // Clear current messages
    m_messageModel->clear();

    // Load messages from cache/server for this thread
    // For now, just filter from existing messages
    // In a full implementation, this would query the Bluetooth MAP service
}

void MessageManager::sendMessage(const QString &recipient, const QString &body)
{
    qDebug() << "MessageManager: Sending message to" << recipient << ":" << body;

    if (body.trimmed().isEmpty()) {
        emit messageSent(false, "Message body cannot be empty");
        return;
    }

#ifdef Q_OS_WIN
    // Windows mock - simulate send
    QTimer::singleShot(500, this, [this, recipient, body]() {
        // Create a new message
        Message msg;
        msg.id = QString::number(QDateTime::currentMSecsSinceEpoch());
        msg.threadId = createThreadId(recipient);
        msg.sender = "Me";
        msg.senderAddress = "self";
        msg.body = body;
        msg.timestamp = QDateTime::currentDateTime();
        msg.isIncoming = false;
        msg.isRead = true;
        msg.type = "SMS";

        // Add to model
        m_messageModel->addMessage(msg);

        // Update conversation
        updateConversationFromMessage(msg);

        emit messageSent(true, "");
    });
#else
    pushMessage(recipient, body);
#endif
}

void MessageManager::markAsRead(const QString &threadId)
{
    qDebug() << "MessageManager: Marking thread as read" << threadId;

    Conversation *conv = m_conversationModel->findConversation(threadId);
    if (conv) {
        conv->unreadCount = 0;
        m_conversationModel->updateConversation(threadId, *conv);
        updateUnreadCount();
    }
}

void MessageManager::deleteConversation(const QString &threadId)
{
    qDebug() << "MessageManager: Deleting conversation" << threadId;

    m_conversationModel->removeConversation(threadId);
    updateUnreadCount();
    saveMessagesCache();
}

void MessageManager::onMessageReceived(const QString &sender, const QString &body, const QDateTime &timestamp)
{
    qDebug() << "MessageManager: Message received from" << sender;

    Message msg;
    msg.id = QString::number(timestamp.toMSecsSinceEpoch());
    msg.threadId = createThreadId(sender);
    msg.sender = sender;
    msg.senderAddress = sender;
    msg.body = body;
    msg.timestamp = timestamp;
    msg.isIncoming = true;
    msg.isRead = false;
    msg.type = "SMS";

    // Add to model if this is the current conversation
    if (m_currentThreadId == msg.threadId) {
        m_messageModel->addMessage(msg);
    }

    // Update conversation
    updateConversationFromMessage(msg);

    emit newMessageReceived(msg.threadId, sender, body);
}

void MessageManager::updateUnreadCount()
{
    int total = 0;
    // Sum up unread counts from all conversations
    // This would need to iterate through the conversation model
    // For now, simplified
    m_totalUnreadCount = total;
    emit totalUnreadCountChanged();
}

void MessageManager::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

void MessageManager::setIsConnected(bool connected)
{
    if (m_isConnected != connected) {
        m_isConnected = connected;
        emit isConnectedChanged();
    }
}

void MessageManager::setIsSyncing(bool syncing)
{
    if (m_isSyncing != syncing) {
        m_isSyncing = syncing;
        emit isSyncingChanged();
    }
}

#ifndef Q_OS_WIN
bool MessageManager::connectMAP()
{
    qDebug() << "MessageManager: Connecting to Bluetooth MAP service";

    // Connect to org.bluez.obex.MessageAccess1 interface
    // This is a placeholder - full MAP implementation would be more complex
    m_mapInterface = new QDBusInterface(
        "org.bluez.obex",
        "/org/bluez/obex",
        "org.bluez.obex.MessageAccess1",
        QDBusConnection::sessionBus(),
        this
    );

    if (!m_mapInterface->isValid()) {
        qWarning() << "MessageManager: Failed to connect to MAP service:" << m_mapInterface->lastError().message();
        return false;
    }

    return true;
}

void MessageManager::pullMessageList()
{
    // Bluetooth MAP message list pull would go here
    // This is a simplified stub
    setIsSyncing(false);
    setStatusMessage("Messages synced");
}

void MessageManager::pullMessage(const QString &messageId)
{
    Q_UNUSED(messageId);
    // Pull individual message implementation
}

void MessageManager::pushMessage(const QString &recipient, const QString &body)
{
    Q_UNUSED(recipient);
    Q_UNUSED(body);
    // Push message implementation via MAP
    emit messageSent(false, "Bluetooth MAP send not yet implemented");
}
#else
bool MessageManager::connectMAP()
{
    return true;
}

void MessageManager::pullMessageList()
{
}

void MessageManager::pullMessage(const QString &messageId)
{
    Q_UNUSED(messageId);
}

void MessageManager::pushMessage(const QString &recipient, const QString &body)
{
    Q_UNUSED(recipient);
    Q_UNUSED(body);
}
#endif

void MessageManager::processMessageData(const QByteArray &data)
{
    Q_UNUSED(data);
    // Parse message data (vMessage format)
}

QString MessageManager::createThreadId(const QString &phoneNumber)
{
    // Normalize phone number and create consistent thread ID
    QString normalized = phoneNumber;
    normalized.remove(QRegularExpression("[^0-9+]"));
    return "thread_" + normalized;
}

void MessageManager::updateConversationFromMessage(const Message &msg)
{
    Conversation *conv = m_conversationModel->findConversation(msg.threadId);

    if (conv) {
        // Update existing conversation
        conv->lastMessageBody = msg.body;
        conv->lastMessageTime = msg.timestamp;
        if (msg.isIncoming && !msg.isRead) {
            conv->unreadCount++;
        }
        m_conversationModel->updateConversation(msg.threadId, *conv);
    } else {
        // Create new conversation
        Conversation newConv;
        newConv.threadId = msg.threadId;
        newConv.contactName = msg.sender;
        newConv.contactAddress = msg.senderAddress;
        newConv.lastMessageBody = msg.body;
        newConv.lastMessageTime = msg.timestamp;
        newConv.unreadCount = (msg.isIncoming && !msg.isRead) ? 1 : 0;
        newConv.isPinned = false;
        m_conversationModel->addConversation(newConv);
    }

    m_conversationModel->sortConversations();
    updateUnreadCount();
    saveMessagesCache();
}

void MessageManager::saveMessagesCache()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);

    QString cachePath = cacheDir + "/messages_cache.json";
    QFile file(cachePath);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "MessageManager: Failed to open cache file for writing";
        return;
    }

    // Save conversations to JSON
    // Simplified for now
    file.close();
}

void MessageManager::loadMessagesCache()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString cachePath = cacheDir + "/messages_cache.json";

    QFile file(cachePath);
    if (!file.exists()) {
        qDebug() << "MessageManager: No cached messages found";
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "MessageManager: Failed to open cache file for reading";
        return;
    }

    // Load conversations from JSON
    // Simplified for now
    file.close();
}
