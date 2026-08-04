#include "ChatBox.h"
#include "ui_ChatBox.h"

#include <QTimer>
#include <QString>
#include <QMessageBox>
#include <QAudioFormat>
#include <QFileDialog>
#include <QJsonObject>

ChatBox::ChatBox(QWidget *parent)
    : ui(new Ui::ChatBox)
    , m_pWidget(nullptr)
    , m_pPipelineBtnGroup(new QButtonGroup(this))
    , m_pTimer(new QTimer(this))
    , m_pAudioFlushTimer(new QTimer(this))
{
}

ChatBox::~ChatBox()
{
    qDebug() << "ChatBox destructor called!";
    delete ui;
}

QWidget *ChatBox::Init(Bus *parent)
{
    m_pBus = parent;

    m_pWidget = _initUI();
    _initConnectsions();
    _retranslate();

    ui->ckLocal->setChecked(true);
    m_pTimer->start(100);

    return m_pWidget;
}

void ChatBox::Shutdown()
{
    qDebug() << "ChatBox::Shutdown() called";

    if(m_pTimer)
        m_pTimer->stop();

    if(m_pAudioFlushTimer)
        m_pAudioFlushTimer->stop();

    if(m_isAudioStarted && m_audioId != -1)
        emit m_pBus->signalAudioCaptureStop(m_audioId);

    m_isAudioStarted = false;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        m_audioBuffer.clear();
    }

    disconnect(this, nullptr, nullptr, nullptr);
}

void ChatBox::_slotPing()
{
    qDebug() << "ChatBox received Ping signal from Bus.";
    emit m_pBus->signalPong();
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
        QVariant::fromValue<qlonglong>(session.id));

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
            QVariant::fromValue<qlonglong>(session.id));
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
        auto currentSessionId = currentItem->data(Qt::UserRole).value<qint64>();
        if(ids.contains(currentSessionId))
            _clearChatBrowser();
    }

    // refresh chat list
    for(int i = ui->listChat->count() - 1; i >= 0; i--)
    {
        auto item = ui->listChat->item(i);
        if(item == nullptr)
            continue;

        qint64 sessionId = item->data(Qt::UserRole).value<qint64>();
        if(ids.contains(sessionId))
        {
            qDebug() << "Delete session with id: " << sessionId;
            ui->listChat->removeItemWidget(item);
        }
    }
}

