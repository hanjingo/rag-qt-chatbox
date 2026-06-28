#include "ChatBox.h"
#include "ui_ChatBox.h"

#include <QString>
#include <QMessageBox>

ChatBox::ChatBox(QWidget *parent)
    : ui(new Ui::ChatBox)
    , m_pPipelineBtnGroup(new QButtonGroup(this))
{
}

ChatBox::~ChatBox()
{
    delete ui;
}

QWidget *ChatBox::Init(Bus *parent)
{
    m_pBus = parent;

    // create UI
    auto wgt = new QWidget(nullptr);
    wgt->setStyleSheet("background-color: transparent;");
    ui->setupUi(wgt);

    ui->listChat->setEditTriggers(QAbstractItemView::DoubleClicked
                                  | QAbstractItemView::SelectedClicked);

    ui->txtBrowserChat->setFocusPolicy(Qt::NoFocus);
    ui->txtBrowserChat->setOpenExternalLinks(true);
    ui->txtBrowserChat->setStyleSheet(
        "QTextBrowser {"
        "   background-color: #ffffff;"
        "   border: 1px solid #cccccc;"
        "   font-family: 'Microsoft YaHei', sans-serif;"
        "   font-size: 14px;"
        "}");

    ui->btnStart->setEnabled(true);
    ui->btnStart->setIcon(QIcon(":/icons/send"));

    m_pPipelineBtnGroup->addButton(ui->ckLocal);
    m_pPipelineBtnGroup->addButton(ui->ckRemote);
    m_pPipelineBtnGroup->addButton(ui->ckHybrid);
    m_pPipelineBtnGroup->setExclusive(true);
    ui->ckLocal->click();

    // init BUS connect
    connect(m_pBus, &Bus::SignalPing, this, &ChatBox::_slotPing);
    connect(m_pBus,
            &Bus::SignalLanguageSwitch,
            this,
            &ChatBox::_slotLanguageSwitch);
    connect(m_pBus,
            &Bus::SignalNewSessionResp,
            this,
            &ChatBox::_slotNewSessionResp);
    connect(m_pBus,
            &Bus::SignalGetSessionResp,
            this,
            &ChatBox::_slotGetSessionResp);
    connect(m_pBus,
            &Bus::SignalDelSessionResp,
            this,
            &ChatBox::_slotDelSessionResp);
    connect(m_pBus, &Bus::SignalQueryResp, this, &ChatBox::_slotQueryResp);
    connect(m_pBus,
            &Bus::SignalStopAnswerResp,
            this,
            &ChatBox::_slotStopAnswerResp);
    connect(m_pBus,
            &Bus::SignalGetMessageInfoResp,
            this,
            &ChatBox::_slotGetMessageInfoResp);
    connect(m_pBus,
            &Bus::SignalModelInfoUpdateNtf,
            this,
            &ChatBox::_slotModelInfoUpdate);

    // init UI connect
    connect(ui->btnStart,
            &QPushButton::clicked,
            this,
            &ChatBox::_slotBtnStartClicked);
    connect(ui->listChat,
            &QListWidget::currentRowChanged,
            this,
            &ChatBox::_slotCurrentRowChanged);
    connect(m_pPipelineBtnGroup,
            &QButtonGroup::idClicked,
            this,
            &ChatBox::_slotPipelineBtnGroupClicked);

    return wgt;
}

void ChatBox::_slotPing()
{
    qDebug() << "ChatBox received Ping signal from Bus.";
    emit m_pBus->SignalPong();
}

void ChatBox::_slotLanguageSwitch(const QString &lang)
{
    qDebug() << "ChatBox received LanguageSwitch signal from Bus. lang: "
             << lang;
}

void ChatBox::_slotNewSessionResp(const int32_t       errorCode,
                                  const Bus::Session &session)
{
    qDebug() << "ChatBox received NewSessionResp signal from Bus. errorCode: "
             << errorCode << ", sessionId: " << session.id
             << ", title: " << session.title;
    if(errorCode != 0)
    {
        qDebug() << "Failed to create new session with title: "
                 << session.title;
        QMessageBox::warning(nullptr,
                             tr("Create Session Failed"),
                             tr("Failed to create new session with title: %1")
                                 .arg(session.title));
        return;
    }

    auto item = new QListWidgetItem(session.title);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    ui->listChat->addItem(item);
    ui->listChat->setCurrentItem(item);
    ui->listChat->currentItem()->setData(
        Qt::UserRole,
        QVariant::fromValue(static_cast<qlonglong>(session.id)));

    // query question
    _query();
}

