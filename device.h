#ifndef DEVICE_H
#define DEVICE_H

#include <QtGlobal>
#include <chips/motorola68000.h>

class DevicePrivate;
class Device
{
   public:
      enum InterruptType {
         AUTOVECTOR_INTERRUPT = -1,
         SPURIOUS_INTERRUPT = -2,
      };

   public:
      Device();
      virtual ~Device();

      virtual void  attachCpu(Motorola68000* cpu);

      virtual void   reset();
      virtual void   interruptRequest(int level);
      virtual int    interruptAcknowledge(unsigned int level);
      virtual void   clearInterrupt();
      virtual bool   interruptPending();

   private:
      DevicePrivate* d_ptr;
      Q_DECLARE_PRIVATE(Device)
};

#endif // DEVICE_H
