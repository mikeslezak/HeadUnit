#include "VoiceCommandHandler.h"
#include "ContactManager.h"
#include "MessageManager.h"
#include "BluetoothManager.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QMetaObject>

VoiceCommandHandler::VoiceCommandHandler(QObject *parent)
    : QObject(parent)
    , m_statusMessage("Ready")
    , m_awaitingConfirmation(false)
    , m_pendingAction("")
    , m_contactManager(nullptr)
    , m_messageManager(nullptr)
    , m_bluetoothManager(nullptr)
{
    qDebug() << "VoiceCommandHandler: Initialized";
}

// ========================================================================
// DEPENDENCY INJECTION
// ========================================================================

void VoiceCommandHandler::setContactManager(QObject *contactManager)
{
    m_contactManager = contactManager;
    qDebug() << "VoiceCommandHandler: ContactManager set";
}

void VoiceCommandHandler::setMessageManager(QObject *messageManager)
{
    m_messageManager = messageManager;
    qDebug() << "VoiceCommandHandler: MessageManager set";
}

void VoiceCommandHandler::setBluetoothManager(QObject *bluetoothManager)
{
    m_bluetoothManager = bluetoothManager;
    qDebug() << "VoiceCommandHandler: BluetoothManager set";
}

// ========================================================================
// PROPERTY SETTERS
// ========================================================================

void VoiceCommandHandler::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "VoiceCommandHandler:" << msg;
    }
}

void VoiceCommandHandler::setAwaitingConfirmation(bool awaiting)
{
    if (m_awaitingConfirmation != awaiting) {
        m_awaitingConfirmation = awaiting;
        emit awaitingConfirmationChanged();
    }
}

void VoiceCommandHandler::setPendingAction(const QString &action)
{
    if (m_pendingAction != action) {
        m_pendingAction = action;
        emit pendingActionChanged();
    }
}

// ========================================================================
// COMMAND PROCESSING
// ========================================================================

void VoiceCommandHandler::processClaudeResponse(const QString &claudeResponse)
{
    qDebug() << "VoiceCommandHandler: Processing Claude response...";

    // Auto-cancel any stale pending confirmation before processing new command
    if (m_awaitingConfirmation) {
        qDebug() << "VoiceCommandHandler: Clearing stale pending confirmation";
        cancelAction();
    }

    // Parse JSON command from Claude's response
    QVariantMap command = parseCommandJSON(claudeResponse);

    if (command.isEmpty()) {
        qDebug() << "VoiceCommandHandler: No actionable command found in response";
        return;
    }

    QString action = command["action"].toString();
    qDebug() << "VoiceCommandHandler: Executing action:" << action;

    // Route to appropriate handler and track success
    bool succeeded = false;
    if (action == "call") {
        succeeded = executeCallCommand(command);
    } else if (action == "message") {
        succeeded = executeMessageCommand(command);
    } else if (action == "read_messages") {
        succeeded = executeReadMessagesCommand(command);
    } else if (action == "navigate") {
        succeeded = executeNavigateCommand(command);
    } else if (action == "search_places") {
        succeeded = executeSearchPlacesCommand(command);
    } else if (action == "quiet_mode") {
        succeeded = executeQuietModeCommand(command);
    } else {
        qWarning() << "VoiceCommandHandler: Unknown action:" << action;
    }

    // Check for expects_reply flag — signal follow-up mode only if action succeeded
    if (succeeded && command.contains("expects_reply") && command["expects_reply"].toBool()) {
        qDebug() << "VoiceCommandHandler: Claude expects a reply, signaling follow-up mode";
        emit followUpExpected();
    }
}