void ChatBox::_slotGetSessionResp(const int                    errorCode,
                                  const QVector<Bus::Session> &sessions)
{
    qDebug() << "ChatBox: GetSessionResp";
    if(errorCode != 0)
    {
        qDebug() << "Failed to get sessions from Bus. errorCode: " << errorCode;
        return;
    }

    ui->listChat->clear();
    for(auto session : sessions)
    {
        auto item = new QListWidgetItem(session.title);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->listChat->addItem(item);
        ui->listChat->setCurrentItem(item);
        ui->listChat->currentItem()->setData(
            Qt::UserRole,
            QVariant::fromValue(static_cast<qlonglong>(session.id)));
    }
}

void ChatBox::_slotDelSessionResp(const int               errorCode,
                                  const QVector<int64_t> &ids)
{
    qDebug() << "ChatBox: DelSessionResp";
    if(errorCode != 0)
    {
        qDebug() << "Failed to delete sessions from Bus. errorCode: "
                 << errorCode;
        return;
    }

    auto currentItem = ui->listChat->currentItem();
    for(int i = ui->listChat->count() - 1; i >= 0; i--)
    {
        auto item = ui->listChat->item(i);
        if(item == nullptr)
            continue;

        auto sessionId = item->data(Qt::UserRole).toLongLong();
        if(ids.contains(sessionId))
        {
            qDebug() << "Delete session with id: " << sessionId;
            ui->listChat->removeItemWidget(item);
        }
    }

    // refresh
    _refreshChatBrowser();
}

void ChatBox::_slotQueryResp(const int32_t  errorCode,
                             const int64_t  sessionId,
                             const QString &resp,
                             const bool     isFinished)
{
    qDebug() << "ChatBox received QueryResp signal from Bus. sessionId: "
             << sessionId << ", resp: " << resp
             << ", isFinished: " << isFinished;
    if(errorCode != 0)
    {
        qDebug() << "Failed to get query response for sessionId: " << sessionId
                 << ", errorCode: " << errorCode;
        _drawAnswerRecord(
            tr("Failed to get query response for sessionId: %1, errorCode: %2")
                .arg(sessionId)
                .arg(errorCode),
            true);
        _refreshAnswerFinishState(true);
        return;
    }

    _drawAnswerRecord(resp, isFinished);
    _refreshAnswerFinishState(isFinished);
    _addMsgRecord(sessionId,
                  "assistant",
                  resp,
                  QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
                  isFinished);
    return;
}

void ChatBox::_slotStopAnswerResp(const int64_t errorCode,
                                  const int64_t sessionId)
{
    qDebug() << "ChatBox received StopAnswerResp signal from Bus. sessionId: "
             << sessionId << ", errorCode: " << errorCode;
    if(errorCode != 0)
    {
        qDebug() << "Failed to stop answer for sessionId: " << sessionId
                 << ", errorCode: " << errorCode;
        QMessageBox::warning(nullptr,
                             tr("Stop Answer Failed"),
                             tr("Failed to stop answer for sessionId: %1, "
                                "errorCode: %2")
                                 .arg(sessionId)
                                 .arg(errorCode));
        return;
    }

    _refreshAnswerFinishState(true);
}

void ChatBox::_slotGetMessageInfoResp(const int errorCode,
                                      const QVector<Bus::MessageInfo> &messages)
{
    qDebug() << "ChatBox received GetMessageInfoResp signal from Bus. message "
                "count: "
             << messages.size();
    if(errorCode != 0)
    {
        qDebug() << "Failed to get messages for sessionId: "
                 << (messages.isEmpty() ? -1 : messages.first().sessionId);
        QMessageBox::warning(
            nullptr,
            tr("Get Message Failed"),
            tr("Failed to get messages for sessionId: %1")
                .arg(messages.isEmpty() ? -1 : messages.first().sessionId));
        return;
    }

    m_messageInfos.clear();
    for(int i = messages.size() - 1; i >= 0; i--)
    {
        const auto &msg = messages[i];
        qDebug() << "Message: id=" << msg.id << ", sessionId=" << msg.sessionId
                 << ", role=" << msg.role << ", content=" << msg.content;
        _addMsgRecord(msg.sessionId,
                      msg.role,
                      msg.content,
                      msg.timestamp,
                      true);
    }
    _refreshChatBrowser();
}

