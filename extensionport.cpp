#include "extensionport.h"

class ExtensionPortPrivate {
   public:
      ExtensionPortPrivate(ExtensionPort* q)
         : q_ptr(q)
      {

      }

   private:
      ExtensionPort* q_ptr;
      Q_DECLARE_PUBLIC(ExtensionPort)
};

ExtensionPort::ExtensionPort(QObject *parent)
   : QObject(parent),
     d_ptr(new ExtensionPortPrivate(this))
{

}

ExtensionPort::~ExtensionPort()
{

}

int ExtensionPort::peek(quint32 address, quint8& val)
{
   val = 0;
   return NO_ERROR;
}

int ExtensionPort::poke(quint32 address, quint8 val)
{
   return NO_ERROR;
}
