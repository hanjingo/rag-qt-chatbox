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
    void _slotModelInfoUpdate(const QVector<Bus::ModelInfo> &modelInfos);
    void _slotAudioParamUpdateNtf(const QVector<Bus::AudioParam> &params);
    void _slotAudioCaptureStarted(const qint64 id, const QByteArray devId);
    void _slotAudioCaptured(const qint64 id, const QByteArray &data);
    void _slotAudioCaptureStopped(const qint64 id);
    void _slotRecognizeResp(const int      errorCode,
                            const QString &transcript,
                            const bool     isFinished,
                            const double   confidence);
    void _slotStopRecognizeResp(const int errorCode, const qint64 streamId);
    void _slotUploadResp(const int errorCode, const QString &filePath);

    // ui signal
    void _slotBtnStartClicked();
    void _slotBtnAttachClicked();
    void _slotBtnAudioStartClicked();
    void _slotCurrentRowChanged(int row);
    void _slotPipelineBtnGroupClicked(int id);

    // flush buffer
    void _slotFlushAudioBuffer();

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

    void _appendAudioData(const QByteArray &data);
    void _flushAudioBuffer(bool force = false);
    void _clearAudioBuffer();
    bool _isAudioEnough();
    bool _isAudioOverflow();
    int  _minBufferSize();
    int  _maxBufferSize();

  private:
    Ui::ChatBox  *ui;
    QButtonGroup *m_pPipelineBtnGroup;

    mutable QMutex m_mu;
    Bus           *m_pBus;
    QTimer        *m_pTimer;

    bool m_isAnswerFinished  = true;
    bool m_isLastMsgFinished = true;

    // audio
    QTimer    *m_pAudioFlushTimer;
    qint64     m_audioId        = -1;
    bool       m_isAudioStarted = false;
    QByteArray m_audioBuffer;
    QMutex     m_audioBufferMutex;
    int        m_flushIntervalMs = 300; // flush every 300ms

    QString m_streamingAnswer;
    QString m_streamTimestamp;
    int     m_streamStartPos = -1;

    QVector<Bus::AudioParam>                  m_audioParams;
    QVector<Bus::ModelInfo>                   m_modelInfos;
    QVector<Bus::MessageInfo>                 m_buf;
    QHash<int64_t, QVector<Bus::MessageInfo>> m_messageInfos;
};

#endif