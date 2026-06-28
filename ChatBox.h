#ifndef CHATBOX_H
#define CHATBOX_H

#include <QObject>
#include <QtPlugin>
#include <QWidget>
#include <QVector>
#include <QButtonGroup>
#include <QHash>

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
    void _slotDelSessionResp(const int errorCode, const QVector<int64_t> &ids);
    void _slotQueryResp(const int32_t  errorCode,
                        const int64_t  sessionId,
                        const QString &resp,
                        const bool     isFinished);
    void _slotStopAnswerResp(const int64_t errorCode, const int64_t sessionId);
    void _slotGetMessageInfoResp(const int                        errorCode,
                                 const QVector<Bus::MessageInfo> &messages);
    void _slotModelInfoUpdate(const QVector<Bus::ModelConfig> &modelInfos);

    // ui signal
    void _slotBtnStartClicked();
    void _slotCurrentRowChanged(int row);
    void _slotPipelineBtnGroupClicked(int id);

  private:
    void _drawQueryRecord(const QString &query);
    void _drawAnswerRecord(const QString &answer, const bool isFinished = true);
    void _refreshModelItem();
    void _refreshAnswerFinishState(bool isFinished);
    void _refreshChatBrowser();
    void _query();
    void _stopQuery();
    void _addMsgRecord(const int64_t  sessionId,
                       const QString &role,
                       const QString &content,
                       const QString &timestamp,
                       const bool     isFinished);

  private:
    Ui::ChatBox  *ui;
    QButtonGroup *m_pPipelineBtnGroup;

    Bus *m_pBus;

    bool m_isAnswerFinished = true;

    bool m_isLastMsgFinished = true;

    QString m_streamingAnswer;
    QString m_streamTimestamp;
    int     m_streamStartPos = -1;

    QVector<Bus::ModelConfig>                 m_modelInfos;
    QHash<int64_t, QVector<Bus::MessageInfo>> m_messageInfos;
};

#endif