#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "chips/vdp.h"

#include <QTimer>

MainWindow::MainWindow(QWidget *parent) :
   QMainWindow(parent),
   ui(new Ui::MainWindow)
{
   ui->setupUi(this);

   this->frameTimer = new QTimer(this);
   this->frameTimer->setInterval(0);
   this->frameTimer->start();
   connect(this->frameTimer, &QTimer::timeout, this, &MainWindow::emulateFrame);

   this->emulator = new Emulator(this);
   this->emulator->setClockRate(53203424);
   this->emulator->reset();

   connect(this->emulator->vdp(), &VDP::frameUpdated, this, &MainWindow::updateFrame);
}

MainWindow::~MainWindow()
{
   delete ui;
}

void MainWindow::emulateFrame()
{
   this->emulator->emulate();
}

void MainWindow::updateFrame(QImage* frame)
{
   ui->label->setPixmap(QPixmap::fromImage(*frame));
}
