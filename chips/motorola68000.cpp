#include "motorola68000.h"
#include "chips/motorola68000private.h"

#include <QDebug>
#include <QVector>
#include <QtEndian>

Motorola68000::Motorola68000(QObject *parent)
   : QObject(parent),
     d_ptr(new Motorola68000Private(this))
{

}

void Motorola68000::attachBus(MemoryBus* bus)
{
   Q_D(Motorola68000);

   d->bus = bus;
}

int Motorola68000::clock(int ticks)
{

}

void Motorola68000::reset()
{
   Q_D(Motorola68000);

   quint32 pc, ssp;

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

   qDebug() << "Programm counter: " << d->registerData[Motorola68000Private::PC_INDEX];
}
