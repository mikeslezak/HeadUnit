#ifndef CONTACTMANAGER_H
#define CONTACTMANAGER_H

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QList>
#include <QTimer>
#include <QVariantMap>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#endif

/**
 * Contact - Represents a phone contact
 */
struct Contact {
    QString id;
    QString name;
    QString phoneNumber;
    QString phoneNumber2;  // Secondary number
    QString email;
    QString organization;
    QString photoPath;     // Path to contact photo
    QChar firstLetter;     // For alphabetical grouping

    Contact()
        : firstLetter('?') {}
};

/**
 * ContactModel - QML-accessible model for contact list
 */
class ContactModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        PhoneNumberRole,
        PhoneNumber2Role,
        EmailRole,
        OrganizationRole,
        PhotoPathRole,
        FirstLetterRole
    };

    explicit ContactModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addContact(const Contact &contact);
    void updateContact(const QString &id, const Contact &contact);
    void removeContact(const QString &id);
    void clear();
    void sortContacts();

    Contact* findContact(const QString &id);
    int findContactIndex(const QString &id);

private:
    QList<Contact> m_contacts;
};

/**
 * ContactManager - Manages phone contact syncing via PBAP
 *
 * Integrates with BlueZ OBEX daemon to:
 * - Pull contacts from connected phone
 * - Parse vCard format
 * - Provide searchable contact list
 * - Support contact photos
 */
class ContactManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(ContactModel* contactModel READ contactModel CONSTANT)
    Q_PROPERTY(int contactCount READ contactCount NOTIFY contactCountChanged)
    Q_PROPERTY(bool isSyncing READ isSyncing NOTIFY isSyncingChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int syncProgress READ syncProgress NOTIFY syncProgressChanged)

public:
    explicit ContactManager(QObject *parent = nullptr);
    ~ContactManager();

    // Property getters
    ContactModel* contactModel() { return m_contactModel; }
    int contactCount() const { return m_contactModel->rowCount(); }
    bool isSyncing() const { return m_isSyncing; }
    bool isConnected() const { return m_isConnected; }
    QString statusMessage() const { return m_statusMessage; }
    int syncProgress() const { return m_syncProgress; }

public slots:
    // Contact sync operations
    void syncContacts(const QString &deviceAddress);
    void stopSync();
    void clearContacts();

    // Search and query
    Q_INVOKABLE QVariantList searchContacts(const QString &query);
    Q_INVOKABLE QVariantMap getContact(const QString &id);
    Q_INVOKABLE QStringList getAlphabeticalSections();
    Q_INVOKABLE QString findContactNameByNumber(const QString &phoneNumber);

    // Actions
    Q_INVOKABLE void callContact(const QString &id);
    Q_INVOKABLE void messageContact(const QString &id);

    // Context for AI assistant
    Q_INVOKABLE QStringList getAllContactNames();

signals:
    void contactCountChanged();
    void isSyncingChanged();
    void isConnectedChanged();
    void statusMessageChanged();
    void syncProgressChanged();

    void syncStarted();
    void syncCompleted(int contactCount);
    void syncFailed(const QString &error);

    void contactAdded(const QString &id);
    void error(const QString &message);

private slots:
    void onSyncTimeout();

#ifndef Q_OS_WIN
    void onTransferComplete(const QDBusObjectPath &transfer);
    void onTransferError(const QDBusObjectPath &transfer, const QString &error);
    void onTransferPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
#endif

private:
    void setStatusMessage(const QString &msg);
    void setSyncProgress(int progress);
    void initialize();

#ifndef Q_OS_WIN
    void setupOBEXClient();
    void startPBAPTransfer(const QString &deviceAddress);
    void parseVCard(const QString &vcardData);
    Contact parseVCardEntry(const QString &vcardEntry);
    QString extractVCardField(const QString &vcard, const QString &field);
    QDBusInterface* createOBEXSession(const QString &deviceAddress);
#endif

    void loadCachedContacts();
    void saveCachedContacts();
    void generateMockContacts();

    // Member variables
    bool m_isSyncing;
    bool m_isConnected;
    QString m_statusMessage;
    int m_syncProgress;
    QString m_currentDeviceAddress;

    ContactModel *m_contactModel;
    QTimer *m_syncTimeout;

#ifndef Q_OS_WIN
    QDBusInterface *m_obexClient;
    QDBusInterface *m_obexSession;
    QString m_currentTransferPath;
#endif

    bool m_mockMode;
};

#endif // CONTACTMANAGER_H
