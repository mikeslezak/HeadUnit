#ifndef MESSAGEMANAGER_H
#define MESSAGEMANAGER_H

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QList>
#include <QDateTime>
#include <QTimer>
#include <QVariantMap>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#endif

/**
 * Message - Represents a single SMS/MMS message
 */
struct Message {
    QString id;
    QString threadId;       // Conversation thread ID
    QString sender;         // Phone number or contact name
    QString senderAddress;  // Raw phone number
    QString body;           // Message text
    QDateTime timestamp;
    bool isIncoming;        // true = received, false = sent
    bool isRead;
    QString type;           // "SMS" or "MMS"

    Message()
        : isIncoming(true), isRead(false), type("SMS") {}
};

/**
 * Conversation - Represents a message thread with a contact
 */
struct Conversation {
    QString threadId;
    QString contactName;
    QString contactAddress;  // Phone number
    QString lastMessageBody;
    QDateTime lastMessageTime;
    int unreadCount;
    bool isPinned;

    Conversation()
        : unreadCount(0), isPinned(false) {}
};

/**
 * MessageModel - QML-accessible model for message list
 */
class MessageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ThreadIdRole,
        SenderRole,
        SenderAddressRole,
        BodyRole,
        TimestampRole,
        IsIncomingRole,
        IsReadRole,
        TypeRole,
        FormattedTimeRole
    };

    explicit MessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addMessage(const Message &message);
    void updateMessage(const QString &id, const Message &message);
    void removeMessage(const QString &id);
    void clear();
    void sortMessagesByTime();

    Message* findMessage(const QString &id);
    const QList<Message>& messages() const { return m_messages; }

private:
    QList<Message> m_messages;
    QString formatTimestamp(const QDateTime &dt) const;
};

/**
 * ConversationModel - QML-accessible model for conversation list
 */
class ConversationModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ThreadIdRole = Qt::UserRole + 1,
        ContactNameRole,
        ContactAddressRole,
        LastMessageBodyRole,
        LastMessageTimeRole,
        UnreadCountRole,
        IsPinnedRole,
        FormattedTimeRole
    };

    explicit ConversationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addConversation(const Conversation &conv);
    void updateConversation(const QString &threadId, const Conversation &conv);
    void removeConversation(const QString &threadId);
    void clear();
    void sortConversations();

    Conversation* findConversation(const QString &threadId);
    const QList<Conversation>& conversations() const { return m_conversations; }

private:
    QList<Conversation> m_conversations;
    QString formatTimestamp(const QDateTime &dt) const;
};

/**
 * MessageManager - Manages SMS/MMS messaging via Bluetooth MAP
 *
 * Integrates with BlueZ MAP daemon to:
 * - Pull messages from connected phone
 * - Send SMS messages
 * - Track read/unread status
 * - Organize messages into conversations
 * - Handle message notifications
 */
class MessageManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ConversationModel* conversationModel READ conversationModel CONSTANT)
    Q_PROPERTY(MessageModel* messageModel READ messageModel CONSTANT)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(bool isSyncing READ isSyncing NOTIFY isSyncingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int totalUnreadCount READ totalUnreadCount NOTIFY totalUnreadCountChanged)
    Q_PROPERTY(QString currentThreadId READ currentThreadId WRITE setCurrentThreadId NOTIFY currentThreadIdChanged)

public:
    explicit MessageManager(QObject *parent = nullptr);
    ~MessageManager();

    ConversationModel* conversationModel() { return m_conversationModel; }
    MessageModel* messageModel() { return m_messageModel; }

    bool isConnected() const { return m_isConnected; }
    bool isSyncing() const { return m_isSyncing; }
    QString statusMessage() const { return m_statusMessage; }
    int totalUnreadCount() const { return m_totalUnreadCount; }
    QString currentThreadId() const { return m_currentThreadId; }

    void setCurrentThreadId(const QString &threadId);

    // QML-invokable methods
    Q_INVOKABLE void connectToDevice(const QString &deviceAddress);
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void syncMessages();
    Q_INVOKABLE void loadConversation(const QString &threadId);
    Q_INVOKABLE void sendMessage(const QString &recipient, const QString &body);
    Q_INVOKABLE void markAsRead(const QString &threadId);
    Q_INVOKABLE void deleteConversation(const QString &threadId);

signals:
    void isConnectedChanged();
    void isSyncingChanged();
    void statusMessageChanged();
    void totalUnreadCountChanged();
    void currentThreadIdChanged();
    void messageSent(bool success, const QString &error);
    void newMessageReceived(const QString &threadId, const QString &sender, const QString &body);

private slots:
    void onMessageReceived(const QString &sender, const QString &body, const QDateTime &timestamp);
    void updateUnreadCount();

private:
    void setStatusMessage(const QString &msg);
    void setIsConnected(bool connected);
    void setIsSyncing(bool syncing);

    // Bluetooth MAP operations
    bool connectMAP();
    void pullMessageList();
    void pullMessage(const QString &messageId);
    void pushMessage(const QString &recipient, const QString &body);

    // Message processing
    void processMessageData(const QByteArray &data);
    QString createThreadId(const QString &phoneNumber);
    void updateConversationFromMessage(const Message &msg);

    // Data persistence
    void saveMessagesCache();
    void loadMessagesCache();

    ConversationModel *m_conversationModel;
    MessageModel *m_messageModel;

    bool m_isConnected;
    bool m_isSyncing;
    QString m_statusMessage;
    int m_totalUnreadCount;
    QString m_currentThreadId;
    QString m_deviceAddress;

#ifndef Q_OS_WIN
    QDBusInterface *m_obexClient;
    QDBusInterface *m_mapSession;    // org.bluez.obex.MessageAccess1 on session path
    QString m_sessionPath;

    void setupOBEXClient();
    void cleanupSession();
    void parseBMessageList(const QDBusMessage &reply);
    void parseBMessage(const QString &filePath, const QString &handle);
    void onTransferPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
    QString m_currentTransferPath;
    QString m_pendingMessageHandle;
#endif

    QTimer *m_syncTimer;
};

#endif // MESSAGEMANAGER_H
