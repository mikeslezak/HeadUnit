#include "ContactManager.h"
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QRandomGenerator>

#ifndef Q_OS_WIN
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusVariant>
#endif

// ========== ContactModel Implementation ==========

ContactModel::ContactModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ContactModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_contacts.count();
}

QVariant ContactModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_contacts.count())
        return QVariant();

    const Contact &contact = m_contacts.at(index.row());

    switch (role) {
    case IdRole:
        return contact.id;
    case NameRole:
        return contact.name;
    case PhoneNumberRole:
        return contact.phoneNumber;
    case PhoneNumber2Role:
        return contact.phoneNumber2;
    case EmailRole:
        return contact.email;
    case OrganizationRole:
        return contact.organization;
    case PhotoPathRole:
        return contact.photoPath;
    case FirstLetterRole:
        return contact.firstLetter;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ContactModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "contactId";
    roles[NameRole] = "name";
    roles[PhoneNumberRole] = "phoneNumber";
    roles[PhoneNumber2Role] = "phoneNumber2";
    roles[EmailRole] = "email";
    roles[OrganizationRole] = "organization";
    roles[PhotoPathRole] = "photoPath";
    roles[FirstLetterRole] = "firstLetter";
    return roles;
}

void ContactModel::addContact(const Contact &contact)
{
    beginInsertRows(QModelIndex(), m_contacts.count(), m_contacts.count());
    m_contacts.append(contact);
    endInsertRows();
}

void ContactModel::updateContact(const QString &id, const Contact &contact)
{
    int index = findContactIndex(id);
    if (index >= 0) {
        m_contacts[index] = contact;
        QModelIndex modelIndex = this->index(index);
        emit dataChanged(modelIndex, modelIndex);
    }
}

void ContactModel::removeContact(const QString &id)
{
    int index = findContactIndex(id);
    if (index >= 0) {
        beginRemoveRows(QModelIndex(), index, index);
        m_contacts.removeAt(index);
        endRemoveRows();
    }
}

void ContactModel::clear()
{
    beginResetModel();
    m_contacts.clear();
    endResetModel();
}

void ContactModel::sortContacts()
{
    beginResetModel();
    std::sort(m_contacts.begin(), m_contacts.end(), [](const Contact &a, const Contact &b) {
        return a.name.toLower() < b.name.toLower();
    });
    endResetModel();
}

Contact* ContactModel::findContact(const QString &id)
{
    for (int i = 0; i < m_contacts.count(); ++i) {
        if (m_contacts[i].id == id) {
            return &m_contacts[i];
        }
    }
    return nullptr;
}

int ContactModel::findContactIndex(const QString &id)
{
    for (int i = 0; i < m_contacts.count(); ++i) {
        if (m_contacts[i].id == id) {
            return i;
        }
    }
    return -1;
}

// ========== ContactManager Implementation ==========

ContactManager::ContactManager(QObject *parent)
    : QObject(parent)
    , m_isSyncing(false)
    , m_isConnected(false)
    , m_syncProgress(0)
    , m_contactModel(new ContactModel(this))
    , m_syncTimeout(new QTimer(this))
#ifndef Q_OS_WIN
    , m_obexClient(nullptr)
    , m_obexSession(nullptr)
#endif
{
    m_syncTimeout->setSingleShot(true);
    m_syncTimeout->setInterval(30000);  // 30 second timeout
    connect(m_syncTimeout, &QTimer::timeout,
            this, &ContactManager::onSyncTimeout);

    initialize();
}

ContactManager::~ContactManager()
{
#ifndef Q_OS_WIN
    if (m_obexSession) {
        m_obexSession->deleteLater();
    }
    if (m_obexClient) {
        m_obexClient->deleteLater();
    }
#endif
}

