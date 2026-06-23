#ifndef CHATBOX_H
#define CHATBOX_H

#include <QObject>
#include <QtPlugin>
#include <QWidget>
#include <QVector>

#include "Bus.h"
#include "PluginInterface.h"

namespace Ui
{
class ChatBox;
}

class ChatBox : public QObject, public PluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "rag-qt.PluginInterface" FILE "chatboxplugin.json")
    Q_INTERFACES(PluginInterface)

  public:
    explicit ChatBox(QWidget *parent = nullptr);
    ~ChatBox();

    QString  Id() override { return "chatbox-v0.0.1"; }
    QString  Name() override { return "chatbox"; }
    QString  Icon() override { return "ChatBoxIcon.png"; }
    QString  Version() override { return "0.0.1"; }
    QWidget *Init(Bus *parent = nullptr) override;

  private slots:
    // bus signal
    void _slotPing();
    void _slotLanguageSwitch(const QString &lang);
    void _slotNewSessionResp(const int32_t       errorCode,
                             const Bus::Session &session);
    void _slotGetSessionResp(const int                    errorCode,
                             const QVector<Bus::Session> &sessions);
    void _slotQueryResp(const int32_t  errorCode,
                        const int64_t  sessionId,
                        const QString &resp);
    void _slotGetMessageInfoResp(const int                        errorCode,
                                 const QVector<Bus::MessageInfo> &messages);
    void _slotModelInfoUpdate(const QVector<Bus::Model> &modelInfos);

    // ui signal
    void _slotBtnStartClicked();
    void _slotCurrentRowChanged(int row);

  private:
    void _addQueryRecord(const QString &query);
    void _addAnswerRecord(const QString &answer);

  private:
    Ui::ChatBox *ui;
    Bus         *m_pBus;

    QVector<Bus::Model> m_modelInfos;
};

#endif