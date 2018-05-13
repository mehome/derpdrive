#include "systemversion.h"

class SystemVersionPrivate {
   public:
      bool  overseasModel;
      bool  palMode;
      bool  fddConnected;
      int   megadriveVersion;

   public:
      SystemVersionPrivate(SystemVersion* q)
         : q_ptr(q),
           overseasModel(true),
           palMode(true),
           fddConnected(false),
           megadriveVersion(0)
      {
      }

   private:
      SystemVersion* q_ptr;
      Q_DECLARE_PUBLIC(SystemVersion)
};

SystemVersion::SystemVersion(QObject* parent)
   : QObject(parent),
     d_ptr(new SystemVersionPrivate(this))
{

}

SystemVersion::~SystemVersion()
{
    delete d_ptr;
}

int SystemVersion::peek(quint32 address, quint8& val)
{
   Q_D(SystemVersion);

   if (address > 0x1)
      return MemoryBus::BUS_ERROR;

   if (address == 0x0) {
      val = 0;
   } else {
      val = d->overseasModel ? 1 << 7 : 0 |
            d->palMode ? 1 << 6 : 0 |
            d->fddConnected ? 1 << 5 : 0 |
            (d->megadriveVersion & 0x0F);
   }

   return MemoryBus::NO_ERROR;
}

int SystemVersion::poke(quint32 address, quint8 val)
{
   Q_UNUSED(address);
   Q_UNUSED(val);

   return MemoryBus::BUS_ERROR;
}