void ContactManager::initialize()
{
#ifdef Q_OS_WIN
    // ========== MOCK MODE (Windows) ==========
    m_mockMode = true;
    qDebug() << "ContactManager: Running in MOCK mode";

    QTimer::singleShot(500, this, [this]() {
        generateMockContacts();
    });
#else
    // ========== REAL MODE (Linux with BlueZ) ==========
    m_mockMode = false;
    qDebug() << "ContactManager: Real PBAP mode";

    // Try to load cached contacts
    loadCachedContacts();

    setupOBEXClient();
#endif
}

void ContactManager::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "ContactManager:" << msg;
    }
}

void ContactManager::setSyncProgress(int progress)
{
    if (m_syncProgress != progress) {
        m_syncProgress = progress;
        emit syncProgressChanged();
    }
}

// ========== Contact Sync Operations ==========

void ContactManager::syncContacts(const QString &deviceAddress)
{
    if (m_isSyncing) {
        qWarning() << "ContactManager: Sync already in progress";
        return;
    }

    qDebug() << "=== ContactManager: syncContacts() CALLED ===";
    qDebug() << "ContactManager: Device address:" << deviceAddress;
    qDebug() << "ContactManager: OBEX client valid:" << (m_obexClient != nullptr);

    m_currentDeviceAddress = deviceAddress;
    m_isSyncing = true;
    emit isSyncingChanged();
    emit syncStarted();

    setSyncProgress(0);
    setStatusMessage("Connecting to phonebook...");

#ifdef Q_OS_WIN
    // Mock sync
    QTimer::singleShot(1000, this, [this]() {
        setSyncProgress(50);
        setStatusMessage("Downloading contacts...");
    });

    QTimer::singleShot(2000, this, [this]() {
        setSyncProgress(100);
        setStatusMessage("Sync completed");
        m_isSyncing = false;
        emit isSyncingChanged();
        emit syncCompleted(m_contactModel->rowCount());
    });
#else
    qDebug() << "ContactManager: Calling startPBAPTransfer()";
    startPBAPTransfer(deviceAddress);
#endif
}

void ContactManager::stopSync()
{
    if (!m_isSyncing) {
        return;
    }

    qDebug() << "ContactManager: Stopping sync";

    m_syncTimeout->stop();
    m_isSyncing = false;
    emit isSyncingChanged();
    setStatusMessage("Sync cancelled");
}

void ContactManager::clearContacts()
{
    m_contactModel->clear();
    emit contactCountChanged();
    setStatusMessage("Contacts cleared");
}

// ========== Search and Query ==========

QVariantList ContactManager::searchContacts(const QString &query)
{
    QVariantList results;

    if (query.isEmpty()) {
        return results;
    }

    QString lowerQuery = query.toLower();

    for (int i = 0; i < m_contactModel->rowCount(); ++i) {
        QModelIndex idx = m_contactModel->index(i);
        QString name = m_contactModel->data(idx, ContactModel::NameRole).toString();
        QString phone = m_contactModel->data(idx, ContactModel::PhoneNumberRole).toString();

        if (name.toLower().contains(lowerQuery) || phone.contains(query)) {
            QVariantMap contact;
            contact["id"] = m_contactModel->data(idx, ContactModel::IdRole);
            contact["name"] = name;
            contact["phoneNumber"] = phone;
            contact["email"] = m_contactModel->data(idx, ContactModel::EmailRole);
            results.append(contact);
        }
    }

    return results;
}

QVariantMap ContactManager::getContact(const QString &id)
{
    QVariantMap result;
    Contact *contact = m_contactModel->findContact(id);

    if (contact) {
        result["id"] = contact->id;
        result["name"] = contact->name;
        result["phoneNumber"] = contact->phoneNumber;
        result["phoneNumber2"] = contact->phoneNumber2;
        result["email"] = contact->email;
        result["organization"] = contact->organization;
        result["photoPath"] = contact->photoPath;
    }

    return result;
}

