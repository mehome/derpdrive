#ifndef ROM_H
#define ROM_H

#include <QObject>

#include <memorybus.h>

class RamPrivate;
class Ram
      : public QObject,
        public IMemory
{
      Q_OBJECT
   public:
      explicit Ram(int size, QObject *parent = nullptr);

      int      peek(quint32 address, quint8& val);
      int      poke(quint32 address, quint8 val);

   signals:

   public slots:

   private:
      RamPrivate* d_ptr;
      Q_DECLARE_PRIVATE(Ram)
};

#endif // ROM_H
