#ifndef CPU_Z80_H
#define CPU_Z80_H

#include <QObject>
#include <memorybus.h>

class Z80Private;
class Z80
      : public QObject,
        public IMemory
{
      Q_OBJECT

   public:
      Z80(QObject* parent = 0);
      ~Z80();

      // Setup
      void           attachBus(MemoryBus* bus);
      MemoryBus*     bus();

      // Emulation
      int            clock(int cycles);

      void           reset();
      void           interrupt();

      int            peek(quint32 address, quint8& val);
      int            poke(quint32 address, quint8 val);

   private:
      Z80Private* d_ptr;
      Q_DECLARE_PRIVATE(Z80)
};

#endif // CPU_Z80_H