QStringList ContactManager::getAlphabeticalSections()
{
    QSet<QString> sections;

    for (int i = 0; i < m_contactModel->rowCount(); ++i) {
        QModelIndex idx = m_contactModel->index(i);
        QChar letter = m_contactModel->data(idx, ContactModel::FirstLetterRole).toChar();
        sections.insert(QString(letter));
    }

    QStringList sorted = sections.values();
    sorted.sort();
    return sorted;
}

// ========== Actions ==========

void ContactManager::callContact(const QString &id)
{
    Contact *contact = m_contactModel->findContact(id);
    if (contact) {
        qDebug() << "ContactManager: Calling" << contact->name << "at" << contact->phoneNumber;
        // TODO: Integrate with phone call functionality
    }
}

void ContactManager::messageContact(const QString &id)
{
    Contact *contact = m_contactModel->findContact(id);
    if (contact) {
        qDebug() << "ContactManager: Messaging" << contact->name << "at" << contact->phoneNumber;
        // TODO: Integrate with messaging functionality
    }
}

// ========== Private Slots ==========

void ContactManager::onSyncTimeout()
{
    qWarning() << "ContactManager: Sync timeout";
    m_isSyncing = false;
    emit isSyncingChanged();
    emit syncFailed("Sync timeout");
    setStatusMessage("Sync failed: timeout");
}

#ifndef Q_OS_WIN

// ========== OBEX/PBAP Implementation ==========

void ContactManager::setupOBEXClient()
{
    m_obexClient = new QDBusInterface("org.bluez.obex",
                                       "/org/bluez/obex",
                                       "org.bluez.obex.Client1",
                                       QDBusConnection::sessionBus(),
                                       this);

    if (!m_obexClient->isValid()) {
        qWarning() << "ContactManager: Failed to connect to OBEX client:" << m_obexClient->lastError().message();
        m_obexClient->deleteLater();
        m_obexClient = nullptr;
    } else {
        qDebug() << "ContactManager: OBEX client connected";
    }
}

void ContactManager::startPBAPTransfer(const QString &deviceAddress)
{
    qDebug() << "=== ContactManager::startPBAPTransfer() CALLED ===";

    if (!m_obexClient) {
        qWarning() << "ContactManager: OBEX client not available!";
        emit syncFailed("OBEX client not available");
        m_isSyncing = false;
        emit isSyncingChanged();
        return;
    }

    qDebug() << "ContactManager: OBEX client is valid, creating session...";

    // Create OBEX session
    QDBusInterface *session = createOBEXSession(deviceAddress);
    if (!session) {
        qWarning() << "ContactManager: Failed to create OBEX session!";
        emit syncFailed("Failed to create OBEX session");
        m_isSyncing = false;
        emit isSyncingChanged();
        return;
    }

    qDebug() << "ContactManager: OBEX session created successfully";
    m_obexSession = session;

    // Step 1: Select the phonebook
    qDebug() << "ContactManager: Selecting phonebook...";
    QDBusMessage selectMsg = m_obexSession->call("Select", "int", "pb");

    if (selectMsg.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "ContactManager: Select failed:" << selectMsg.errorMessage();
        emit syncFailed("Failed to select phonebook: " + selectMsg.errorMessage());
        m_isSyncing = false;
        emit isSyncingChanged();
        return;
    }

    qDebug() << "ContactManager: Phonebook selected successfully";

    // Step 2: Pull all contacts
    setSyncProgress(30);
    setStatusMessage("Downloading phonebook...");

    // Create target file path
    QString targetFile = "/tmp/contacts.vcf";

    // Create empty filters dictionary
    QVariantMap filters;

    qDebug() << "ContactManager: Calling PullAll with targetFile:" << targetFile;
    QDBusMessage msg = m_obexSession->call("PullAll", targetFile, filters);

    if (msg.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "ContactManager: PBAP PullAll failed:" << msg.errorMessage();
        emit syncFailed(msg.errorMessage());
        m_isSyncing = false;
        emit isSyncingChanged();
        return;
    }

    qDebug() << "ContactManager: PullAll reply arguments count:" << msg.arguments().count();

    if (msg.arguments().count() < 1) {
        qWarning() << "ContactManager: PullAll returned no transfer path!";
        emit syncFailed("No transfer path returned");
        m_isSyncing = false;
        emit isSyncingChanged();
        return;
    }

    // Get the transfer path (first return value)
    QDBusObjectPath transferObjPath = msg.arguments().at(0).value<QDBusObjectPath>();
    QString transferPath = transferObjPath.path();
    m_currentTransferPath = transferPath;

    qDebug() << "ContactManager: PBAP transfer started:" << transferPath;
    qDebug() << "ContactManager: Monitoring transfer object for completion...";

    // CRITICAL FIX: Connect to DBus signals to monitor transfer
    QDBusConnection::sessionBus().connect("org.bluez.obex",
                                          transferPath,
                                          "org.freedesktop.DBus.Properties",
                                          "PropertiesChanged",
                                          this,
                                          SLOT(onTransferPropertiesChanged(QString,QVariantMap,QStringList)));

    m_syncTimeout->start();
    qDebug() << "ContactManager: Sync timeout timer started (30 seconds)";
}