QVariantMap VoiceCommandHandler::parseCommandJSON(const QString &claudeResponse)
{
    // Look for JSON block in Claude's response
    // Expected format: ```json\n{...}\n```

    QRegularExpression jsonPattern(R"(```json\s*(\{[^`]*\})\s*```)");
    QRegularExpressionMatch match = jsonPattern.match(claudeResponse);

    if (!match.hasMatch()) {
        qDebug() << "VoiceCommandHandler: No JSON command block found";
        return QVariantMap();
    }

    QString jsonStr = match.captured(1);
    qDebug() << "VoiceCommandHandler: Found JSON command:" << jsonStr;

    // Parse JSON
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "VoiceCommandHandler: Failed to parse JSON command";
        return QVariantMap();
    }

    return doc.object().toVariantMap();
}

// ========================================================================
// COMMAND EXECUTION
// ========================================================================

bool VoiceCommandHandler::executeCallCommand(const QVariantMap &command)
{
    QString contactName = command["contact_name"].toString();
    QString phoneNumber = command["phone_number"].toString();

    qDebug() << "VoiceCommandHandler: Call command - Contact:" << contactName << "Number:" << phoneNumber;

    // If no phone number provided, look up contact
    if (phoneNumber.isEmpty() && !contactName.isEmpty()) {
        phoneNumber = findContactPhoneNumber(contactName);
    }

    if (phoneNumber.isEmpty()) {
        emit commandFailed("call", "Contact not found or no phone number");
        return false;
    }

    // Execute call via BluetoothManager
    if (m_bluetoothManager) {
        // Call dialNumber method
        QMetaObject::invokeMethod(m_bluetoothManager, "dialNumber",
                                Q_ARG(QString, phoneNumber));

        emit commandExecuted("call", QString("Calling %1").arg(contactName));
        return true;
    } else {
        qWarning() << "VoiceCommandHandler: BluetoothManager not set";
        emit commandFailed("call", "BluetoothManager not available");
        return false;
    }
}

bool VoiceCommandHandler::executeMessageCommand(const QVariantMap &command)
{
    QString contactName = command["contact_name"].toString();
    QString messageBody = command["message_body"].toString();
    QString phoneNumber = command["phone_number"].toString();

    qDebug() << "VoiceCommandHandler: Message command - Contact:" << contactName
             << "Body:" << messageBody << "Number:" << phoneNumber;

    // If no phone number provided, look up contact
    if (phoneNumber.isEmpty() && !contactName.isEmpty()) {
        phoneNumber = findContactPhoneNumber(contactName);
    }

    if (phoneNumber.isEmpty()) {
        emit commandFailed("message", "Contact not found or no phone number");
        return false;
    }

    if (messageBody.isEmpty()) {
        emit commandFailed("message", "No message body provided");
        return false;
    }

    // Store pending command and request confirmation
    m_pendingCommand = command;
    m_pendingCommand["phone_number"] = phoneNumber;  // Store resolved phone number

    setPendingAction("send_message");
    setAwaitingConfirmation(true);

    // Request confirmation via signal (QML layer handles TTS)
    QString recipient = contactName.isEmpty() ? phoneNumber : contactName;
    QString confirmMsg = QString("Sending message to %1: %2. Should I send it?")
                            .arg(recipient, messageBody);

    emit confirmationRequested("send_message", confirmMsg);
    return true;
}

bool VoiceCommandHandler::executeReadMessagesCommand(const QVariantMap &command)
{
    QString contactName = command["contact_name"].toString();

    qDebug() << "VoiceCommandHandler: Read messages command - Contact:" << contactName;

    if (!m_messageManager) {
        qWarning() << "VoiceCommandHandler: MessageManager not set";
        emit commandFailed("read_messages", "MessageManager not available");
        return false;
    }

    // Get conversation model
    QObject *conversationModel = m_messageManager->property("conversationModel").value<QObject*>();
    if (!conversationModel) {
        emit commandFailed("read_messages", "No messages found");
        return false;
    }

    emit commandExecuted("read_messages", QString("Reading messages for %1").arg(contactName));
    return true;
}

