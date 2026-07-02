#ifndef CHATBOX_H
#define CHATBOX_H

#include <QObject>
#include <QtPlugin>
#include <QWidget>
#include <QVector>
#include <QButtonGroup>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QBuffer>
#include <QByteArray>

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
    void _slotAudioCaptureStarted(const qint64 id, const QByteArray devId);
    void _slotAudioCaptured(const qint64 id, const QByteArray &data);
    void _slotAudioCaptureStopped(const qint64 id);
    void _slotAudioTranslated(const int               errorCode,
                              const QByteArray       &src,
                              const QVector<QString> &segments);

    // ui signal
    void _slotBtnStartClicked();
    void _slotBtnAudioStartClicked();
    void _slotCurrentRowChanged(int row);
    void _slotPipelineBtnGroupClicked(int id);

  private:
    void _refreshUI();
    void _refreshModelItem();
    void _refreshChatBrowser(const QVector<Bus::MessageInfo> &msgs);
    void _drawQueryRecord(const QString &query);
    void _drawAnswerRecord(const QString &answer, const bool isFinished = true);
    void _clearChatBrowser();

    void     _retranslate();
    QWidget *_initUI();
    void     _initConnectsions();

    void                      _writeBuf(const Bus::MessageInfo &msg);
    QVector<Bus::MessageInfo> _readBufAll();
    void                      _setAnswerFinishState(bool isFinish);
    void                      _query();
    void                      _stopQuery();
    void                      _startAudioRecord();
    void                      _stopAudioRecord();
    void             _setAudioRecordState(bool isStarted, qint64 id = -1);
    void             _addMsgRecord(const Bus::MessageInfo &msg);
    Bus::MessageInfo _convert(const int64_t  msg_id,
                              const int64_t  sessionId,
                              const QString &role,
                              const QString &content,
                              const QString &timestamp,
                              const bool     isFinished);

  private:
    Ui::ChatBox  *ui;
    QButtonGroup *m_pPipelineBtnGroup;

    mutable QMutex m_mu;
    Bus           *m_pBus;
    QTimer        *m_pTimer;

    bool m_isAnswerFinished  = true;
    bool m_isLastMsgFinished = true;

    qint64  m_audioId        = -1;
    bool    m_isAudioStarted = false;
    QBuffer m_audioBuffer;

    QString m_streamingAnswer;
    QString m_streamTimestamp;
    int     m_streamStartPos = -1;

    QVector<Bus::ModelConfig>                 m_modelInfos;
    QVector<Bus::MessageInfo>                 m_buf;
    QHash<int64_t, QVector<Bus::MessageInfo>> m_messageInfos;
};

#endif