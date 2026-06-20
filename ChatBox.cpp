#include "ChatBox.h"
#include "ui_ChatBox.h"

#include <QString>

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

    // init BUS connect
    connect(m_pBus, &Bus::SignalPing, this, &ChatBox::_slotPing);
    connect(m_pBus,
            &Bus::SignalLanguageSwitch,
            this,
            &ChatBox::_slotLanguageSwitch);
    connect(m_pBus, &Bus::SignalQueryResp, this, &ChatBox::_slotQueryResp);

    // init UI connect
    connect(ui->btnStart,
            &QPushButton::clicked,
            this,
            &ChatBox::_slotBtnStartClicked);

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

void ChatBox::_slotQueryResp(const int64_t sessionId, const QString &resp)
{
    qDebug() << "ChatBox received QueryResp signal from Bus. sessionId: "
             << sessionId << ", resp: " << resp;

    _addAnserRecord(resp);
    ui->btnStart->released();
    ui->btnStart->setIcon(QIcon(":/icons/send"));
    ui->listChat->currentItem()->setData(
        Qt::UserRole,
        QVariant::fromValue(static_cast<qlonglong>(sessionId)));
}

void ChatBox::_slotBtnStartClicked()
{
    int64_t sessionId = -1;
    QString query     = ui->editInput->toPlainText();
    auto    item      = ui->listChat->currentItem();
    if(item == nullptr)
    {
        QString title = query;
        item          = new QListWidgetItem(title);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->listChat->addItem(item);
        ui->listChat->setCurrentItem(item);
    } else
    {
        sessionId = item->data(Qt::UserRole).toLongLong();
    }

    _addQueryRecord(query);
    ui->btnStart->pressed();
    ui->btnStart->setIcon(QIcon(":/icons/pause_check"));
    ui->editInput->clear();
    emit m_pBus->SignalQuery(-1, query);
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

void ChatBox::_addAnserRecord(const QString &answer)
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
}
