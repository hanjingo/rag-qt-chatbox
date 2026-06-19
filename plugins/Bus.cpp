#include "Bus.h"

Bus *Bus::Instance()
{
    static Bus instance;
    return &instance;
}

void Bus::Version(int8_t &major, int8_t &minor, int8_t &patch)
{
    major = 0;
    minor = 0;
    patch = 1;
}