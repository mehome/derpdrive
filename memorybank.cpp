#include "memorybank.h"

#include <QDebug>

class MemoryBankPrivate {
public:
    IMemory*                mem;
    MemoryBankController*   controller;
    quint32                 baseAddress;
    quint32                 nextAddress;
    int                     bankBitCount;

public:
    MemoryBankPrivate(MemoryBank* q)
        : mem(nullptr),
          controller(new MemoryBankController(q)),
          baseAddress(0),
          nextAddress(0),
          bankBitCount(0),
          q_ptr(q)
    {
    }

    ~MemoryBankPrivate() {
        delete this->controller;
    }

private:
    MemoryBank* q_ptr;
    Q_DECLARE_PUBLIC(MemoryBank)
};

MemoryBank::MemoryBank(QObject *parent)
    : QObject(parent),
      d_ptr(new MemoryBankPrivate(this))
{

}

void MemoryBank::attachMemory(IMemory *mem)
{
    Q_D(MemoryBank);

    d->mem = mem;
}

int MemoryBank::peek(quint32 address, quint8 &val)
{
    Q_D(MemoryBank);

    return d->mem->peek(d->baseAddress + address, val);
}

int MemoryBank::poke(quint32 address, quint8 val)
{
    Q_D(MemoryBank);

    return d->mem->poke(d->baseAddress + address, val);
}

void MemoryBank::pushBankBit(quint8 bit)
{
    Q_D(MemoryBank);

    d->nextAddress |= ((bit & 0x1) << d->bankBitCount) << 15;
    d->bankBitCount++;

    if (d->bankBitCount == 9) {
        d->baseAddress = d->nextAddress;
        d->nextAddress = 0;
        d->bankBitCount = 0;

        //qDebug() << "Bank switch to" << QString::number(d->baseAddress, 16).rightJustified(6, '0');
    }
}

MemoryBankController *MemoryBank::controller()
{
    Q_D(MemoryBank);

    return d->controller;
}

MemoryBankController::MemoryBankController(MemoryBank *bank)
    : bank(bank)
{
}

MemoryBankController::~MemoryBankController()
{
}

int MemoryBankController::peek(quint32 address, quint8 &val)
{
    Q_UNUSED(address);

    val = 0xFF;
    return IMemory::NO_ERROR;
}

int MemoryBankController::poke(quint32 address, quint8 val)
{
    Q_UNUSED(address);

    //qDebug() << "BNK" << QString::number(val, 16).rightJustified(2, '0');

    this->bank->pushBankBit(val);
    return IMemory::NO_ERROR;
}
