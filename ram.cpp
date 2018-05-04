#include "ram.h"

class RamPrivate {
   public:
      QByteArray data;

   public:
      RamPrivate(Ram* q)
         : q_ptr(q)
      {}

   private:
      Ram* q_ptr;
      Q_DECLARE_PUBLIC(Ram)
};

Ram::Ram(int size, QObject *parent)
   : QObject(parent),
     d_ptr(new RamPrivate(this))
{
   Q_D(Ram);

   d->data.resize(size);
}

int Ram::peek(quint32 address, quint8& val)
{
   Q_D(Ram);

   val = d->data[address];
   return NO_ERROR;
}

int Ram::poke(quint32 address, quint8 val)
{
   Q_D(Ram);

   d->data[address] = val;
   return NO_ERROR;
}
