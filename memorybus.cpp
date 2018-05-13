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
      int                  busSize;
      MemoryWiring*        wiring; //[BUS_SIZE];
      QVector<IMemory*>    devices;
      quint32              exceptionAddress;

   public:
      MemoryBusPrivate(MemoryBus* q)
         : q_ptr(q),
           wiring(0)
      {
         qDebug() << "BUS Size:" << (sizeof(this->wiring) / sizeof(MemoryWiring));
      }

      ~MemoryBusPrivate() {
         if (this->wiring)
            delete[] this->wiring;
      }

   private:
      MemoryBus* q_ptr;
      Q_DECLARE_PUBLIC(MemoryBus)

};

MemoryBus::MemoryBus(int size, QObject *parent)
   : QObject(parent),
     d_ptr(new MemoryBusPrivate(this))
{
   Q_D(MemoryBus);

   d->busSize = size;
   d->wiring = new MemoryWiring[size];
}

MemoryBus::~MemoryBus()
{
   delete d_ptr;
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

   if (start >= d->busSize || end > d->busSize) {
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

   if (src >= d->busSize) {
      qCritical() << "Can't wire address past bus size:" <<  src;
      return;
   }

   d->wiring[src].address = dst;
   d->wiring[src].handle  = device;
}

int MemoryBus::peek(quint32 address, quint8& val) {
   Q_D(MemoryBus);

   // Check if adress is legal
   if (Q_UNLIKELY(address >= d->busSize)) {
      d->exceptionAddress = address;
      val = 0;
      return BUS_ERROR;
   }

   // Check if adress is hooked up
   if(Q_LIKELY(d->wiring[address].handle >= 0)) {
      IMemory* dev = d->devices[d->wiring[address].handle];
      return dev->peek(d->wiring[address].address, val);
   } else {
      d->exceptionAddress = address;
      val = 0;
      return BUS_ERROR;
   }
}

int MemoryBus::poke(quint32 address, quint8 val) {
   Q_D(MemoryBus);

   // Check if adress is legal
   if (address >= d->busSize) {
      d->exceptionAddress = address;
      return BUS_ERROR;
   }

   if(d->wiring[address].handle >= 0) {
      IMemory* dev = d->devices[d->wiring[address].handle];
      return dev->poke(d->wiring[address].address, val);
   }

   d->exceptionAddress = address;
   return BUS_ERROR;
}

quint32 MemoryBus::lastExceptionAddress()
{
   return 0;
}
