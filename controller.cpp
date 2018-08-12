#include "controller.h"

#include <QDebug>
#include <SDL2/SDL.h>

class ControllerPrivate {
    // Register
    quint8 data;
    quint8 control;

    quint8 serialTX;
    quint8 serialRX;
    quint8 serialControl;

    int     controllerNum;

    SDL_GameController* controller;

public:
    ControllerPrivate(Controller* q, int num)
        : q_ptr(q),
          data(0),
          control(0),
          serialTX(0),
          serialRX(0),
          serialControl(0),
          controllerNum(num)
    {
        this->controller = SDL_GameControllerOpen(num);
        if (!this->controller)
            qDebug() << "Failed to open controller" << num;
        else
            qDebug() << "Opened" << SDL_GameControllerName(this->controller);
    }

    quint8 readState() {
        if (!this->controller)
            return 0;

        quint8 data;

        // Read actual controller state
        int lrAxis = SDL_GameControllerGetAxis(this->controller, SDL_CONTROLLER_AXIS_LEFTX);
        int udAxis = SDL_GameControllerGetAxis(this->controller, SDL_CONTROLLER_AXIS_LEFTY);
        bool buttonA = SDL_GameControllerGetButton(this->controller, SDL_CONTROLLER_BUTTON_A);
        bool buttonB = SDL_GameControllerGetButton(this->controller, SDL_CONTROLLER_BUTTON_B);
        bool buttonC = SDL_GameControllerGetButton(this->controller, SDL_CONTROLLER_BUTTON_Y);
        bool buttonStart = SDL_GameControllerGetButton(this->controller, SDL_CONTROLLER_BUTTON_START);

        //qDebug() << "Axis" << lrAxis << udAxis << buttonA << buttonB << buttonC << buttonStart;

        if (this->data & 0x40) {
            data =  (0x40) |
                    (buttonC ? 0x00 : 0x20 ) |
                    (buttonB ? 0x00 : 0x10 ) |
                    ((lrAxis > 5000)  ? 0x00 : 0x08 ) |
                    ((lrAxis < -5000) ? 0x00 : 0x04 ) |
                    ((udAxis > 5000)  ? 0x00 : 0x02 ) |
                    ((udAxis < -5000) ? 0x00 : 0x01 );
        } else {
            data =  (0x00) |
                    (buttonStart ? 0x00 : 0x20 ) |
                    (buttonA ? 0x00 : 0x10 ) |
                    ((udAxis > 5000)  ? 0x00 : 0x01 ) |
                    ((udAxis < -5000) ? 0x00 : 0x02 );
        }

        //qDebug() << QString::number(data, 16).rightJustified(2, '0');

        return data;
    }

private:
    Controller* q_ptr;
    Q_DECLARE_PUBLIC(Controller)
};

Controller::Controller(int controller, QObject *parent)
    : QObject(parent),
      d_ptr(new ControllerPrivate(this, controller))
{
    //qDebug() << "Controller count" << SDL_NumJoysticks();
}

Controller::~Controller()
{
    delete d_ptr;
}

int Controller::peek(quint32 address, quint8& val)
{
    Q_D(Controller);

    //qDebug() << "CTRL RD" << d->controllerNum << address;

    switch (address) {
    case 0:
        val = 0;
        return MemoryBus::NO_ERROR;

    case 1:
        val = d->readState();
        return MemoryBus::NO_ERROR;

    case 2:
        val = 0;
        return MemoryBus::NO_ERROR;

    case 3:
        val = d->control;
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

    //qDebug() << "CTRL WR" << d->controllerNum << address << val;

    switch (address) {
    case 0:
        return MemoryBus::NO_ERROR;

    case 1:
        d->data = val;
        return MemoryBus::NO_ERROR;

    case 2:
        return MemoryBus::NO_ERROR;

    case 3:
        d->control = val;
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