void ChatBox::_slotModelInfoUpdate(const QVector<Bus::ModelConfig> &modelInfos)
{
    qDebug()
        << "ChatBox received ModelInfoUpdate signal from Bus. model count: "
        << modelInfos.size();
    m_modelInfos.clear();
    for(const auto &model : modelInfos)
    {
        m_modelInfos.append(model);
        qDebug() << "Model id: " << model.id << ", addr: " << model.addr;
    }

    // refresh model combo box
    _refreshModelItem();
}

void ChatBox::_slotBtnStartClicked()
{
    if(m_isAnswerFinished) // start query
        _query();
    else // stop query
        _stopQuery();
}

void ChatBox::_slotCurrentRowChanged(int row)
{
    if(row < 0 || row > ui->listChat->count() || !ui->listChat->currentItem())
        return;

    auto sessionId =
        ui->listChat->currentItem()->data(Qt::UserRole).toLongLong();

    auto messages =
        m_messageInfos.value(sessionId, QVector<Bus::MessageInfo>());
    ui->txtBrowserChat->clear();
    for(const auto &msg : messages)
    {
        if(msg.role == "user")
            _drawQueryRecord(msg.content);
        else
            _drawAnswerRecord(msg.content, true);
    }
}

void ChatBox::_slotPipelineBtnGroupClicked(int id)
{
    _refreshModelItem();
}

void ChatBox::_drawQueryRecord(const QString &query)
{
    qDebug() << "Add Query Record: " << query;
    QString tm   = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString html = QString(R"(
        <table width="100%" border="0" cellspacing="0" cellpadding="0" style="margin-bottom: 10px;">
            <tr>
                <td align="right">
                    <span style="color: #888888; font-size: 11px; margin-right: 5px;">%1</span>
                    <br>
                    <table bgcolor="#d5f5e3" style="border-radius: 10px; margin-top: 3px;" cellpadding="6">
                        <tr>
                            <td style="color: #000000; font-size: 14px; text-align: left;">%2</td>
                        </tr>
                    </table>
                </td>
            </tr>
        </table>
    )")
                       .arg(tm, query);

    ui->txtBrowserChat->append(html);

    QTextCursor cursor = ui->txtBrowserChat->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->txtBrowserChat->setTextCursor(cursor);
}

