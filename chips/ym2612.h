#ifndef YM2612_H
#define YM2612_H

#include <QObject>
#include <memorybus.h>

class YM2612Private;
class YM2612
        : public QObject,
        public IMemory
{
public:
    YM2612(QObject* parent = 0);
    ~YM2612();

    // Emulation
    int     peek(quint32 address, quint8& val);
    int     poke(quint32 address, quint8 val);

    void    clock(int cycles);

public slots:
    void    reportSampleFrequency();

private:
    YM2612Private* d_ptr;
    Q_DECLARE_PRIVATE(YM2612)
};

#endif // YM2612_H
