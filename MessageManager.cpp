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
#include <QDBusVariant>
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
    , m_obexClient(nullptr)
    , m_mapSession(nullptr)
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
    cleanupSession();
    if (m_obexClient) {
        delete m_obexClient;
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
    cleanupSession();
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

void MessageManager::setupOBEXClient()
{
    m_obexClient = new QDBusInterface("org.bluez.obex",
                                       "/org/bluez/obex",
                                       "org.bluez.obex.Client1",
                                       QDBusConnection::sessionBus(),
                                       this);

    if (!m_obexClient->isValid()) {
        qWarning() << "MessageManager: OBEX client not available:" << m_obexClient->lastError().message();
        m_obexClient->deleteLater();
        m_obexClient = nullptr;
    } else {
        qDebug() << "MessageManager: OBEX client connected";
    }
}

void MessageManager::cleanupSession()
{
    if (m_mapSession) {
        // Remove the OBEX session
        if (m_obexClient && !m_sessionPath.isEmpty()) {
            m_obexClient->call("RemoveSession", QDBusObjectPath(m_sessionPath));
        }
        m_mapSession->deleteLater();
        m_mapSession = nullptr;
        m_sessionPath.clear();
    }
}

bool MessageManager::connectMAP()
{
    qDebug() << "MessageManager: Connecting to Bluetooth MAP service";

    if (!m_obexClient) {
        setupOBEXClient();
    }

    if (!m_obexClient) {
        qWarning() << "MessageManager: OBEX client not available";
        return false;
    }

    // Clean up any previous session
    cleanupSession();

    // Create OBEX session with MAP target
    QVariantMap args;
    args["Target"] = "MAP";

    qDebug() << "MessageManager: Creating MAP session to" << m_deviceAddress;
    QDBusMessage msg = m_obexClient->call("CreateSession", m_deviceAddress, args);

    if (msg.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MessageManager: MAP CreateSession failed:" << msg.errorMessage();
        return false;
    }

    if (msg.arguments().isEmpty()) {
        qWarning() << "MessageManager: CreateSession returned no arguments";
        return false;
    }

    m_sessionPath = msg.arguments().at(0).value<QDBusObjectPath>().path();
    qDebug() << "MessageManager: MAP session created:" << m_sessionPath;

    // Create MessageAccess1 interface on the session
    m_mapSession = new QDBusInterface("org.bluez.obex",
                                       m_sessionPath,
                                       "org.bluez.obex.MessageAccess1",
                                       QDBusConnection::sessionBus(),
                                       this);

    if (!m_mapSession->isValid()) {
        qWarning() << "MessageManager: MAP session interface invalid:" << m_mapSession->lastError().message();
        m_mapSession->deleteLater();
        m_mapSession = nullptr;
        m_sessionPath.clear();
        return false;
    }

    qDebug() << "MessageManager: MAP connected successfully";
    return true;
}

void MessageManager::pullMessageList()
{
    if (!m_mapSession || !m_mapSession->isValid()) {
        qWarning() << "MessageManager: No MAP session for pullMessageList";
        setIsSyncing(false);
        return;
    }

    qDebug() << "MessageManager: Pulling message list from inbox";

    // Navigate to inbox: SetFolder("telecom/msg/inbox")
    QDBusMessage setFolderReply = m_mapSession->call("SetFolder", "telecom");
    if (setFolderReply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MessageManager: SetFolder(telecom) failed:" << setFolderReply.errorMessage();
        // Try root-relative path
    }
    m_mapSession->call("SetFolder", "msg");
    m_mapSession->call("SetFolder", "inbox");

    // ListMessages returns array of {object_path, properties_dict}
    QVariantMap filters;
    filters["Offset"] = QVariant::fromValue(quint16(0));
    filters["MaxCount"] = QVariant::fromValue(quint16(50));

    QDBusMessage reply = m_mapSession->call("ListMessages", "", filters);

    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MessageManager: ListMessages failed:" << reply.errorMessage();
        setIsSyncing(false);
        setStatusMessage("Sync failed: " + reply.errorMessage());
        return;
    }

    parseBMessageList(reply);

    setIsSyncing(false);
    setStatusMessage("Messages synced");
}

void MessageManager::parseBMessageList(const QDBusMessage &reply)
{
    if (reply.arguments().isEmpty()) return;

    // ListMessages returns a(oa{sv}) — array of structs {object_path, properties_dict}
    // We'll use qdbus_cast to convert the reply to a usable format
    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    int count = 0;

    arg.beginArray();
    while (!arg.atEnd()) {
        arg.beginStructure();

        QDBusObjectPath msgPath;
        arg >> msgPath;

        // Read properties dict using beginMap/endMap
        QVariantMap props;
        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            QString key;
            QDBusVariant val;
            arg >> key >> val;
            props[key] = val.variant();
            arg.endMapEntry();
        }
        arg.endMap();

        arg.endStructure();

        // Create Message from MAP properties
        Message msg;
        msg.id = msgPath.path().section('/', -1); // Use last path component as ID
        msg.sender = props.value("Sender", "").toString();
        msg.senderAddress = props.value("SenderAddress", props.value("Sender", "")).toString();
        msg.body = props.value("Subject", "").toString(); // Subject is the preview text
        msg.isRead = props.value("Read", false).toBool();
        msg.isIncoming = props.value("Type", "").toString() != "SENT";
        msg.type = props.value("Type", "SMS").toString();

        // Parse timestamp
        QString timeStr = props.value("Timestamp", "").toString();
        if (!timeStr.isEmpty()) {
            // MAP timestamp format: YYYYMMDDTHHMMSS±HHMM
            msg.timestamp = QDateTime::fromString(timeStr.left(15), "yyyyMMdd'T'HHmmss");
            if (!msg.timestamp.isValid()) {
                msg.timestamp = QDateTime::currentDateTime();
            }
        } else {
            msg.timestamp = QDateTime::currentDateTime();
        }

        // Build thread ID from sender address
        msg.threadId = createThreadId(msg.senderAddress);

        // Add to models
        if (!m_messageModel->findMessage(msg.id)) {
            m_messageModel->addMessage(msg);
            updateConversationFromMessage(msg);
            count++;
        }

        qDebug() << "MessageManager: Message from" << msg.sender
                 << "at" << msg.timestamp.toString("hh:mm")
                 << ":" << msg.body.left(50);
    }
    arg.endArray();

    if (count > 0) {
        m_messageModel->sortMessagesByTime();
        qDebug() << "MessageManager: Loaded" << count << "new messages";
    }
}