void ChatBox::_slotQueryResp(const int32_t  errorCode,
                             const int64_t  sessionId,
                             const int64_t  msgId,
                             const QString &resp,
                             const bool     isFinished)
{
    qDebug() << "ChatBox received QueryResp signal from Bus. sessionId: "
             << sessionId << ", msgId: " << msgId << ", resp: " << resp
             << ", isFinished: " << isFinished;
    if(errorCode != 0)
    {
        qDebug() << "Failed to get query response for sessionId: " << sessionId
                 << ", errorCode: " << errorCode;
        auto msg = _convert(
            _gen64(),
            sessionId,
            "assistant",
            tr("Failed to get query response, errorCode: %1").arg(errorCode),
            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
            true);
        _writeBuf(msg);

        _setAnswerFinishState(true);
        return;
    }

    if(_isAnswerFinished())
    {
        qDebug() << "Answer already finished for sessionId: " << sessionId
                 << ", ignoring response.";
        return;
    }

    auto msg =
        _convert(msgId,
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

    for(int i = messages.size() - 1; i >= 0; i--)
    {
        const auto &msg = messages[i];
        if(m_recvdMsgIds.contains(msg.id))
        {
            qDebug() << "Duplicate message received, skipping. id=" << msg.id;
            continue;
        }

        m_recvdMsgIds.insert(msg.id);
        qDebug() << "Message: id=" << msg.id << ", sessionId=" << msg.sessionId
                 << ", role=" << msg.role << ", content=" << msg.content;
        _writeBuf(msg);
    }
}

void ChatBox::_slotModelInfoUpdate(const QVector<Bus::ModelInfo> &modelInfos)
{
    qDebug()
        << "ChatBox received ModelInfoUpdate signal from Bus. model count: "
        << modelInfos.size();
    m_modelInfos.clear();
    for(const auto &model : modelInfos)
    {
        m_modelInfos.append(model);
        qDebug() << "Model id: " << model.id;
    }

    _refreshModelItem();
}

void ChatBox::_slotMemoryInfoUpdate(const QVector<Bus::MemoryInfo> &memoryInfos)
{
    qDebug()
        << "ChatBox received MemoryInfoUpdate signal from Bus. memory count: "
        << memoryInfos.size();
    m_memoryInfos.clear();
    for(const auto &memory : memoryInfos)
    {
        m_memoryInfos.append(memory);
        qDebug() << "Memory id: " << memory.id;
    }

    _refreshMemoryItem();
}

void ChatBox::_slotAudioParamUpdateNtf(const QVector<Bus::AudioParam> &params)
{
    qDebug()
        << "ChatBox received AudioParamUpdate signal from Bus. param count: "
        << params.size();
    m_audioParams.clear();
    for(const auto &param : params)
    {
        m_audioParams.append(param);
        qDebug() << "AudioParam translatorId: " << param.translatorId
                 << ", minNewSampleSize: " << param.minNewSampleSize
                 << ", minAudioBufferSize: " << param.minAudioBufferSize
                 << ", maxAudioBufferSize: " << param.maxAudioBufferSize;
    }
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

    if(data.isEmpty())
        return;

    _appendAudioData(data);

    // lock and flush buffer if size exceeds threshold
    if(_isAudioEnough())
    {
        _flushAudioBuffer(false);
    }
}

void ChatBox::_slotAudioCaptureStopped(const qint64 id)
{
    if(!m_isAudioStarted || m_audioId != id)
        return;

    _setAudioRecordState(false);
}

void ChatBox::_slotRecognizeResp(const int      errorCode,
                                 const QString &transcript,
                                 const bool     isFinished,
                                 const double   confidence)
{
    qDebug() << "Audio recognize with errorCode:" << errorCode
             << ", transcript:" << transcript;
    if(errorCode != 0)
    {
        qDebug() << "Failed to translate audio with errorCode: " << errorCode;
        return;
    }

    if(!m_isAudioStarted)
        return;

    QString content = ui->editInput->toPlainText();
    content += transcript;
    ui->editInput->setText(content);
}

void ChatBox::_slotStopRecognizeResp(const int errorCode, const qint64 streamId)
{
    qDebug() << "Audio stop recognize with errorCode:" << errorCode
             << ", streamId:" << streamId;
}

void ChatBox::_slotUploadResp(const int errorCode, const QString &filePath)
{
    qDebug() << "Upload response received with errorCode:" << errorCode
             << ", filePath:" << filePath;
}

void ChatBox::_slotRetrieveResp(const int                   errorCode,
                                const QString              &question,
                                const int                   topK,
                                const QString              &memoryId,
                                const QVector<QJsonObject> &memorys)
{
    qDebug() << "Retrieve response received with errorCode:" << errorCode
             << ", question:" << question << ", topK:" << topK
             << ", memoryId:" << memoryId
             << ", memorys count:" << memorys.size()
             << ", m_waitRetrieveQuestion:" << m_waitRetrieveQuestion;
    if(question != m_waitRetrieveQuestion)
        return;

    QString        model = ui->comboModel->currentText();
    auto           item  = ui->listChat->currentItem();
    Bus::ModelInfo info;
    for(auto item : m_modelInfos)
    {
        if(item.id != model)
            continue;

        info = item;
        break;
    }
    qint64 sessionId = m_waitRetrieveSessionId;
    qint64 msgId     = m_waitRetrieveMsgId;

    // embedding fail; just query
    if(errorCode != 0)
    {
        info.pipeline = m_pipeline;
        qDebug() << "retrieve failed query with pipeline:" << info.pipeline;
        emit m_pBus->signalQuery(sessionId, msgId, question, model, info);
        return;
    }

    auto query = _buildPrompt(question, memorys);
    qDebug() << "build query:" << query;
    info.pipeline = m_pipeline;
    qDebug() << "query with pipeline:" << info.pipeline;
    _setAnswerFinishState(false);
    emit m_pBus->signalQuery(sessionId, msgId, query, model, info);
}

void ChatBox::_slotBtnStartClicked()
{
    // protect the button
    ui->btnStart->setEnabled(false);
    QTimer::singleShot(1000, this, [=]() { ui->btnStart->setEnabled(true); });

    qDebug() << "Start button clicked. _isAnswerFinished: "
             << _isAnswerFinished();
    if(_isAnswerFinished())
        _query(); // start query
    else
        _stopQuery(); // stop query
}

void ChatBox::_slotBtnAttachClicked()
{
    QStringList filePaths = QFileDialog::getOpenFileNames(
        nullptr,
        tr("Select Files"),
        "",
        tr("All Files (*.*);;Text Files (*.txt);;Images (*.png *.jpg *.jpeg "
           "*.bmp);;PDF Files (*.pdf)"));

    if(filePaths.isEmpty())
        return;

    for(const QString &filePath : filePaths)
    {
        _addAttachedFile(filePath);
        emit m_pBus->signalUpload(filePath);
        qDebug() << "Attach button clicked with filePath: " << filePath;
    }
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

    qint64 sessionId =
        ui->listChat->currentItem()->data(Qt::UserRole).value<qint64>();
    auto msgs = _readRecvedMsgAll(sessionId);
    _refreshChatBrowser(msgs);
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
        if(!msg.isFinished)
            continue;

        if(msg.role != "assistant")
            continue;

        qDebug() << "Found finished answer message. id: " << msg.id
                 << ", sessionId: " << msg.sessionId;
        _setAnswerFinishState(true);
        break;
    }
}

