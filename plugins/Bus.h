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
    void SignalPing();
    void SignalPong();

  private:
    explicit Bus(QObject *parent = nullptr)
        : QObject(parent) {};
    ~Bus() {};
};

#endif