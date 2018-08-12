#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QDebug>

#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "mainwindow.h"

void debugOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    Q_UNUSED(context);

    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stdout, "Debug: %s\n", localMsg.constData());
        break;
    case QtInfoMsg:
        fprintf(stdout, "Info: %s\n", localMsg.constData());
        break;
    case QtWarningMsg:
        fprintf(stdout, "Warning: %s\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        fprintf(stdout, "Critical: %s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        fprintf(stdout, "Fatal: %s\n", localMsg.constData());
        break;
    }
}

class EventFilter
        : public QAbstractNativeEventFilter
{
public:
    EventFilter() {

    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) {
        if (eventType == "windows_generic_MSG") {

        }
    }
};

int main(int argc, char *argv[])
{
    // qInstallMessageHandler(debugOutput);
    EventFilter sdlPipe;

    QApplication a(argc, argv);
    //a.installNativeEventFilter(&sdlPipe);

    SDL_SetMainReady();
    if ( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0 )
        qDebug() << "Failed to initialize SDL";

    SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP,        "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,  "1");

    SDL_GameControllerEventState(SDL_IGNORE);

    MainWindow w;
    w.show();

    int result = a.exec();
    a.removeNativeEventFilter(&sdlPipe);

    SDL_Quit();

    return result;
}
