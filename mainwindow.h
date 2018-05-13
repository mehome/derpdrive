#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <emulator.h>

namespace Ui {
   class MainWindow;
}

class MainWindow : public QMainWindow
{
      Q_OBJECT

   private:
      Emulator* emulator;

   public:
      explicit MainWindow(QWidget *parent = 0);
      ~MainWindow();

   public slots:
      void emulateFrame();
      void updateFrame(QImage* frame);

   private:
      Ui::MainWindow *ui;
      QTimer*         frameTimer;
};

#endif // MAINWINDOW_H
