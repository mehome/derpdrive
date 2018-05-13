#ifndef EXTENSIONPORT_H
#define EXTENSIONPORT_H

#include <QObject>
#include <memorybus.h>

class ExtensionPortPrivate;
class ExtensionPort
      : public QObject,
        public IMemory
{
      Q_OBJECT
   public:
      explicit ExtensionPort(QObject *parent = nullptr);
      ~ExtensionPort();

      int      peek(quint32 address, quint8& val);
      int      poke(quint32 address, quint8 val);

   signals:

   public slots:

   private:
      ExtensionPortPrivate* d_ptr;
      Q_DECLARE_PRIVATE(ExtensionPort)
};

#endif // EXTENSIONPORT_H