void ChatBox::_refreshModelItem()
{
    if(ui->ckLocal->isChecked())
        m_pipeline = "local";
    else if(ui->ckRemote->isChecked())
        m_pipeline = "remote";
    else if(ui->ckHybrid->isChecked())
        m_pipeline = "hybrid";
    else
        m_pipeline = "local";

    ui->comboModel->clear();
    for(auto item : m_modelInfos)
    {
        if(item.pipeline != m_pipeline && m_pipeline != "hybrid")
            continue;

        ui->comboModel->addItem(item.id);
    }
}

void ChatBox::_refreshMemoryItem()
{
    ui->comboMemory->clear();
    ui->comboMemory->addItem(tr("None"));
    for(auto memory : m_memoryInfos)
        ui->comboMemory->addItem(memory.id);
}

void ChatBox::_setAnswerFinishState(bool isFinish)
{
    QMutexLocker locker(&m_mu);
    // if(m_isAnswerFinished == isFinish)
    //     return;

    qDebug() << "Set answer finish state: " << isFinish;
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

bool ChatBox::_isAnswerFinished()
{
    QMutexLocker locker(&m_mu);
    return m_isAnswerFinished;
}

void ChatBox::_refreshChatBrowser(const QVector<Bus::MessageInfo> &msgs)
{
    qint64 sessionId = -1;
    if(ui->listChat->currentItem())
    {
        sessionId =
            ui->listChat->currentItem()->data(Qt::UserRole).value<qint64>();
    }
    if(sessionId == -1)
    {
        _clearChatBrowser();
        return;
    }

    // refresh buffer msg
    for(auto msg : msgs)
    {
        qDebug() << "Refresh chat browser with selected sessionId=" << sessionId
                 << ", message: id=" << msg.id
                 << ", sessionId=" << msg.sessionId << ", role=" << msg.role
                 << ", content=" << msg.content
                 << ", isFinished=" << msg.isFinished;
        if(sessionId != msg.sessionId)
            continue;

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

    ui->btnAttach->setStyleSheet("QPushButton {"
                                 "   background - color : #7B8DA3;"
                                 "   qproperty - iconSize : 56px 56px;"
                                 "border:"
                                 "   none;"
                                 "}");
    ui->btnAttach->setEnabled(true);
    ui->btnAttach->setIcon(QIcon(":/icons/attach"));

    ui->btnAudioStart->setStyleSheet("QPushButton {"
                                     "   background - color : #7B8DA3;"
                                     "   qproperty - iconSize : 56px 56px;"
                                     "border:"
                                     "   none;"
                                     "}");
    ui->btnAudioStart->setEnabled(true);
    ui->btnAudioStart->setIcon(QIcon(":/icons/audio_norm"));

    ui->listAttachFiles->hide();

    m_pPipelineBtnGroup->addButton(ui->ckLocal, 0);
    m_pPipelineBtnGroup->addButton(ui->ckRemote, 1);
    m_pPipelineBtnGroup->addButton(ui->ckHybrid, 2);
    m_pPipelineBtnGroup->setExclusive(true);

    // start audio flush timer
    m_pAudioFlushTimer->start(m_flushIntervalMs);

    return wgt;
}

void ChatBox::_initConnectsions()
{
    // init BUS connect
    connect(m_pBus, &Bus::signalPing, this, &ChatBox::_slotPing);
    connect(m_pBus,
            &Bus::signalLanguageSwitch,
            this,
            &ChatBox::_slotLanguageSwitch);
    connect(m_pBus,
            &Bus::signalNewSessionResp,
            this,
            &ChatBox::_slotNewSessionResp);
    connect(m_pBus,
            &Bus::signalGetSessionResp,
            this,
            &ChatBox::_slotGetSessionResp);
    connect(m_pBus,
            &Bus::signalDelSessionResp,
            this,
            &ChatBox::_slotDelSessionResp);
    connect(m_pBus,
            &Bus::signalQueryResp,
            this,
            &ChatBox::_slotQueryResp,
            Qt::QueuedConnection);
    connect(m_pBus,
            &Bus::signalStopAnswerResp,
            this,
            &ChatBox::_slotStopAnswerResp);
    connect(m_pBus,
            &Bus::signalGetMessageInfoResp,
            this,
            &ChatBox::_slotGetMessageInfoResp);
    connect(m_pBus,
            &Bus::signalModelInfoUpdateNtf,
            this,
            &ChatBox::_slotModelInfoUpdate);
    connect(m_pBus,
            &Bus::signalMemoryInfoUpdateNtf,
            this,
            &ChatBox::_slotMemoryInfoUpdate);
    connect(m_pBus,
            &Bus::signalAudioParamUpdateNtf,
            this,
            &ChatBox::_slotAudioParamUpdateNtf);
    connect(m_pBus,
            &Bus::signalAudioCaptureStarted,
            this,
            &ChatBox::_slotAudioCaptureStarted);
    connect(m_pBus,
            &Bus::signalAudioCaptured,
            this,
            &ChatBox::_slotAudioCaptured);
    connect(m_pBus,
            &Bus::signalAudioCaptureStopped,
            this,
            &ChatBox::_slotAudioCaptureStopped);
    connect(m_pBus,
            &Bus::signalRecognizeResp,
            this,
            &ChatBox::_slotRecognizeResp);
    connect(m_pBus,
            &Bus::signalStopRecognizeResp,
            this,
            &ChatBox::_slotStopRecognizeResp);
    connect(m_pBus, &Bus::signalUploadResp, this, &ChatBox::_slotUploadResp);
    connect(m_pBus,
            &Bus::signalRetrieveResp,
            this,
            &ChatBox::_slotRetrieveResp);


    // init UI connect
    connect(m_pTimer, &QTimer::timeout, this, &ChatBox::_refreshUI);
    connect(ui->btnStart,
            &QPushButton::clicked,
            this,
            &ChatBox::_slotBtnStartClicked);
    connect(ui->btnAttach,
            &QPushButton::clicked,
            this,
            &ChatBox::_slotBtnAttachClicked);
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

    // init timer connect
    connect(m_pAudioFlushTimer,
            &QTimer::timeout,
            this,
            &ChatBox::_slotFlushAudioBuffer);
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

QVector<Bus::MessageInfo> ChatBox::_readRecvedMsgAll(int64_t sessionId)
{
    QMutexLocker              locker(&m_mu);
    QVector<Bus::MessageInfo> msgs;
    if(m_messageInfos.contains(sessionId))
        msgs = m_messageInfos[sessionId];

    return msgs;
}

void ChatBox::_query()
{
    if(!_isAnswerFinished())
    {
        qDebug() << "Cannot start query, previous answer is not finished.";
        QMessageBox::warning(
            nullptr,
            tr("Query Not Finished"),
            tr("Cannot start query, previous answer is not finished."));
        return;
    }

    QString memory = ui->comboMemory->currentText();
    QString model  = ui->comboModel->currentText();
    QString query  = ui->editInput->toPlainText();
    auto    item   = ui->listChat->currentItem();
    if(item == nullptr)
    {
        qDebug() << "No session selected, create new session with query:"
                 << query;
        QString title = query;
        emit    m_pBus->signalNewSession(title, query, model);
        return;
    }

    qint64 sessionId = item->data(Qt::UserRole).value<qint64>();
    auto   msgId     = _gen64();
    auto   msg =
        _convert(msgId,
                 sessionId,
                 "user",
                 query,
                 QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),
                 true);
    qDebug() << "Start Question for sessionId: " << sessionId
             << ", msgId: " << msgId << ", query: " << query
             << ", model: " << model << ", memory: " << memory;
    _writeBuf(msg);
    ui->editInput->clear();

    // embedding
    if(!memory.isEmpty() && memory != tr("None"))
    {
        qDebug() << "Start embedding for sessionId: " << sessionId
                 << ", memory: " << memory;

        m_waitRetrieveSessionId = sessionId;
        m_waitRetrieveMsgId     = msgId;
        m_waitRetrieveQuestion  = query;
        emit m_pBus->signalRetrieve(query, 5, memory);
        return;
    }

    // query
    Bus::ModelInfo info;
    for(auto item : m_modelInfos)
    {
        if(item.id != model)
            continue;

        info = item;
        break;
    }
    info.pipeline = m_pipeline;
    _setAnswerFinishState(false);
    emit m_pBus->signalQuery(sessionId, msgId, query, model, info);
}

void ChatBox::_stopQuery()
{
    if(_isAnswerFinished())
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

    // clear buffer and set answer finish state
    _setAnswerFinishState(true);
    _readBufAll(); // clear buffer

    qint64 sessionId = item->data(Qt::UserRole).value<qint64>();
    qDebug() << "Stop Question for sessionId: " << sessionId;
    emit m_pBus->signalStopAnswer(sessionId);
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
    emit m_pBus->signalAudioCaptureStart(format, QByteArray());
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

    emit m_pBus->signalAudioCaptureStop(m_audioId);
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

        _clearAudioBuffer();
    } else
    {
        m_isAudioStarted = false;
        m_audioId        = id;

        ui->btnAudioStart->setChecked(true);
        ui->btnAudioStart->setIcon(QIcon(":/icons/audio_norm"));

        _flushAudioBuffer();

        emit m_pBus->signalStopRecognize(id);
    }
}

void ChatBox::_addMsgRecord(const Bus::MessageInfo &msg)
{
    qDebug() << "Add message record: id=" << msg.id
             << ", sessionId=" << msg.sessionId << ", role=" << msg.role
             << ", content=" << msg.content
             << ", isFinished=" << msg.isFinished;
    m_recvdMsgIds.insert(msg.id);
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
    msg.id         = msg_id;
    msg.sessionId  = sessionId;
    msg.role       = role;
    msg.content    = content;
    msg.timestamp  = timestamp;
    msg.isFinished = isFinished;
    return msg;
}

void ChatBox::_appendAudioData(const QByteArray &data)
{
    QMutexLocker locker(&m_audioBufferMutex);
    if(m_audioBuffer.size() + data.size() > _minBufferSize())
        _flushAudioBuffer(false);

    m_audioBuffer.append(data);
}

void ChatBox::_flushAudioBuffer(bool force)
{
    QMutexLocker locker(&m_audioBufferMutex);
    if(m_audioBuffer.isEmpty())
        return;

    if(!force && m_audioBuffer.size() < _minBufferSize() / 2)
        return;

    qDebug() << "Flushing audio buffer, size:" << m_audioBuffer.size()
             << ", force:" << force;

    emit m_pBus->signalRecognize(m_audioId, m_audioBuffer, "");
    m_audioBuffer.clear();
}

void ChatBox::_clearAudioBuffer()
{
    QMutexLocker locker(&m_audioBufferMutex);
    m_audioBuffer.clear();
}

void ChatBox::_slotFlushAudioBuffer()
{
    if(m_isAudioStarted)
    {
        _flushAudioBuffer(false);
    }
}

bool ChatBox::_isAudioEnough()
{
    QMutexLocker locker(&m_audioBufferMutex);
    return m_audioBuffer.size() >= _minBufferSize();
}

bool ChatBox::_isAudioOverflow()
{
    QMutexLocker locker(&m_audioBufferMutex);
    return m_audioBuffer.size() >= _maxBufferSize();
}

int ChatBox::_minBufferSize()
{
    if(m_audioParams.isEmpty())
        return 16000 * 2; // 16kHz * 2 bytes per sample

    return m_audioParams.first().minAudioBufferSize;
}

int ChatBox::_maxBufferSize()
{
    if(m_audioParams.isEmpty())
        return 64000 * 2; // 64k

    return m_audioParams.first().maxAudioBufferSize;
}

void ChatBox::_addAttachedFile(const QString &filePath)
{
    if(m_attachedFiles.contains(filePath))
    {
        qDebug() << "File already attached: " << filePath;
        return;
    }

    m_attachedFiles.append(filePath);
    QWidget     *itemWidget = new QWidget(ui->listAttachFiles);
    QHBoxLayout *layout     = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);

    QLabel   *iconLabel = new QLabel(itemWidget);
    QFileInfo fileInfo(filePath);
    QIcon     fileIcon;
    if(fileInfo.suffix().toLower() == "txt")
        fileIcon = QIcon(":/icons/file_txt");
    else if(fileInfo.suffix().toLower() == "pdf")
        fileIcon = QIcon(":/icons/file_pdf");
    else if(fileInfo.suffix().toLower() == "png"
            || fileInfo.suffix().toLower() == "jpg"
            || fileInfo.suffix().toLower() == "jpeg")
        fileIcon = QIcon(":/icons/file_image");
    else
        fileIcon = QIcon(":/icons/file");
    iconLabel->setPixmap(fileIcon.pixmap(24, 24));
    layout->addWidget(iconLabel);

    QLabel *nameLabel = new QLabel(fileInfo.fileName(), itemWidget);
    nameLabel->setStyleSheet("QLabel { font-size: 12px; }");
    layout->addWidget(nameLabel);

    QLabel *sizeLabel =
        new QLabel(tr("%1 KB").arg(fileInfo.size() / 1024.0, 0, 'f', 1),
                   itemWidget);
    sizeLabel->setStyleSheet("QLabel { color: #888888; font-size: 11px; }");
    layout->addWidget(sizeLabel);

    QPushButton *delBtn = new QPushButton("×", itemWidget);
    delBtn->setFixedSize(20, 20);
    delBtn->setStyleSheet("QPushButton {"
                          "   background-color: transparent;"
                          "   color: #999999;"
                          "   border: none;"
                          "   font-size: 16px;"
                          "   font-weight: bold;"
                          "}"
                          "QPushButton:hover {"
                          "   color: #ff0000;"
                          "}");
    connect(delBtn, &QPushButton::clicked, this, [this, filePath]() {
        _removeAttachedFile(filePath);
    });
    layout->addWidget(delBtn);
    layout->addStretch();

    QListWidgetItem *item = new QListWidgetItem(ui->listAttachFiles);
    item->setSizeHint(itemWidget->sizeHint());
    ui->listAttachFiles->addItem(item);
    ui->listAttachFiles->setItemWidget(item, itemWidget);
    ui->listAttachFiles->setVisible(m_attachedFiles.size() > 0);
}

