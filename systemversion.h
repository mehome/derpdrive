#ifndef SYSTEMVERSION_H
#define SYSTEMVERSION_H

#include <QObject>

#include <memorybus.h>

class SystemVersionPrivate;
class SystemVersion
      : public QObject,
        public IMemory
{
      Q_OBJECT

   public:
      explicit SystemVersion(QObject* parent = 0);
      ~SystemVersion();

      int      peek(quint32 address, quint8& val);
      int      poke(quint32 address, quint8 val);

   private:
      SystemVersionPrivate* d_ptr;
      Q_DECLARE_PRIVATE(SystemVersion)
};

#endif // SYSTEMVERSION_H
