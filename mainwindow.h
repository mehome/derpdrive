#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <emulator.h>

namespace Ui {
   class MainWindow;
}

class M68KDebugger;
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

   private slots:
      void on_actionView_VRAM_triggered();
      void on_actionDebugger_M68K_triggered();
      void on_actionReset_Z80_triggered();
      void on_actionReset_M68K_2_triggered();

private:
      Ui::MainWindow*   ui;
      QTimer*           frameTimer;
      M68KDebugger*     m68kdebugger;
};

#endif // MAINWINDOW_H