QDBusInterface* ContactManager::createOBEXSession(const QString &deviceAddress)
{
    QVariantMap args;
    args["Target"] = "PBAP";

    QDBusMessage msg = m_obexClient->call("CreateSession", deviceAddress, args);

    if (msg.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "ContactManager: CreateSession failed:" << msg.errorMessage();
        return nullptr;
    }

    QString sessionPath = msg.arguments().at(0).value<QDBusObjectPath>().path();
    qDebug() << "ContactManager: OBEX session created:" << sessionPath;

    QDBusInterface *session = new QDBusInterface("org.bluez.obex",
                                                   sessionPath,
                                                   "org.bluez.obex.PhonebookAccess1",
                                                   QDBusConnection::sessionBus(),
                                                   this);

    if (!session->isValid()) {
        qWarning() << "ContactManager: Invalid OBEX session:" << session->lastError().message();
        session->deleteLater();
        return nullptr;
    }

    return session;
}

void ContactManager::onTransferComplete(const QDBusObjectPath &transfer)
{
    if (transfer.path() != m_currentTransferPath) {
        return;
    }

    qDebug() << "ContactManager: Transfer complete:" << transfer.path();

    m_syncTimeout->stop();

    // Read the downloaded vCard file
    QString filename = "/tmp/obex-transfer-XXXXXX.vcf";  // BlueZ typically saves here

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ContactManager: Failed to open vCard file";
        emit syncFailed("Failed to read contacts");
        m_isSyncing = false;
        emit isSyncingChanged();
        return;
    }

    QString vcardData = QTextStream(&file).readAll();
    file.close();

    parseVCard(vcardData);

    saveCachedContacts();

    setSyncProgress(100);
    setStatusMessage(QString("Synced %1 contacts").arg(m_contactModel->rowCount()));

    m_isSyncing = false;
    m_isConnected = true;
    emit isSyncingChanged();
    emit isConnectedChanged();
    emit syncCompleted(m_contactModel->rowCount());
}

void ContactManager::onTransferError(const QDBusObjectPath &transfer, const QString &error)
{
    if (transfer.path() != m_currentTransferPath) {
        return;
    }

    qWarning() << "ContactManager: Transfer error:" << error;

    m_syncTimeout->stop();
    m_isSyncing = false;
    emit isSyncingChanged();
    emit syncFailed(error);
    setStatusMessage("Sync failed: " + error);
}

