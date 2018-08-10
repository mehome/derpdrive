#include "controller.h"

#include <QDebug>
#include <SDL2/SDL.h>

class ControllerPrivate {
      // Register
      quint16 data;
      quint16 control;

      quint16 serialTX;
      quint16 serialRX;
      quint16 serialControl;

   public:
      ControllerPrivate(Controller* q)
         : q_ptr(q),
           data(0),
           control(0),
           serialTX(0),
           serialRX(0),
           serialControl(0)
      {
      }

   private:
      Controller* q_ptr;
      Q_DECLARE_PUBLIC(Controller)
};

Controller::Controller(QObject *parent)
   : QObject(parent),
     d_ptr(new ControllerPrivate(this))
{
    qDebug() << "Controller count" << SDL_NumJoysticks();
}

Controller::~Controller()
{
   delete d_ptr;
}

int Controller::peek(quint32 address, quint8& val)
{
   Q_D(Controller);

   switch (address) {
      case 0:
         val = (d->data & 0xFF00) >> 8;
         return MemoryBus::NO_ERROR;

      case 1:
         val = d->data & 0xFF;
         return MemoryBus::NO_ERROR;

      case 2:
         val = (d->control & 0xFF00) >> 8;
         return MemoryBus::NO_ERROR;

      case 3:
         val = d->control & 0xFF;
         return MemoryBus::NO_ERROR;

      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
      case 9:
         val = 0;
         return NO_ERROR;

      default:
         return MemoryBus::BUS_ERROR;
   }
}

int Controller::poke(quint32 address, quint8 val)
{
   Q_D(Controller);

   switch (address) {
      case 0:
         d->data = (d->data & 0xFF) | (val << 8);
         return MemoryBus::NO_ERROR;

      case 1:
         d->data = (d->data & 0xFF00) | val;
         return MemoryBus::NO_ERROR;

      case 2:
         d->control = (d->control & 0xFF) | (val << 8);
         return MemoryBus::NO_ERROR;

      case 3:
         d->control = (d->control & 0xFF00) | val;
         return MemoryBus::NO_ERROR;

      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
      case 9:
         return NO_ERROR;

      default:
         return MemoryBus::BUS_ERROR;
   }
}
