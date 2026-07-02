#include "ChatBox.h"
#include "ui_ChatBox.h"

#include <QTimer>
#include <QString>
#include <QMessageBox>
#include <QAudioFormat>

ChatBox::ChatBox(QWidget *parent)
    : ui(new Ui::ChatBox)
    , m_pPipelineBtnGroup(new QButtonGroup(this))
    , m_pTimer(new QTimer(this))
{
}

ChatBox::~ChatBox()
{
    delete ui;
}

QWidget *ChatBox::Init(Bus *parent)
{
    m_pBus = parent;

    auto wgt = _initUI();
    _initConnectsions();
    _retranslate();

    ui->ckLocal->setChecked(true);
    m_pTimer->start(100);

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

    // refresh chat history
    auto currentItem = ui->listChat->currentItem();
    if(currentItem)
    {
        auto currentSessionId = currentItem->data(Qt::UserRole).toLongLong();
        if(ids.contains(currentSessionId))
            _clearChatBrowser();
    }

    // refresh chat list
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
        auto msg = _convert(
            -1,
            sessionId,
            "assistant",
            tr("Failed to get query response, errorCode: %1").arg(errorCode),
            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
            true);
        _writeBuf(msg);
        return;
    }

    auto msg =
        _convert(-1,
                 sessionId,
                 "assistant",
                 resp,
                 QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
                 isFinished);
    _writeBuf(msg);
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
        _writeBuf(msg);
    }
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

    _refreshModelItem();
}

void ChatBox::_slotAudioCaptureStarted(const qint64 id, const QByteArray devId)
{
    if(m_isAudioStarted || (m_audioId != -1 && m_audioId != id))
        return;

    _setAudioRecordState(true, id);
}

void ChatBox::_slotAudioCaptured(const qint64 id, const QByteArray &data)
{
    // qDebug() << "Audio Captured in:" << data;
    if(!m_isAudioStarted || m_audioId != id)
        return;

    qDebug() << "Audio Captured in ChatBox. id: " << id
             << ", data size: " << data.size();
    emit m_pBus->SignalAudioTranslate(data, QString());
}

void ChatBox::_slotAudioCaptureStopped(const qint64 id)
{
    if(!m_isAudioStarted || m_audioId != id)
        return;

    _setAudioRecordState(false);
}

void ChatBox::_slotAudioTranslated(const int               errorCode,
                                   const QByteArray       &src,
                                   const QVector<QString> &segments)
{
    qDebug() << "Audio translated with errorCode:" << errorCode
             << ", segments:" << segments;
    QString content = ui->editInput->toPlainText();
    for(auto seg : segments)
        content.append(seg);

    ui->editInput->setText(content);
}

void ChatBox::_slotBtnStartClicked()
{
    qDebug() << "Start button clicked. m_isAnswerFinished: "
             << m_isAnswerFinished;
    // ui->btnStart->setEnabled(false);
    // QTimer::singleShot(3000, this, [=]() { ui->btnStart->setEnabled(true); });

    if(m_isAnswerFinished) // start query
        _query();
    else // stop query
        _stopQuery();
}

void ChatBox::_slotBtnAudioStartClicked()
{
    qDebug() << "Audio start button clicked. m_isAudioStarted:"
             << m_isAudioStarted;

    if(m_isAudioStarted) // stop record
        _stopAudioRecord();
    else // start record
        _startAudioRecord();
}

void ChatBox::_slotCurrentRowChanged(int row)
{
    if(row < 0 || row > ui->listChat->count() || !ui->listChat->currentItem())
        return;

    _clearChatBrowser();
}

void ChatBox::_slotPipelineBtnGroupClicked(int id)
{
    qDebug() << "Pipeline button group clicked. id: " << id;
    _refreshModelItem();
}

