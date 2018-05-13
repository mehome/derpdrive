#include "device.h"

class DevicePrivate {
   public:
      bool           interruptPending;
      Motorola68000* cpu;

   public:
      DevicePrivate(Device* q)
         : q_ptr(q),
           interruptPending(false),
           cpu(0)
      {
      }

   private:
      Device* q_ptr;
      Q_DECLARE_PUBLIC(Device)
};

Device::Device()
   : d_ptr(new DevicePrivate(this))
{

}

Device::~Device() {
   delete d_ptr;
}

void Device::attachCpu(Motorola68000* cpu)
{
   Q_D(Device);

   d->cpu = cpu;
}

void Device::reset()
{

}

void Device::interruptRequest(int level)
{
   Q_D(Device);

   // If no interrupt is pending, then request one
   if (!d->interruptPending) {
      d->interruptPending = true;
      d->cpu->interruptRequest(this, level);
   }
}

int Device::interruptAcknowledge(unsigned int level)
{
   Q_D(Device);

   if (d->interruptPending) {
      d->interruptPending = false;
      return AUTOVECTOR_INTERRUPT;
   } else {
      return SPURIOUS_INTERRUPT;
   }
}

void Device::clearInterrupt()
{
   Q_D(Device);

   if (d->interruptPending) {
      d->interruptPending = false;
      d->cpu->clearInterrupt(this, 0);
   }
}

bool Device::interruptPending() {
   Q_D(Device);

   return d->interruptPending;
}