bool VoiceCommandHandler::executeNavigateCommand(const QVariantMap &command)
{
    QString destination = command["destination"].toString();

    qDebug() << "VoiceCommandHandler: Navigate command - Destination:" << destination;

    if (destination.isEmpty()) {
        emit commandFailed("navigate", "No destination provided");
        return false;
    }

    emit navigationRequested(destination);
    emit commandExecuted("navigate", QString("Navigating to %1").arg(destination));
    return true;
}

bool VoiceCommandHandler::executeSearchPlacesCommand(const QVariantMap &command)
{
    QString query = command["query"].toString();
    QString category = command["category"].toString();

    qDebug() << "VoiceCommandHandler: Search places - Query:" << query << "Category:" << category;

    if (query.isEmpty()) {
        emit commandFailed("search_places", "No search query provided");
        return false;
    }

    emit placesSearchRequested(query, category);
    emit commandExecuted("search_places", QString("Searching for %1").arg(query));
    return true;
}

bool VoiceCommandHandler::executeQuietModeCommand(const QVariantMap &command)
{
    bool enabled = command.value("enabled", true).toBool();
    qDebug() << "VoiceCommandHandler: Quiet mode -" << (enabled ? "on" : "off");
    emit quietModeRequested(enabled);
    emit commandExecuted("quiet_mode", enabled ? "enabled" : "disabled");
    return true;
}

// ========================================================================
// CONFIRMATION FLOW
// ========================================================================

void VoiceCommandHandler::confirmAction()
{
    qDebug() << "VoiceCommandHandler: Confirming action:" << m_pendingAction;

    if (!m_awaitingConfirmation) {
        qWarning() << "VoiceCommandHandler: No pending action to confirm";
        return;
    }

    if (m_pendingAction == "send_message") {
        // Send the message
        QString phoneNumber = m_pendingCommand["phone_number"].toString();
        QString messageBody = m_pendingCommand["message_body"].toString();
        QString contactName = m_pendingCommand["contact_name"].toString();

        if (m_messageManager) {
            QMetaObject::invokeMethod(m_messageManager, "sendMessage",
                                    Q_ARG(QString, phoneNumber),
                                    Q_ARG(QString, messageBody));

            emit commandExecuted("send_message", QString("Sent to %1").arg(contactName));
        }
    }

    // Clear pending state
    setAwaitingConfirmation(false);
    setPendingAction("");
    m_pendingCommand.clear();
}

void VoiceCommandHandler::cancelAction()
{
    qDebug() << "VoiceCommandHandler: Canceling action:" << m_pendingAction;

    setAwaitingConfirmation(false);
    setPendingAction("");
    m_pendingCommand.clear();
}

// ========================================================================
// HELPER METHODS
// ========================================================================

QString VoiceCommandHandler::findContactPhoneNumber(const QString &contactName)
{
    if (!m_contactManager) {
        qWarning() << "VoiceCommandHandler: ContactManager not set";
        return QString();
    }

    // Use searchContacts to find matching contact
    QVariantList results;
    QMetaObject::invokeMethod(m_contactManager, "searchContacts",
                            Q_RETURN_ARG(QVariantList, results),
                            Q_ARG(QString, contactName));

    qDebug() << "VoiceCommandHandler: Search for" << contactName << "returned" << results.size() << "results";

    if (results.isEmpty()) {
        qDebug() << "VoiceCommandHandler: No contact found for:" << contactName;
        return QString();
    }

    // Log all matches for debugging
    for (int i = 0; i < qMin(results.size(), 3); ++i) {
        QVariantMap c = results.at(i).toMap();
        qDebug() << "VoiceCommandHandler:   Match" << (i+1) << ":" << c["name"].toString() << "->" << c["phoneNumber"].toString();
    }

    // Get first match
    QVariantMap contact = results.first().toMap();
    QString phoneNumber = contact["phoneNumber"].toString();
    QString matchedName = contact["name"].toString();

    qDebug() << "VoiceCommandHandler: Found phone number:" << phoneNumber << "for:" << contactName << "(matched:" << matchedName << ")";
    return phoneNumber;
}

