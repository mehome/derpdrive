#include <QApplication>
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

int main(int argc, char *argv[])
{
   // qInstallMessageHandler(debugOutput);


    QApplication a(argc, argv);

    SDL_Init(SDL_INIT_VIDEO);

    MainWindow w;
    w.show();

    int result = a.exec();

    SDL_Quit();

    return result;
}
