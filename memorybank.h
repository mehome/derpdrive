#ifndef MEMORYBANK_H
#define MEMORYBANK_H

#include <QObject>

#include "memorybus.h"

class MemoryBankController;
class MemoryBankPrivate;

class MemoryBank
        : public QObject,
          public IMemory
{
    Q_OBJECT
public:
    explicit MemoryBank(QObject *parent = 0);

    void    attachMemory(IMemory* mem);

    int     peek(quint32 address, quint8& val);
    int     poke(quint32 address, quint8 val);

    void    pushBankBit(quint8 bit);

    MemoryBankController* controller();

signals:

public slots:

private:
    MemoryBankPrivate* d_ptr;
    Q_DECLARE_PRIVATE(MemoryBank)
};

class MemoryBankController
        : public IMemory
{
    friend MemoryBankPrivate;

protected:
    MemoryBankController(MemoryBank* bank);
    ~MemoryBankController();

public:
    int peek(quint32 address, quint8& val);
    int poke(quint32 address, quint8 val);

private:
    MemoryBank* bank;
};

#endif // MEMORYBANK_H