void ContactManager::onTransferPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    Q_UNUSED(invalidated);

    qDebug() << "ContactManager: Transfer properties changed:" << interface;
    qDebug() << "  Changed properties:" << changed.keys();

    if (!changed.contains("Status")) {
        return;
    }

    QString status = changed["Status"].toString();
    qDebug() << "ContactManager: Transfer status:" << status;

    if (status == "complete") {
        qDebug() << "ContactManager: Transfer complete!";
        m_syncTimeout->stop();

        // Use the file path we specified in PullAll
        QString vcardFile = "/tmp/contacts.vcf";

        qDebug() << "ContactManager: Reading vCard from:" << vcardFile;

        // Read the downloaded vCard file
        QFile file(vcardFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "ContactManager: Failed to open vCard file:" << vcardFile;
            emit syncFailed("Failed to read contacts");
            m_isSyncing = false;
            emit isSyncingChanged();
            return;
        }

        QString vcardData = QTextStream(&file).readAll();
        file.close();

        qDebug() << "ContactManager: vCard data size:" << vcardData.length() << "bytes";

        parseVCard(vcardData);
        saveCachedContacts();

        setSyncProgress(100);
        setStatusMessage(QString("Synced %1 contacts").arg(m_contactModel->rowCount()));

        m_isSyncing = false;
        m_isConnected = true;
        emit isSyncingChanged();
        emit isConnectedChanged();
        emit syncCompleted(m_contactModel->rowCount());
    }
    else if (status == "error") {
        qWarning() << "ContactManager: Transfer failed";

        m_syncTimeout->stop();
        m_isSyncing = false;
        emit isSyncingChanged();
        emit syncFailed("Transfer failed");
        setStatusMessage("Sync failed");
    }
}

// ========== vCard Parsing ==========

void ContactManager::parseVCard(const QString &vcardData)
{
    qDebug() << "ContactManager: Parsing vCard data";

    m_contactModel->clear();

    // Split into individual vCards
    QRegularExpression vcardRegex("BEGIN:VCARD.*?END:VCARD", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator it = vcardRegex.globalMatch(vcardData);

    int count = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString vcardEntry = match.captured(0);

        Contact contact = parseVCardEntry(vcardEntry);

        if (!contact.name.isEmpty() || !contact.phoneNumber.isEmpty()) {
            m_contactModel->addContact(contact);
            count++;

            if (count % 10 == 0) {
                setSyncProgress(30 + (count * 60 / 100));  // Progress from 30% to 90%
            }
        }
    }

    m_contactModel->sortContacts();
    emit contactCountChanged();

    qDebug() << "ContactManager: Parsed" << count << "contacts";
}

Contact ContactManager::parseVCardEntry(const QString &vcardEntry)
{
    Contact contact;

    // Generate unique ID
    contact.id = QString::number(QDateTime::currentMSecsSinceEpoch()) +
                 QString::number(QRandomGenerator::global()->generate());

    // Extract fields
    contact.name = extractVCardField(vcardEntry, "FN");
    contact.phoneNumber = extractVCardField(vcardEntry, "TEL");
    contact.email = extractVCardField(vcardEntry, "EMAIL");
    contact.organization = extractVCardField(vcardEntry, "ORG");

    // Extract secondary phone number
    QRegularExpression telRegex("TEL[^:]*:([^\\r\\n]+)");
    QRegularExpressionMatchIterator telIt = telRegex.globalMatch(vcardEntry);
    int telCount = 0;
    while (telIt.hasNext()) {
        QRegularExpressionMatch match = telIt.next();
        telCount++;
        if (telCount == 2) {
            contact.phoneNumber2 = match.captured(1).trimmed();
            break;
        }
    }

    // Set first letter for alphabetical grouping
    if (!contact.name.isEmpty()) {
        QChar firstChar = contact.name.at(0).toUpper();
        if (firstChar.isLetter()) {
            contact.firstLetter = firstChar;
        } else {
            contact.firstLetter = '#';
        }
    } else {
        contact.firstLetter = '#';
    }

    return contact;
}