void ChatBox::_removeAttachedFile(const QString &filePath)
{
    int index = m_attachedFiles.indexOf(filePath);
    if(index == -1)
        return;

    m_attachedFiles.removeAt(index);
    QListWidgetItem *item = ui->listAttachFiles->takeItem(index);
    if(item)
    {
        delete item->listWidget();
        delete item;
    }

    ui->listAttachFiles->setVisible(m_attachedFiles.size() > 0);
    qDebug() << "Removed attached file: " << filePath;
}

void ChatBox::_clearAttachedFiles()
{
    m_attachedFiles.clear();
    ui->listAttachFiles->clear();
    ui->listAttachFiles->setVisible(false);
}

QString ChatBox::_buildPrompt(const QString              &question,
                              const QVector<QJsonObject> &memorys,
                              const QString              &lang)
{
    QVector<QString> references;
    for(auto memory : memorys)
    {
        auto chunkId      = memory["chunk_id"].toInt(0);
        auto data         = memory["data"].toString("");
        auto startPos     = memory["start_pos"].toInt(0);
        auto endPos       = memory["end_pos"].toInt(0);
        auto chunkSize    = memory["chunk_size"].toInt(0);
        auto filePathName = memory["file_path_name"].toString();
        auto timestamp    = memory["timestamp"].toString();

        references.append(data);
        qDebug() << "add data:" << data;
    }

    QStringList parts;
    if(lang == "zh_CN")
    {
        parts << "请根据以下参考资料回答用户的问题。";
        parts << "";
        parts << "参考资料：";
        for(int i = 0; i < references.size(); ++i)
        {
            parts << QString("【文档 %1】").arg(i + 1);
            parts << references[i];
            parts << "";
        }

        parts << QString("用户问题：%1").arg(question);
        parts << "回答：";
        return parts.join("\n");
    } else
    {
        parts << "Based on the following references, please answer the user's "
                 "question.";
        parts << "";
        parts << "References:";
        for(int i = 0; i < references.size(); ++i)
        {
            parts << QString("[Document %1]").arg(i + 1);
            parts << references[i];
            parts << "";
        }

        parts << QString("User Question: %1").arg(question);
        parts << "Answer:";
        return parts.join("\n");
    }

    return question;
}