void ChatBox::_refreshUI()
{
    auto msgs = _readBufAll();
    for(auto msg : msgs)
    {
        _addMsgRecord(msg);
    }

    // refresh chat browser
    _refreshChatBrowser(msgs);

    // refresh answer finish state
    for(auto msg : msgs)
    {
        _setAnswerFinishState(msg.isFinished);
    }
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

void ChatBox::_setAnswerFinishState(bool isFinish)
{
    if(m_isAnswerFinished == isFinish)
        return;

    m_isAnswerFinished = isFinish;
    if(m_isAnswerFinished)
    {
        m_streamStartPos = -1;
        m_streamingAnswer.clear();
        m_streamTimestamp.clear();

        ui->btnStart->setChecked(false);
        ui->btnStart->setIcon(QIcon(":/icons/send"));
    } else
    {
        ui->btnStart->setChecked(true);
        ui->btnStart->setIcon(QIcon(":/icons/pause_check"));
    }
}

void ChatBox::_refreshChatBrowser(const QVector<Bus::MessageInfo> &msgs)
{
    auto sessionId = -1;
    if(ui->listChat->currentItem())
    {
        sessionId =
            ui->listChat->currentItem()->data(Qt::UserRole).toLongLong();
    }
    if(sessionId == -1)
    {
        _clearChatBrowser();
        return;
    }

    // refresh buffer msg
    for(auto msg : msgs)
    {
        if(msg.role == "user")
            _drawQueryRecord(msg.content);
        else
            _drawAnswerRecord(msg.content, msg.isFinished);
    }
}

void ChatBox::_drawQueryRecord(const QString &query)
{
    qDebug() << "Draw Query Record: " << query;
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
    qDebug() << "Draw answer record:" << answer;
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
}

void ChatBox::_clearChatBrowser()
{
    ui->txtBrowserChat->clear();
}

void ChatBox::_retranslate()
{
    // TODO retranslate language
}

QWidget *ChatBox::_initUI()
{
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

    ui->btnStart->setStyleSheet("QPushButton {"
                                "   background - color : #7B8DA3;"
                                "   qproperty - iconSize : 56px 56px;"
                                "border:"
                                "   none;"
                                "}");
    ui->btnStart->setEnabled(true);
    ui->btnStart->setIcon(QIcon(":/icons/send"));

    ui->btnAudioStart->setStyleSheet("QPushButton {"
                                     "   background - color : #7B8DA3;"
                                     "   qproperty - iconSize : 56px 56px;"
                                     "border:"
                                     "   none;"
                                     "}");
    ui->btnAudioStart->setEnabled(true);
    ui->btnAudioStart->setIcon(QIcon(":/icons/audio_norm"));

    m_pPipelineBtnGroup->addButton(ui->ckLocal, 0);
    m_pPipelineBtnGroup->addButton(ui->ckRemote, 1);
    m_pPipelineBtnGroup->addButton(ui->ckHybrid, 2);
    m_pPipelineBtnGroup->setExclusive(true);

    return wgt;
}

void ChatBox::_initConnectsions()
{
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
    connect(m_pBus,
            &Bus::SignalQueryResp,
            this,
            &ChatBox::_slotQueryResp,
            Qt::QueuedConnection);
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
    connect(m_pBus,
            &Bus::SignalAudioCaptureStarted,
            this,
            &ChatBox::_slotAudioCaptureStarted);
    connect(m_pBus,
            &Bus::SignalAudioCaptured,
            this,
            &ChatBox::_slotAudioCaptured);
    connect(m_pBus,
            &Bus::SignalAudioCaptureStopped,
            this,
            &ChatBox::_slotAudioCaptureStopped);
    connect(m_pBus,
            &Bus::SignalAudioTranslated,
            this,
            &ChatBox::_slotAudioTranslated);

    // init UI connect
    connect(m_pTimer, &QTimer::timeout, this, &ChatBox::_refreshUI);
    connect(ui->btnStart,
            &QPushButton::clicked,
            this,
            &ChatBox::_slotBtnStartClicked);
    connect(ui->btnAudioStart,
            &QPushButton::clicked,
            this,
            &ChatBox::_slotBtnAudioStartClicked);
    connect(ui->listChat,
            &QListWidget::currentRowChanged,
            this,
            &ChatBox::_slotCurrentRowChanged);
    connect(m_pPipelineBtnGroup,
            &QButtonGroup::idClicked,
            this,
            &ChatBox::_slotPipelineBtnGroupClicked);
}

void ChatBox::_writeBuf(const Bus::MessageInfo &msg)
{
    QMutexLocker locker(&m_mu);
    m_buf.append(msg);
}

QVector<Bus::MessageInfo> ChatBox::_readBufAll()
{
    QMutexLocker              locker(&m_mu);
    QVector<Bus::MessageInfo> msgs;
    msgs = m_buf;
    m_buf.clear();
    return msgs;
}

void ChatBox::_query()
{
    if(!m_isAnswerFinished)
    {
        qDebug() << "Cannot start query, previous answer is not finished.";
        QMessageBox::warning(
            nullptr,
            tr("Query Not Finished"),
            tr("Cannot start query, previous answer is not finished."));
        return;
    }

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
    auto msg =
        _convert(-1,
                 sessionId,
                 "user",
                 query,
                 QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
                 true);
    _writeBuf(msg);
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
    if(m_isAnswerFinished)
    {
        qDebug() << "Cannot stop query, answer is already finished.";
        QMessageBox::warning(
            nullptr,
            tr("Stop Query Failed"),
            tr("Cannot stop query, answer is already finished."));
        return;
    }

    auto item = ui->listChat->currentItem();
    if(item == nullptr)
    {
        qDebug() << "No session selected, cannot stop query.";
        QMessageBox::warning(nullptr,
                             tr("Stop Query Failed"),
                             tr("No session selected, cannot stop query."));
        return;
    }

    auto sessionId = item->data(Qt::UserRole).toLongLong();
    qDebug() << "Stop Question for sessionId: " << sessionId;
    emit m_pBus->SignalStopAnswer(sessionId);
}

void ChatBox::_startAudioRecord()
{
    if(m_isAudioStarted)
    {
        qDebug() << "Cannot start audio record, is already started.";
        QMessageBox::warning(nullptr,
                             tr("Start Audio Record Failed"),
                             tr("Cannot start audio record, previous recording "
                                "is not finished."));
        return;
    }

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    emit m_pBus->SignalAudioCaptureStart(format, QByteArray());
    qDebug() << "Start Audio Record with SampleRate:" << format.sampleRate()
             << ", ChannelCount:" << format.channelCount()
             << ", SampleFormat:Int16";
}

void ChatBox::_stopAudioRecord()
{
    if(!m_isAudioStarted)
    {
        qDebug() << "Cannot stop audio record, is already stopped.";
        return;
    }

    emit m_pBus->SignalAudioCaptureStop(m_audioId);
    qDebug() << "Stop Audio Record with audioId:" << m_audioId;
}

void ChatBox::_setAudioRecordState(bool isStarted, qint64 id)
{
    if(isStarted)
    {
        m_isAudioStarted = true;
        m_audioId        = id;

        ui->btnAudioStart->setChecked(false);
        ui->btnAudioStart->setIcon(QIcon(":/icons/audio_recording"));
    } else
    {
        m_isAudioStarted = false;
        m_audioId        = id;

        ui->btnAudioStart->setChecked(true);
        ui->btnAudioStart->setIcon(QIcon(":/icons/audio_norm"));
    }
}

void ChatBox::_addMsgRecord(const Bus::MessageInfo &msg)
{
    if(!m_messageInfos.contains(msg.sessionId))
    {
        m_messageInfos[msg.sessionId] = QVector<Bus::MessageInfo>();
    }

    if(m_messageInfos[msg.sessionId].empty())
    {
        m_messageInfos[msg.sessionId].append(msg);
        m_isLastMsgFinished = msg.isFinished;
        return;
    }

    if(m_isLastMsgFinished)
    {
        m_messageInfos[msg.sessionId].append(msg);
        m_isLastMsgFinished = msg.isFinished;
        return;
    }

    if(m_messageInfos[msg.sessionId].last().role != msg.role)
    {
        m_messageInfos[msg.sessionId].append(msg);
        m_isLastMsgFinished = msg.isFinished;
        return;
    }

    m_messageInfos[msg.sessionId].last().content += msg.content;
    m_isLastMsgFinished = msg.isFinished;
}

Bus::MessageInfo ChatBox::_convert(const int64_t  msg_id,
                                   const int64_t  sessionId,
                                   const QString &role,
                                   const QString &content,
                                   const QString &timestamp,
                                   const bool     isFinished)
{
    Bus::MessageInfo msg;
    msg.id         = -1;
    msg.sessionId  = sessionId;
    msg.role       = role;
    msg.content    = content;
    msg.timestamp  = timestamp;
    msg.isFinished = isFinished;
    return msg;
}