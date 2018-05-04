#ifndef MOTOROLA68000_H
#define MOTOROLA68000_H

#include <QObject>

#include <memorybus.h>

class Motorola68000Private;
class Motorola68000 : public QObject
{
      Q_OBJECT
   public:
      explicit Motorola68000(QObject *parent = nullptr);

      // Setup
      void  attachBus(MemoryBus* bus);

      // Hardware functions
      int   clock(int ticks);
      void  reset();

   signals:

   public slots:

   private:
      Motorola68000Private* d_ptr;
      Q_DECLARE_PRIVATE(Motorola68000)
};

#endif // MOTOROLA68000_H
