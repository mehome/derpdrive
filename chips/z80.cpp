#include "z80.h"
#include "z80/z80emu.h"

#include <QDebug>

class Z80Private {
   public:
      Z80_STATE   state;
      MemoryBus*  bus;
      bool        busReq;
      bool        resetting;

   public:
      Z80Private(Z80* q)
         : q_ptr(q),
           bus(0),
           busReq(0),
           resetting(0)
      {
         memset(&this->state, 0, sizeof(Z80_STATE));
         Z80Reset(&this->state);
      }

   private:
      Z80* q_ptr;
      Q_DECLARE_PUBLIC(Z80)
};

Z80::Z80(QObject* parent)
   : QObject(parent),
     d_ptr(new Z80Private(this))
{

}

Z80::~Z80()
{
   delete d_ptr;
}

void Z80::attachBus(MemoryBus* bus)
{
   Q_D(Z80);

   d->bus = bus;
}

MemoryBus* Z80::bus()
{
   Q_D(Z80);

   return d->bus;
}

int Z80::clock(int cycles)
{
   Q_D(Z80);

   if (!d->busReq && !d->resetting)
      Z80Emulate(&d->state, cycles, this);

   return 0;
}

void Z80::reset()
{
   Q_D(Z80);

   Z80Reset(&d->state);
}

void Z80::interrupt()
{
   Q_D(Z80);

   Z80Interrupt(&d->state, 0, this);
}

int Z80::peek(quint32 address, quint8& val)
{
   Q_D(Z80);

   //qDebug() << "Z80 READ" << address;

   switch (address) {
      case 0x00:
         val = d->busReq ? 0x0 : 0x1;
         break;

      default:
         val = 0;
         break;
   }

   return NO_ERROR;
}

int Z80::poke(quint32 address, quint8 val)
{
   Q_D(Z80);

   //qDebug() << "Z80 WRITE" << address << "=" << val;

   switch (address) {
      case 0x00:
         d->busReq = val != 0;
         break;

      case 0x01:
         break;

      case 0x02:
         d->resetting = !val;
         if (d->resetting)
            this->reset();
         break;

      case 0x03:
         break;

      default:
         return BUS_ERROR;
   }

   return NO_ERROR;
}
