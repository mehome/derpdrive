#ifndef MOTOROLA68000_H
#define MOTOROLA68000_H

#include <QObject>

#include <memorybus.h>

class Device;

class Motorola68000Private;
class Motorola68000 : public QObject
{
      Q_OBJECT
   public:
      explicit Motorola68000(QObject *parent = nullptr);

      // Setup
      void  attachBus(MemoryBus* bus);

      // Options
      void  setTracing(bool tracing);
      bool  tracing() const;

      // Hardware functions
      void  setDisabled(bool disabled);
      int   clock(int ticks);
      void  reset();
      void  interruptRequest(Device* device, int level);
      void  clearInterrupt(Device* device, int level);

      quint32  programCounter() const;

   signals:

   public slots:

   private:
      Motorola68000Private* d_ptr;
      Q_DECLARE_PRIVATE(Motorola68000)
};

#endif // MOTOROLA68000_H
