#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>

#include <memorybus.h>

class ControllerPrivate;
class Controller
      : public QObject,
        public IMemory
{
      Q_OBJECT
   public:
      explicit Controller(int controller, QObject *parent = nullptr);
      ~Controller();

      int      peek(quint32 address, quint8& val);
      int      poke(quint32 address, quint8 val);

   signals:

   public slots:

   private:
      ControllerPrivate* d_ptr;
      Q_DECLARE_PRIVATE(Controller)
};

#endif // CONTROLLER_H
