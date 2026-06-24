#include "ChatBox.h"
#include "ui_ChatBox.h"

#include <QString>
#include <QMessageBox>

ChatBox::ChatBox(QWidget *parent)
    : ui(new Ui::ChatBox)
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

    ui->ckLocal->setChecked(true);
    ui->ckRemote->setChecked(false);

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
    auto query = ui->editInput->toPlainText();
    if(query.isEmpty())
        return;

    auto model = ui->comboModel->currentText();
    qDebug() << "ChatBox: New session created, now query with sessionId: "
             << session.id << ", query: " << query << ", model: " << model;

    _addQueryRecord(query);
    ui->btnStart->pressed();
    ui->btnStart->setIcon(QIcon(":/icons/pause_check"));
    ui->editInput->clear();
    emit m_pBus->SignalQuery(session.id, query, model);
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
            if(currentItem == item)
                ui->txtBrowserChat->clear();

            delete item;
        }
    }

    // repull the session list from the server to refresh the list
    emit m_pBus->SignalGetSession(-1, 50);
}

void ChatBox::_slotQueryResp(const int32_t  errorCode,
                             const int64_t  sessionId,
                             const QString &resp)
{
    qDebug() << "ChatBox received QueryResp signal from Bus. sessionId: "
             << sessionId << ", resp: " << resp;
    if(errorCode != 0)
    {
        qDebug() << "Failed to get query response for sessionId: " << sessionId;
        QMessageBox::warning(
            nullptr,
            tr("Query Failed"),
            tr("Failed to get query response:%1 for sessionId: %2")
                .arg(resp)
                .arg(sessionId));
        return;
    }

    _addAnswerRecord(resp);
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

    auto sessionId = -1;
    if(ui->listChat->currentItem())
        sessionId =
            ui->listChat->currentItem()->data(Qt::UserRole).toLongLong();
    for(const auto &msg : messages)
    {
        if(sessionId == -1 || msg.sessionId != sessionId)
            continue;

        qDebug() << "Message: id=" << msg.id << ", sessionId=" << msg.sessionId
                 << ", role=" << msg.role << ", content=" << msg.content;
        if(msg.role == "user")
            _addQueryRecord(msg.content);
        else
            _addAnswerRecord(msg.content);
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
        qDebug() << "Model: " << model.name << ", hash: " << model.hash
                 << ", addr: " << model.addr;
    }

    // refresh model combo box
    ui->comboModel->clear();
    for(const auto &model : m_modelInfos)
    {
        ui->comboModel->addItem(model.name);
    }
}

void ChatBox::_slotBtnStartClicked()
{
    int64_t sessionId = -1;
    QString model     = ui->comboModel->currentText();
    QString query     = ui->editInput->toPlainText();
    auto    item      = ui->listChat->currentItem();
    if(item == nullptr)
    {
        qDebug() << "No session selected, create new session with query:"
                 << query;
        QString title = query;
        emit    m_pBus->SignalNewSession(title, query, model);
        return;
    }

    sessionId = item->data(Qt::UserRole).toLongLong();
    _addQueryRecord(query);
    ui->btnStart->pressed();
    ui->btnStart->setIcon(QIcon(":/icons/pause_check"));
    ui->editInput->clear();
    emit m_pBus->SignalQuery(sessionId, query, model);
}

void ChatBox::_slotCurrentRowChanged(int row)
{
    if(row < 0 || row > ui->listChat->count() || !ui->listChat->currentItem())
        return;

    auto sessionId =
        ui->listChat->currentItem()->data(Qt::UserRole).toLongLong();
    emit m_pBus->SignalGetMessageInfo(-1, sessionId, 1000);
}

void ChatBox::_addQueryRecord(const QString &query)
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

void ChatBox::_addAnswerRecord(const QString &answer)
{
    qDebug() << "Add answer record: " << answer;
    QString tm   = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString html = QString(R"(
        <table width="100%" border="0" cellspacing="0" cellpadding="0" style="margin-bottom: 10px;">
            <tr>
                <td align="left">
                    <span style="color: #888888; font-size: 11px; margin-left: 5px;">%1</span>
                    <br>
                    <table bgcolor="#f1f1f1" style="border-radius: 10px; margin-top: 3px;" cellpadding="6">
                        <tr>
                            <td style="color: #000000; font-size: 14px; text-align: left;">%2</td>
                        </tr>
                    </table>
                </td>
            </tr>
        </table>
    )")
                       .arg(tm, answer);

    ui->txtBrowserChat->append(html);

    QTextCursor cursor = ui->txtBrowserChat->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->txtBrowserChat->setTextCursor(cursor);

    ui->btnStart->released();
    ui->btnStart->setIcon(QIcon(":/icons/send"));
}