void ChatBox::_drawAnswerRecord(const QString &answer, bool isFinished)
{
    qDebug() << "Add answer record:" << answer;
    QTextCursor cursor = ui->txtBrowserChat->textCursor();
    if(m_streamStartPos == -1) // new answer
    {
        m_streamTimestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        m_streamingAnswer = answer;

        QString html = QString(R"(
            <table width="100%" border="0" cellspacing="0" cellpadding="0" style="margin-bottom: 10px;">
            <tr>
                <td align="left">
                    <span style="color: #888888; font-size: 11px; margin-left: 5px;">%1</span><br>
                    <table bgcolor="#f1f1f1" style="border-radius: 10px; margin-top: 3px;" cellpadding="6">
                    <tr><td style="color: #000000; font-size: 14px; text-align: left;">%2</td></tr>
                    </table>
                </td>
            </tr>
            </table>
        )")
                           .arg(m_streamTimestamp, m_streamingAnswer);

        cursor.movePosition(QTextCursor::End);
        m_streamStartPos = cursor.position();
        cursor.insertHtml(html);
    } else // update existing answer
    {
        m_streamingAnswer += answer;
        QString html = QString(R"(
            <table width="100%" border="0" cellspacing="0" cellpadding="0" style="margin-bottom: 10px;">
            <tr>
                <td align="left">
                    <span style="color: #888888; font-size: 11px; margin-left: 5px;">%1</span><br>
                    <table bgcolor="#f1f1f1" style="border-radius: 10px; margin-top: 3px;" cellpadding="6">
                    <tr><td style="color: #000000; font-size: 14px; text-align: left;">%2</td></tr>
                    </table>
                </td>
            </tr>
            </table>
        )")
                           .arg(m_streamTimestamp, m_streamingAnswer);

        cursor.setPosition(m_streamStartPos);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cursor.insertHtml(html);
    }

    cursor.movePosition(QTextCursor::End);
    ui->txtBrowserChat->setTextCursor(cursor);

    if(isFinished)
    {
        m_streamStartPos = -1;
        m_streamingAnswer.clear();
        m_streamTimestamp.clear();
        qDebug() << "Answer finished, reset streaming state.";
    }

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ChatBox::_refreshModelItem()
{
    QString pipeline;
    if(ui->ckLocal->isChecked())
        pipeline = "local";
    else if(ui->ckRemote->isChecked())
        pipeline = "remote";
    else if(ui->ckHybrid->isChecked())
        pipeline = "hybrid";
    else
        pipeline = "local";

    ui->comboModel->clear();
    for(auto item : m_modelInfos)
    {
        if(item.pipeline != pipeline)
            continue;

        ui->comboModel->addItem(item.id);
    }
}

void ChatBox::_refreshAnswerFinishState(bool isFinished)
{
    m_isAnswerFinished = isFinished;
    if(m_isAnswerFinished)
    {
        m_streamStartPos = -1;
        m_streamingAnswer.clear();
        m_streamTimestamp.clear();

        ui->btnStart->setChecked(false);
        ui->btnStart->setIcon(QIcon(":/icons/send"));
        qDebug() << "Answer finished, reset streaming state.";
    } else
    {
        ui->btnStart->setChecked(true);
        ui->btnStart->setIcon(QIcon(":/icons/pause_check"));
        qDebug() << "Answer not finished, keep streaming state.";
    }
}

void ChatBox::_refreshChatBrowser()
{
    ui->txtBrowserChat->clear();
    auto sessionId = -1;
    if(ui->listChat->currentItem())
    {
        sessionId =
            ui->listChat->currentItem()->data(Qt::UserRole).toLongLong();
    }
    if(sessionId == -1)
        return;

    auto msgs = m_messageInfos.value(sessionId, QVector<Bus::MessageInfo>());
    for(const auto &msg : msgs)
    {
        qDebug() << "Message: id=" << msg.id << ", sessionId=" << msg.sessionId
                 << ", role=" << msg.role << ", content=" << msg.content;
        if(msg.role == "user")
            _drawQueryRecord(msg.content);
        else
            _drawAnswerRecord(msg.content, true);
    }
}

void ChatBox::_query()
{
    QString model = ui->comboModel->currentText();
    QString query = ui->editInput->toPlainText();
    auto    item  = ui->listChat->currentItem();
    if(item == nullptr)
    {
        qDebug() << "No session selected, create new session with query:"
                 << query;
        QString title = query;
        emit    m_pBus->SignalNewSession(title, query, model);
        return;
    }

    auto sessionId = item->data(Qt::UserRole).toLongLong();
    qDebug() << "Start Question for sessionId: " << sessionId;
    _drawQueryRecord(query);
    _refreshAnswerFinishState(false);
    _addMsgRecord(sessionId,
                  "user",
                  query,
                  QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
                  true);
    ui->editInput->clear();

    Bus::ModelConfig config;
    for(auto item : m_modelInfos)
    {
        if(item.id != model)
            continue;

        config = item;
    }
    emit m_pBus->SignalQuery(sessionId, query, model, config);
}

void ChatBox::_stopQuery()
{
    auto item = ui->listChat->currentItem();
    if(item == nullptr)
    {
        qDebug() << "No session selected, cannot stop query.";
        return;
    }

    auto sessionId = item->data(Qt::UserRole).toLongLong();
    qDebug() << "Stop Question for sessionId: " << sessionId;
    emit m_pBus->SignalStopAnswer(sessionId);
}

void ChatBox::_addMsgRecord(const int64_t  sessionId,
                            const QString &role,
                            const QString &content,
                            const QString &timestamp,
                            const bool     isFinished)
{
    if(!m_messageInfos.contains(sessionId))
    {
        m_messageInfos[sessionId] = QVector<Bus::MessageInfo>();
    }

    if(m_messageInfos[sessionId].empty())
    {
        Bus::MessageInfo msg;
        msg.id        = -1;
        msg.sessionId = sessionId;
        msg.role      = role;
        msg.content   = content;
        msg.timestamp = timestamp;
        m_messageInfos[sessionId].append(msg);
        m_isLastMsgFinished = isFinished;
        return;
    }

    if(m_isLastMsgFinished)
    {
        Bus::MessageInfo msg;
        msg.id        = -1;
        msg.sessionId = sessionId;
        msg.role      = role;
        msg.content   = content;
        msg.timestamp = timestamp;
        m_messageInfos[sessionId].append(msg);
        m_isLastMsgFinished = isFinished;
        return;
    }

    if(m_messageInfos[sessionId].last().role != role)
    {
        Bus::MessageInfo msg;
        msg.id        = -1;
        msg.sessionId = sessionId;
        msg.role      = role;
        msg.content   = content;
        msg.timestamp = timestamp;
        m_messageInfos[sessionId].append(msg);
        m_isLastMsgFinished = isFinished;
        return;
    }

    m_messageInfos[sessionId].last().content += content;
    m_isLastMsgFinished = isFinished;
}