#include "motorola68000.h"
#include "chips/motorola68000private.h"
#include "device.h"

#include <QDebug>
#include <QVector>
#include <QtEndian>

#include <signal.h>

Motorola68000::Motorola68000(QObject *parent)
    : QObject(parent),
      d_ptr(new Motorola68000Private(this))
{

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

    if(!d->debug || (d->debug && d->debugRun))
        d->currentTicks -= ticks;

    QString  traceMessage;

    while(d->currentTicks < 0) {
        if (d->disabled) {
            d->currentTicks = 0;
            return 0;
        }

        unsigned int opcode = 0;
        unsigned int pc = d->registerData[Motorola68000Private::PC_INDEX];
        int      status = Motorola68000Private::EXECUTE_OK;

        if (d->debug && d->debugRun) {
            if (d->breakpoins.contains(pc)) {
                d->currentTicks = 0;
                d->debugRun = false;

                emit this->breakpointReached(pc);
                return 0;
            }
        }

        if (d->tracing) {
            traceMessage = "0x";
            traceMessage += QString::number(pc, 16).rightJustified(8, '0');
            traceMessage += " ";
        }

        if (d->state != Motorola68000Private::STATE_HALT) {
            // Handle interrupts
            bool interruptFlag = false;
            status = d->handleInterrupts(interruptFlag);

            if (!interruptFlag && status == Motorola68000Private::EXECUTE_OK) {

                // Make sure we arent waiting for interrupts
                if (d->state != Motorola68000Private::STATE_STOP) {
                    // Fetch instructions
                    status = d->peek(d->registerData[Motorola68000Private::PC_INDEX],
                            opcode,
                            Motorola68000Private::WORD);

                    if (d->tracing) {
                        traceMessage += QString::number(opcode, 16).rightJustified(8, '0');
                        traceMessage += " ";
                    }

                    if (status == Motorola68000Private::EXECUTE_OK) {
                        d->registerData[Motorola68000Private::PC_INDEX] += 2;

                        // Execute instructoin
                        ExecutionPointer method = d->decodeInstruction(opcode);
                        status = (d->*method)(opcode, traceMessage, d->tracing);

                        // If the last instruction was not privileged then check for trace
                        if ((status == Motorola68000Private::EXECUTE_OK) &&
                                (d->registerData[Motorola68000Private::SR_INDEX] & Motorola68000Private::T_FLAG)) {
                            d->processException(9);
                        }
                    }
                } else if(d->tracing) {

                    traceMessage += "CPU is stopped";
                }
            }

            if (status == Motorola68000Private::EXECUTE_BUS_ERROR) {
                //if (d->debug) {
                    if (!d->tracing) {
                        traceMessage = "0x";
                        traceMessage += QString::number(pc, 16).rightJustified(8, '0');
                        traceMessage += " ";
                        traceMessage += QString::number(opcode, 16).rightJustified(8, '0');
                        traceMessage += " ";
                    }

                    traceMessage += "Bus/Address Error!";

                    this->debugToggle(true);
                    emit this->debugError(pc, opcode, traceMessage);
                //}

                    if(d->tracing)
                        *d->instructionLog << traceMessage << "\n";

                if (d->ExecuteBusError(opcode, traceMessage, d->tracing) != Motorola68000Private::EXECUTE_OK) {
                    // Exception in exception handler, well... shit
                    d->state = Motorola68000Private::STATE_HALT;

                    traceMessage += "Double Bus/Address Error! CPU halted";
                }
            } else if (status == Motorola68000Private::EXECUTE_ADDRESS_ERROR) {
                //if (d->debug) {
                    if (!d->tracing) {
                        traceMessage = "0x";
                        traceMessage += QString::number(pc, 16).rightJustified(8, '0');
                        traceMessage += " ";
                        traceMessage += QString::number(opcode, 16).rightJustified(8, '0');
                        traceMessage += " ";
                    }

                    traceMessage += "Bus/Address Error!";

                    this->debugToggle(true);
                    emit this->debugError(pc, opcode, traceMessage);

                    if(d->tracing)
                        *d->instructionLog << traceMessage << "\n";
                //}


                if (d->ExecuteAddressError(opcode, traceMessage, d->tracing) != Motorola68000Private::EXECUTE_OK) {
                    // Exception in exception handler, well... shit
                    d->state = Motorola68000Private::STATE_HALT;
                    traceMessage += "Double Bus/Address Error! CPU halted";
                }
            } else if (status == Motorola68000Private::EXECUTE_ILLEGAL_INSTRUCTION) {
                traceMessage = QString("0x%1 %2 %3").arg(QString::number(pc, 16).rightJustified(8, '0'), QString::number(opcode, 16).rightJustified(8, '0'), traceMessage);

                this->debugToggle(true);
                emit this->debugError(pc, opcode, traceMessage);
            }
        } else {
            if(d->tracing)
                traceMessage += "CPU is halted";

            d->currentTicks = 0;
        }

        if (d->debug) {
            if (!d->debugRun) {
                emit this->singleStep(pc, opcode, traceMessage);
                d->currentTicks = 0;
            }
        } else if (d->tracing) {
            QStringList registerDbg;

            for(int i=0; i < 18; i++) {
                registerDbg << QString("%1: %2").arg(d->registerInfo[i].name, QString::number(d->registerData[i], 16).rightJustified(8, '0'));
            }

            *d->instructionLog << traceMessage << " (" << registerDbg.join(", ") << ")\n";
        }
    }

    return 0;
}

