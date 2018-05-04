#ifndef MEMORYBUS_H
#define MEMORYBUS_H

#include <QObject>

struct IMemory {
   public:
      enum Error {
         NO_ERROR,
         BUS_ERROR,
      };

   public:
      virtual int peek(quint32 address, quint8& val) = 0;
      virtual int poke(quint32 address, quint8 val) = 0;
};

class MemoryBusPrivate;
class MemoryBus :
   public QObject, public IMemory
{
      Q_OBJECT

   public:
      explicit MemoryBus(QObject *parent = nullptr);

      qint32   attachDevice(IMemory* dev);
      void     detachDevice(IMemory* dev);

      void     wire(int start, int end, int base, qint32 device);
      void     wire(int src, int dst, qint32 device);

      int      peek(quint32 address, quint8& val);
      int      poke(quint32 address, quint8 val);

   signals:

   public slots:

   private:
      MemoryBusPrivate* d_ptr;
      Q_DECLARE_PRIVATE(MemoryBus)
};

#endif // MEMORYBUS_H
