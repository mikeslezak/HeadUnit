#include "VoiceCommandHandler.h"
#include "ContactManager.h"
#include "MessageManager.h"
#include "BluetoothManager.h"
#include "GoogleTTS.h"
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
    , m_googleTTS(nullptr)
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

void VoiceCommandHandler::setGoogleTTS(QObject *googleTTS)
{
    m_googleTTS = googleTTS;
    qDebug() << "VoiceCommandHandler: GoogleTTS set";
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

    // Parse JSON command from Claude's response
    QVariantMap command = parseCommandJSON(claudeResponse);

    if (command.isEmpty()) {
        qDebug() << "VoiceCommandHandler: No actionable command found in response";
        return;
    }

    QString action = command["action"].toString();
    qDebug() << "VoiceCommandHandler: Executing action:" << action;

    // Route to appropriate handler
    if (action == "call") {
        executeCallCommand(command);
    } else if (action == "message") {
        executeMessageCommand(command);
    } else if (action == "read_messages") {
        executeReadMessagesCommand(command);
    } else {
        qWarning() << "VoiceCommandHandler: Unknown action:" << action;
        speakFeedback("I'm not sure how to do that.");
    }
}

QVariantMap VoiceCommandHandler::parseCommandJSON(const QString &claudeResponse)
{
    // Look for JSON block in Claude's response
    // Expected format: ```json\n{...}\n```

    QRegularExpression jsonPattern(R"(```json\s*(\{[^`]*\})\s*```)",
                                   QRegularExpression::DotMatchesEverythingOption);
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

void VoiceCommandHandler::executeCallCommand(const QVariantMap &command)
{
    QString contactName = command["contact_name"].toString();
    QString phoneNumber = command["phone_number"].toString();

    qDebug() << "VoiceCommandHandler: Call command - Contact:" << contactName << "Number:" << phoneNumber;

    // If no phone number provided, look up contact
    if (phoneNumber.isEmpty() && !contactName.isEmpty()) {
        phoneNumber = findContactPhoneNumber(contactName);
    }

    if (phoneNumber.isEmpty()) {
        speakFeedback(QString("I couldn't find a phone number for %1").arg(contactName));
        emit commandFailed("call", "Contact not found or no phone number");
        return;
    }

    // Execute call via BluetoothManager
    if (m_bluetoothManager) {
        speakFeedback(QString("Calling %1").arg(contactName.isEmpty() ? phoneNumber : contactName));

        // Call dialNumber method
        QMetaObject::invokeMethod(m_bluetoothManager, "dialNumber",
                                Q_ARG(QString, phoneNumber));

        emit commandExecuted("call", QString("Calling %1").arg(contactName));
    } else {
        qWarning() << "VoiceCommandHandler: BluetoothManager not set";
        speakFeedback("I can't make calls right now");
        emit commandFailed("call", "BluetoothManager not available");
    }
}

void VoiceCommandHandler::executeMessageCommand(const QVariantMap &command)
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
        speakFeedback(QString("I couldn't find a phone number for %1").arg(contactName));
        emit commandFailed("message", "Contact not found or no phone number");
        return;
    }

    if (messageBody.isEmpty()) {
        speakFeedback("What would you like to say?");
        emit commandFailed("message", "No message body provided");
        return;
    }

    // Store pending command and request confirmation
    m_pendingCommand = command;
    m_pendingCommand["phone_number"] = phoneNumber;  // Store resolved phone number

    setPendingAction("send_message");
    setAwaitingConfirmation(true);

    // Speak confirmation request
    QString recipient = contactName.isEmpty() ? phoneNumber : contactName;
    QString confirmMsg = QString("Sending message to %1: %2. Should I send it?")
                            .arg(recipient, messageBody);
    speakFeedback(confirmMsg);

    emit confirmationRequested("send_message", confirmMsg);
}

void VoiceCommandHandler::executeReadMessagesCommand(const QVariantMap &command)
{
    QString contactName = command["contact_name"].toString();

    qDebug() << "VoiceCommandHandler: Read messages command - Contact:" << contactName;

    if (!m_messageManager) {
        qWarning() << "VoiceCommandHandler: MessageManager not set";
        speakFeedback("I can't access messages right now");
        emit commandFailed("read_messages", "MessageManager not available");
        return;
    }

    // Get conversation model
    QObject *conversationModel = m_messageManager->property("conversationModel").value<QObject*>();
    if (!conversationModel) {
        speakFeedback("No messages found");
        return;
    }

    // If contact name specified, find their conversation
    if (!contactName.isEmpty()) {
        speakFeedback(QString("Reading messages from %1").arg(contactName));
        // TODO: Implement message reading from MessageManager
    } else {
        // Read recent messages
        speakFeedback("Reading recent messages");
        // TODO: Implement reading recent messages
    }

    emit commandExecuted("read_messages", QString("Reading messages for %1").arg(contactName));
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

            speakFeedback("Message sent");
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

    speakFeedback("Cancelled");

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

void VoiceCommandHandler::speakFeedback(const QString &text)
{
    qDebug() << "VoiceCommandHandler: Speaking:" << text;

    if (m_googleTTS) {
        QMetaObject::invokeMethod(m_googleTTS, "speak",
                                Q_ARG(QString, text));
    }
}
