#include "memorybus.h"

#include <QVector>
#include <QDebug>

#include <config.h>

struct MemoryWiring {
   public:
      qint32   address;
      qint32   handle;

   public:
      MemoryWiring()
         : address(0),
           handle(-1)
      {
      }
};

class MemoryBusPrivate {
   public:
      MemoryWiring         wiring[BUS_SIZE];
      QVector<IMemory*>    devices;

   public:
      MemoryBusPrivate(MemoryBus* q)
         : q_ptr(q)
      {

      }

   private:
      MemoryBus* q_ptr;
      Q_DECLARE_PUBLIC(MemoryBus)

};

MemoryBus::MemoryBus(QObject *parent)
   : QObject(parent),
     d_ptr(new MemoryBusPrivate(this))
{

}

qint32 MemoryBus::attachDevice(IMemory* dev)
{
   Q_D(MemoryBus);

   d->devices.push_back(dev);

   return d->devices.length()-1;
}

void MemoryBus::wire(int start, int end, int base, qint32 device)
{
   Q_D(MemoryBus);

   if (start >= BUS_SIZE || end > BUS_SIZE) {
      qCritical() << "Can't wire address past bus size:" <<  start;
      return;
   }

   // Divide by two to align with word boundary
   for(int i=start; i <= end; i++) {
      d->wiring[i].address = base++;
      d->wiring[i].handle  = device;
   }
}

void MemoryBus::wire(int src, int dst, qint32 device)
{
   Q_D(MemoryBus);

   if (src >= BUS_SIZE) {
      qCritical() << "Can't wire address past bus size:" <<  src;
      return;
   }

   d->wiring[src].address = dst;
   d->wiring[src].handle  = device;
}

int MemoryBus::peek(quint32 address, quint8& val) {
   Q_D(MemoryBus);

   // Check if adress is legal
   if (address < 0 || address >= BUS_SIZE) {
      return BUS_ERROR;
   }

   // Check if adress is hooked up
   if(d->wiring[address].handle >= 0) {
      IMemory* dev = d->devices[d->wiring[address].handle];
      dev->peek(d->wiring[address].address, val);

      return NO_ERROR;
   } else {
      return BUS_ERROR;
   }
}

int MemoryBus::poke(quint32 address, quint8 val) {
   Q_D(MemoryBus);

   // Check if adress is legal
   if (address < 0 || address >= BUS_SIZE) {
      return BUS_ERROR;
   }

   if(d->wiring[address].handle >= 0) {
      IMemory* dev = d->devices[d->wiring[address].handle];
      dev->poke(d->wiring[address].address, val);

      return NO_ERROR;
   }

   return BUS_ERROR;
}
