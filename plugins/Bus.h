#ifndef BUS_H
#define BUS_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QAudioFormat>
#include <QJsonObject>
#include <QPointer>

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

    struct ModelInfo
    {
        // base info
        QString id;
        QString name;
        QString publisher;
        QString timestamp;
        QString addr;
        QString pipeline;
        qint32  cost;
        QString hash;

        // context parameters
        int     ctxWindowSize;
        QString stopWords;

        // prompt
        QString prompt;
    };

    struct MemoryInfo
    {
        QString id;
    };

    struct Plugin
    {
        QString hash;
        QString name;
        QString desc;
        QString publisher;
        QString version;
        QString timestamp;
        qint32  platform;

        // external file path, used for staging plugin info before loading
        QString filePath;
    };

    struct AudioParam
    {
        QString translatorId;
        int     minNewSampleSize;
        int     minAudioBufferSize;
        int     maxAudioBufferSize;
    };

  public:
    static QPointer<Bus> instance();
    static void          version(int8_t &major, int8_t &minor, int8_t &patch);

  signals:
    void signalPong();
    void signalPing();

    void signalLanguageSwitch(const QString &lang);

    void signalModelInfoUpdateNtf(const QVector<Bus::ModelInfo> &modelInfos);
    void signalMemoryInfoUpdateNtf(const QVector<Bus::MemoryInfo> &memoryInfos);

    void signalAudioParamUpdateNtf(const QVector<Bus::AudioParam> &params);

    void signalNewSession(const QString &title,
                          const QString &content,
                          const QString &model);
    void signalNewSessionResp(const int32_t       errorCode,
                              const Bus::Session &session);

    void signalGetSession(const int64_t sessionId, int limit);
    void signalGetSessionResp(const int                    errorCode,
                              const QVector<Bus::Session> &sessions);

    void signalDelSessionResp(const int errorCode, const QVector<int64_t> &ids);

    void signalQuery(const int64_t         sessionId,
                     const int64_t         msgId,
                     const QString        &query,
                     const QString        &model,
                     const Bus::ModelInfo &infos);
    void signalQueryResp(const int      errorCode,
                         const int64_t  sessionId,
                         const int64_t  msgId,
                         const QString &content,
                         const bool     isFinished);

    void signalStopAnswer(const int64_t sessionId);
    void signalStopAnswerResp(const int64_t errorCode, const int64_t sessionId);

    void signalGetChatMessage(const int64_t msgId,
                              const int64_t sessionId,
                              int           limit);
    void signalGetChatMessageResp(const int                        errorCode,
                                  const QVector<Bus::ChatMessage> &messages);

    void signalAudioCaptureStart(const QAudioFormat &format,
                                 const QByteArray   &devId);
    void signalAudioCaptureStarted(const qint64 id, const QByteArray &devId);

    void signalAudioCaptured(const qint64 id, const QByteArray &data);

    void signalAudioCaptureStop(const qint64 id);
    void signalAudioCaptureStopped(const qint64 id);

    void signalRecognize(const qint64      sessionId,
                         const QByteArray &src,
                         const QString    &translatorId);
    void signalRecognizeResp(const int      errorCode,
                             const QString &transcript,
                             const bool     isFinished,
                             const double   confidence);

    void signalStopRecognize(const qint64 sessionId);
    void signalStopRecognizeResp(const int errorCode, const qint64 sessionId);

    void signalUpload(const QString &filePath);
    void signalUploadResp(const int errorCode, const QString &filePath);

    void signalRetrieve(const QString &question,
                        const int      topK,
                        const QString &memoryId);
    void signalRetrieveResp(const int                   errorCode,
                            const QString              &question,
                            const int                   topK,
                            const QString              &memoryId,
                            const QVector<QJsonObject> &memorys);

  private:
    explicit Bus(QObject *parent = nullptr)
        : QObject(parent) {};
    ~Bus() {};
};

#endif