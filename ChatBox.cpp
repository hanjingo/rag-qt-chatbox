#include "ChatBox.h"
#include "ui_ChatBox.h"

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

    // init connect
    connect(m_pBus, &Bus::SignalPing, this, &ChatBox::_slotPing);

    // create UI
    auto wgt = new QWidget(nullptr);
    wgt->setStyleSheet("background-color: transparent;");
    ui->setupUi(wgt);
    return wgt;
}

void ChatBox::_slotPing()
{
    qDebug() << "ChatBox received Ping signal from Bus.";
    emit m_pBus->SignalPong();
}