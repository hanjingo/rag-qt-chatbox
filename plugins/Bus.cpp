#include "Bus.h"

QPointer<Bus> Bus::instance()
{
    static QPointer<Bus> instance = new Bus();
    return instance;
}

void Bus::version(int8_t &major, int8_t &minor, int8_t &patch)
{
    major = 0;
    minor = 0;
    patch = 1;
}