#ifndef BUS_H
#define BUS_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QAudioFormat>

#define BUS_VERSION_MAJOR 0
#define BUS_VERSION_MINOR 0
#define BUS_VERSION_PATCH 1

class Bus : public QObject
{
    Q_OBJECT
  public:
    struct Session
    {
        qint64  id;
        qint64  userId;
        QString title;
        QString timestamp;
    };

    struct MessageInfo
    {
        qint64  id;
        qint64  sessionId;
        QString role;
        QString content;
        qint64  prevMessageId;
        QString timestamp;
        bool    isFinished;
    };

    struct ModelConfig
    {
        // base info
        QString id;
        QString name;
        QString publisher;
        QString timestamp;
        QString addr;
        QString pipeline;
        qint32  cost;
        QString apiKey;

        // sampling parameters
        float temperature;
        float topP;
        float topK;
        float reputationPenalty;
        float minP;

        // context parameters
        int     ctxWindowSize;
        QString stopWords;

        // prompt
        QString prompt;
    };

    struct Skill
    {
        QString hash;
        QString name;
        QString desc;
        QString publisher;
        QString version;
        QString timestamp;
        qint32  platform;
    };

  public:
    static Bus *Instance();
    static void Version(int8_t &major, int8_t &minor, int8_t &patch);

  signals:
    void SignalPong();
    void SignalPing();

    void SignalLanguageSwitch(const QString &lang);

    void SignalModelInfoUpdateNtf(const QVector<Bus::ModelConfig> &modelInfos);

    void SignalNewSession(const QString &title,
                          const QString &content,
                          const QString &model);
    void SignalNewSessionResp(const int32_t       errorCode,
                              const Bus::Session &session);

    void SignalGetSession(const int64_t sessionId, int limit);
    void SignalGetSessionResp(const int                    errorCode,
                              const QVector<Bus::Session> &sessions);

    void SignalDelSessionResp(const int errorCode, const QVector<int64_t> &ids);

    void SignalQuery(const int64_t           sessionId,
                     const QString          &query,
                     const QString          &model,
                     const Bus::ModelConfig &config);
    void SignalQueryResp(const int      errorCode,
                         const int64_t  sessionId,
                         const QString &content,
                         const bool     isFinished);

    void SignalStopAnswer(const int64_t sessionId);
    void SignalStopAnswerResp(const int64_t errorCode, const int64_t sessionId);

    void SignalGetMessageInfo(const int64_t msgId,
                              const int64_t sessionId,
                              int           limit);
    void SignalGetMessageInfoResp(const int                        errorCode,
                                  const QVector<Bus::MessageInfo> &messages);

    void SignalAudioCaptureStart(const QAudioFormat &format,
                                 const QByteArray   &devId);
    void SignalAudioCaptureStarted(const qint64 id, const QByteArray &devId);

    void SignalAudioCaptured(const qint64 id, const QByteArray &data);

    void SignalAudioCaptureStop(const qint64 id);
    void SignalAudioCaptureStopped(const qint64 id);

  private:
    explicit Bus(QObject *parent = nullptr)
        : QObject(parent) {};
    ~Bus() {};
};

#endif