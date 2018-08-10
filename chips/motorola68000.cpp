#include "motorola68000.h"
#include "chips/motorola68000private.h"
#include "chips/m68k/m68k.h"
#include "device.h"

#include <QDebug>
#include <QVector>
#include <QtEndian>

#include <signal.h>

Motorola68000::Motorola68000(QObject *parent)
    : QObject(parent),
      d_ptr(new Motorola68000Private(this))
{
    Q_D(Motorola68000);
}

Motorola68000::~Motorola68000()
{
    delete d_ptr;
}

void Motorola68000::attachBus(MemoryBus* bus)
{
    Q_D(Motorola68000);

    d->bus = bus;
}

void Motorola68000::setTracing(bool tracing)
{
    Q_D(Motorola68000);

    d->tracing = tracing;
}

bool Motorola68000::tracing() const
{
    Q_D(const Motorola68000);

    return d->tracing;
}

void Motorola68000::setDisabled(bool disabled)
{
    Q_D(Motorola68000);

    d->disabled = disabled;
}

int Motorola68000::clock(int ticks)
{
    Q_D(Motorola68000);

    d->switchContext();
    m68k_execute(ticks);

    return 0;
}

void Motorola68000::reset()
{
    Q_D(Motorola68000);

    d->switchContext();
    m68k_pulse_reset();
}

void Motorola68000::interruptRequest(Device* device, int level)
{
    Q_D(Motorola68000);

    d->switchContext();
    m68k_set_irq(level);
    //d->pendingInterrupts.push(Motorola68000Private::PendingInterrupt(level, device));
}

void Motorola68000::clearInterrupt(Device* device, int level)
{
    Q_D(Motorola68000);

    d->switchContext();
    m68k_set_irq(0);
    //while(!d->pendingInterrupts.empty())
    //    d->pendingInterrupts.pop();
}

quint32 Motorola68000::programCounter() const
{
    Q_D(const Motorola68000);

    return m68k_get_reg(d->context, M68K_REG_PC);
    //return d->registerData[Motorola68000Private::PC_INDEX];
}

QString Motorola68000::registerName(int reg) const
{
    switch(reg) {
    case M68K_REG_D0: return "D0";
    case M68K_REG_D1: return "D1";
    case M68K_REG_D2: return "D2";
    case M68K_REG_D3: return "D3";
    case M68K_REG_D4: return "D4";
    case M68K_REG_D5: return "D5";
    case M68K_REG_D6: return "D6";
    case M68K_REG_D7: return "D7";
    case M68K_REG_A0: return "A0";
    case M68K_REG_A1: return "A1";
    case M68K_REG_A2: return "A2";
    case M68K_REG_A3: return "A3";
    case M68K_REG_A4: return "A4";
    case M68K_REG_A5: return "A5";
    case M68K_REG_A6: return "A6";
    case M68K_REG_A7: return "A7";
    case M68K_REG_PC: return "PC";
    case M68K_REG_SR: return "SR";
    case M68K_REG_USP: return "USP";
    case M68K_REG_ISP: return "ISP";
    case M68K_REG_MSP: return "MSP";
    case M68K_REG_SFC: return "SFC";
    case M68K_REG_DFC: return "DFC";
    case M68K_REG_VBR: return "VBR";
    case M68K_REG_CACR: return "CACR";
    case M68K_REG_CAAR: return "CAAR";
    }
}

unsigned int Motorola68000::registerValue(int reg) const
{
    Q_D(const Motorola68000);

    return m68k_get_reg(d->context, static_cast<m68k_register_t>(reg));
}

QVector<unsigned int> Motorola68000::breakpoints() const
{
    Q_D(const Motorola68000);

    return d->breakpoins.toVector();
}

void Motorola68000::debugToggle(bool debug)
{
    Q_D(Motorola68000);

    d->debug = debug;
    d->debugRun = false;
    d->tracing = debug;
    //d->currentTicks = 0;
}

void Motorola68000::debugStep()
{
    Q_D(Motorola68000);

    //if(d->debug && !d->debugRun)
    //    d->currentTicks = -1;
}

void Motorola68000::debugRun()
{
    Q_D(Motorola68000);

    d->debugRun = true;
}

void Motorola68000::debugPause()
{
    Q_D(Motorola68000);

    d->debugRun = false;
    //d->currentTicks = -1;
}

void Motorola68000::debugAddBreakpoint(unsigned int adress)
{
    Q_D(Motorola68000);

    d->breakpoins.append(adress);
}

void Motorola68000::debugRemoveBreakpoint(unsigned int adress)
{
    Q_D(Motorola68000);

    d->breakpoins.removeOne(adress);
}
