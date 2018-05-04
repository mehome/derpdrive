#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <QObject>

#include <memorybus.h>

class CartridgePrivate;
class Cartridge
      : public QObject,
        public IMemory
{
      Q_OBJECT
   public:
      explicit Cartridge(QObject *parent = nullptr);

      void     load(QString path);

      int      peek(quint32 address, quint8& val);
      int      poke(quint32 address, quint8 val);

   signals:

   public slots:

   private:
      CartridgePrivate* d_ptr;
      Q_DECLARE_PRIVATE(Cartridge)
};

#endif // CARTRIDGE_H