void MessageManager::pullMessage(const QString &messageId)
{
    if (!m_mapSession || !m_mapSession->isValid()) return;

    // Get full message content
    QString targetFile = "/tmp/message_" + messageId + ".bmsg";
    QVariantMap filters;
    filters["Attachment"] = true;

    QDBusMessage reply = m_mapSession->call("GetMessage", messageId, targetFile, filters);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MessageManager: GetMessage failed:" << reply.errorMessage();
        return;
    }

    // Get transfer path and monitor
    if (!reply.arguments().isEmpty()) {
        QDBusObjectPath transferPath = reply.arguments().at(0).value<QDBusObjectPath>();
        m_currentTransferPath = transferPath.path();
        m_pendingMessageHandle = messageId;

        QDBusConnection::sessionBus().connect("org.bluez.obex",
                                               m_currentTransferPath,
                                               "org.freedesktop.DBus.Properties",
                                               "PropertiesChanged",
                                               this,
                                               SLOT(onTransferPropertiesChanged(QString,QVariantMap,QStringList)));
    }
}

void MessageManager::onTransferPropertiesChanged(const QString &interface,
                                                   const QVariantMap &changed,
                                                   const QStringList &invalidated)
{
    Q_UNUSED(interface);
    Q_UNUSED(invalidated);

    if (!changed.contains("Status")) return;
    QString status = changed.value("Status").toString();

    if (status == "complete") {
        qDebug() << "MessageManager: Message transfer complete";
        if (!m_pendingMessageHandle.isEmpty()) {
            QString filePath = "/tmp/message_" + m_pendingMessageHandle + ".bmsg";
            parseBMessage(filePath, m_pendingMessageHandle);
            m_pendingMessageHandle.clear();
            QFile::remove(filePath);
        }
    } else if (status == "error") {
        qWarning() << "MessageManager: Message transfer failed";
        m_pendingMessageHandle.clear();
    }
}

void MessageManager::parseBMessage(const QString &filePath, const QString &handle)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Extract body from bMessage format
    // Body is between BEGIN:MSG and END:MSG
    QRegularExpression bodyRe("BEGIN:MSG\\r?\\n(.*)\\r?\\nEND:MSG",
                              QRegularExpression::DotMatchesEverythingOption);
    auto match = bodyRe.match(content);

    if (match.hasMatch()) {
        QString body = match.captured(1).trimmed();

        // Update the message in the model
        Message *msg = m_messageModel->findMessage(handle);
        if (msg) {
            msg->body = body;
            qDebug() << "MessageManager: Full message body:" << body.left(100);
        }
    }
}

void MessageManager::pushMessage(const QString &recipient, const QString &body)
{
    if (!m_mapSession || !m_mapSession->isValid()) {
        emit messageSent(false, "Not connected to MAP");
        return;
    }

    qDebug() << "MessageManager: Sending message to" << recipient;

    // Build bMessage format
    QString bMessage = QString(
        "BEGIN:BMSG\r\n"
        "VERSION:1.0\r\n"
        "STATUS:UNREAD\r\n"
        "TYPE:SMS_GSM\r\n"
        "FOLDER:\r\n"
        "BEGIN:VCARD\r\n"
        "VERSION:2.1\r\n"
        "TEL:%1\r\n"
        "END:VCARD\r\n"
        "BEGIN:BENV\r\n"
        "BEGIN:BBODY\r\n"
        "CHARSET:UTF-8\r\n"
        "ENCODING:8BIT\r\n"
        "LENGTH:%2\r\n"
        "BEGIN:MSG\r\n"
        "%3\r\n"
        "END:MSG\r\n"
        "END:BBODY\r\n"
        "END:BENV\r\n"
        "END:BMSG\r\n"
    ).arg(recipient).arg(body.toUtf8().size()).arg(body);

    // Write to temp file
    QString tmpFile = "/tmp/outgoing_msg.bmsg";
    QFile file(tmpFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit messageSent(false, "Failed to create message file");
        return;
    }
    file.write(bMessage.toUtf8());
    file.close();

    // Navigate to outbox
    m_mapSession->call("SetFolder", "/telecom/msg");

    // Push the message
    QVariantMap args;
    QDBusMessage reply = m_mapSession->call("PushMessage", tmpFile, "outbox", args);

    QFile::remove(tmpFile);

    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MessageManager: PushMessage failed:" << reply.errorMessage();
        emit messageSent(false, reply.errorMessage());
    } else {
        qDebug() << "MessageManager: Message sent successfully";
        emit messageSent(true, "");
    }
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
