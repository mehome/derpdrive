#include "emulator.h"

#include <QThread>

#include <memorybus.h>
#include <chips/motorola68000.h>
#include <cartridge.h>
#include <ram.h>

class EmulatorPrivate {
   public:
      MemoryBus*     bus;
      Cartridge*     cartridge;
      Motorola68000* cpu;
      Ram*           ram;

   public:
      EmulatorPrivate(Emulator* q)
         : q_ptr(q)
      {

      }

   public:
      Emulator* q_ptr;
      Q_DECLARE_PUBLIC(Emulator)
};

Emulator::Emulator(QObject *parent)
   : QObject(parent),
     d_ptr(new EmulatorPrivate(this))
{
   Q_D(Emulator);

   d->bus         = new MemoryBus(this);
   d->ram         = new Ram(0xFFFF, this);
   d->cpu         = new Motorola68000(this);
   d->cartridge   = new Cartridge(this);

   /*
    * Bus Setup
    */
   qint32 deviceHandle;

   // Setup RAM
   deviceHandle = d->bus->attachDevice(d->ram);
   d->bus->wire(0xFF0000, 0xFFFFFF, 0xFF0000, deviceHandle);

   // Setup Cardtridge Slot
   deviceHandle = d->bus->attachDevice(d->cartridge);
   d->bus->wire(0x000000, 0x3FFFFF, 0x0, deviceHandle);

   // Setup CPU
   d->cpu->attachBus(d->bus);

   // Load ROM
   d->cartridge->load("roms/Star Trek - Deep Space Nine - Crossroads of Time (Europe).md");
}

void Emulator::startEmulation()
{
   Q_D(Emulator);

   d->cpu->reset();

   while(true) {
      d->cpu->clock(8);
      QThread::sleep(0);
   }
}

void Emulator::stopEmulation()
{

}
