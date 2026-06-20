#ifndef BUS_H
#define BUS_H

#include <QObject>

class Bus : public QObject
{
    Q_OBJECT
  public:
    static Bus *Instance();
    static void Version(int8_t &major, int8_t &minor, int8_t &patch);

  signals:
    // signals that plugins can emit
    void SignalPong();
    void SignalModelSwitch(const QString &model);
    void SignalQuery(const int64_t sessionId, const QString &query);

  signals:
    // signals that framework can emit
    void SignalPing();
    void SignalLanguageSwitch(const QString &lang);
    void SignalQueryResp(const int64_t sessionId, const QString &resp);

  private:
    explicit Bus(QObject *parent = nullptr)
        : QObject(parent) {};
    ~Bus() {};
};

#endif