void Motorola68000::reset()
{
    Q_D(Motorola68000);

    quint32 pc, ssp;

    /*
     * Reset Register
     */
    d->registerData.fill(0);

    /*
    * Reset Flags
    */
    d->registerData[Motorola68000Private::SR_INDEX] =
            Motorola68000Private::S_FLAG |
            Motorola68000Private::I0_FLAG |
            Motorola68000Private::I1_FLAG |
            Motorola68000Private::I2_FLAG;

    /*
    * Fetch Supervisor Stack Pointer
    */
    if (d->peek(0x000000, ssp, Motorola68000Private::LONG) != Motorola68000Private::EXECUTE_OK)
        d->setRegister(Motorola68000Private::SSP_INDEX, 0, Motorola68000Private::LONG);
    else
        d->setRegister(Motorola68000Private::SSP_INDEX, ssp, Motorola68000Private::LONG);

    /*
    * Fetch Program Counter
    */
    if (d->peek(0x000004, pc, Motorola68000Private::LONG) != Motorola68000Private::EXECUTE_OK)
        d->setRegister(Motorola68000Private::PC_INDEX, 0, Motorola68000Private::LONG);
    else
        d->setRegister(Motorola68000Private::PC_INDEX, pc, Motorola68000Private::LONG);

    d->state = Motorola68000Private::STATE_NORMAL;
    d->currentTicks = 0;
    d->pendingInterrupts.empty();

    qDebug() << "SSP" << d->registerData[Motorola68000Private::SSP_INDEX];
    qDebug() << "Programm counter: " << d->registerData[Motorola68000Private::PC_INDEX];
}

void Motorola68000::interruptRequest(Device* device, int level)
{
    Q_D(Motorola68000);

    // We have seven levels of interrupts
    if (level > 7)
        level = 7;
    else if (level < 1)
        level = 1;

    d->pendingInterrupts.push(Motorola68000Private::PendingInterrupt(level, device));
}

void Motorola68000::clearInterrupt(Device* device, int level)
{
    Q_D(Motorola68000);


    while(!d->pendingInterrupts.empty())
        d->pendingInterrupts.pop();
}

quint32 Motorola68000::programCounter() const
{
    Q_D(const Motorola68000);

    return d->registerData[Motorola68000Private::PC_INDEX];
}

QString Motorola68000::registerName(int reg) const
{
    Q_D(const Motorola68000);

    return d->registerInfo[reg].name;
}

unsigned int Motorola68000::registerValue(int reg) const
{
    Q_D(const Motorola68000);

    return d->registerData[reg];
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
    d->currentTicks = 0;
}

void Motorola68000::debugStep()
{
    Q_D(Motorola68000);

    if(d->debug && !d->debugRun)
        d->currentTicks = -1;
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
    d->currentTicks = -1;
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