QString ContactManager::extractVCardField(const QString &vcard, const QString &field)
{
    QRegularExpression regex(field + "[^:]*:([^\\r\\n]+)");
    QRegularExpressionMatch match = regex.match(vcard);

    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    return QString();
}

#endif

// ========== Cache Management ==========

void ContactManager::loadCachedContacts()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);
    QString cacheFile = cacheDir + "/contacts.cache";

    QFile file(cacheFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "ContactManager: No cached contacts found";
        return;
    }

    QDataStream in(&file);
    int count;
    in >> count;

    for (int i = 0; i < count; ++i) {
        Contact contact;
        in >> contact.id >> contact.name >> contact.phoneNumber
           >> contact.phoneNumber2 >> contact.email
           >> contact.organization >> contact.photoPath;

        QString firstLetterStr;
        in >> firstLetterStr;
        contact.firstLetter = firstLetterStr.isEmpty() ? '#' : firstLetterStr.at(0);

        m_contactModel->addContact(contact);
    }

    file.close();

    qDebug() << "ContactManager: Loaded" << count << "cached contacts";
    emit contactCountChanged();
}

void ContactManager::saveCachedContacts()
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);
    QString cacheFile = cacheDir + "/contacts.cache";

    QFile file(cacheFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "ContactManager: Failed to save contacts cache";
        return;
    }

    QDataStream out(&file);
    out << m_contactModel->rowCount();

    for (int i = 0; i < m_contactModel->rowCount(); ++i) {
        QModelIndex idx = m_contactModel->index(i);
        out << m_contactModel->data(idx, ContactModel::IdRole).toString()
            << m_contactModel->data(idx, ContactModel::NameRole).toString()
            << m_contactModel->data(idx, ContactModel::PhoneNumberRole).toString()
            << m_contactModel->data(idx, ContactModel::PhoneNumber2Role).toString()
            << m_contactModel->data(idx, ContactModel::EmailRole).toString()
            << m_contactModel->data(idx, ContactModel::OrganizationRole).toString()
            << m_contactModel->data(idx, ContactModel::PhotoPathRole).toString()
            << QString(m_contactModel->data(idx, ContactModel::FirstLetterRole).toChar());
    }

    file.close();

    qDebug() << "ContactManager: Saved" << m_contactModel->rowCount() << "contacts to cache";
}

// ========== Mock Data ==========

void ContactManager::generateMockContacts()
{
    qDebug() << "ContactManager: Generating mock contacts";

    QStringList names = {
        "Alice Johnson", "Bob Smith", "Charlie Brown", "Diana Prince",
        "Edward Norton", "Fiona Apple", "George Martin", "Hannah Montana",
        "Isaac Newton", "Julia Roberts", "Kevin Hart", "Laura Palmer",
        "Michael Jordan", "Nancy Drew", "Oliver Twist", "Patricia Highsmith",
        "Quentin Tarantino", "Rachel Green", "Steve Jobs", "Tina Fey"
    };

    for (int i = 0; i < names.count(); ++i) {
        Contact contact;
        contact.id = QString::number(i + 1);
        contact.name = names[i];
        contact.phoneNumber = QString("+1 555-0%1%2%3")
            .arg(100 + i, 3, 10, QChar('0'))
            .arg(QChar('0' + (i % 10)))
            .arg(QChar('0' + ((i + 1) % 10)));
        contact.email = names[i].split(" ").first().toLower() + "@example.com";
        contact.organization = (i % 3 == 0) ? "Acme Corp" : "";

        QChar firstChar = contact.name.at(0).toUpper();
        contact.firstLetter = firstChar.isLetter() ? firstChar : '#';

        m_contactModel->addContact(contact);
    }

    m_contactModel->sortContacts();
    emit contactCountChanged();

    qDebug() << "ContactManager: Generated" << m_contactModel->rowCount() << "mock contacts";
}
