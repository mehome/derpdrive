#include "ym2612.h"

class YM2612Private {
public:
    YM2612Private(YM2612* q)
        : q_ptr(q)
    {
    }

private:
    YM2612* q_ptr;
    Q_DECLARE_PUBLIC(YM2612)
};

YM2612::YM2612(QObject *parent)
    : QObject(parent),
      d_ptr(new YM2612Private(this))
{

}

YM2612::~YM2612()
{
    delete this->d_ptr;
}

int YM2612::peek(quint32 address, quint8 &val)
{
    val = 0;
    return NO_ERROR;
}

int YM2612::poke(quint32 address, quint8 val)
{
    return NO_ERROR;
}
