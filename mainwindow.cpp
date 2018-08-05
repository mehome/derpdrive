#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "vramview.h"
#include "m68kdebugger.h"

#include "chips/vdp.h"

#include <QTimer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m68kdebugger(nullptr)
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

    if (qApp->arguments().contains("--debug") || qApp->arguments().contains("-d"))
        this->on_actionDebugger_M68K_triggered();

    connect(this->emulator->mainCpu(), &Motorola68000::debugError, [&](unsigned int pc, unsigned int opcode, QString instruction) {
        this->on_actionDebugger_M68K_triggered();
        this->m68kdebugger->singleStep(pc, opcode, instruction);
    });
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

void MainWindow::on_actionView_VRAM_triggered()
{
    VRAMView* view = new VRAMView(this);
    view->setVDP(this->emulator->vdp());
    view->show();

    connect(view, &VRAMView::finished, view, &VRAMView::deleteLater);
}

void MainWindow::on_actionDebugger_M68K_triggered()
{
    if (!this->m68kdebugger) {
        this->m68kdebugger = new M68KDebugger(this, this->emulator->mainCpu());
        this->m68kdebugger->show();

        connect(this->m68kdebugger, &M68KDebugger::finished, [&]() {
            this->m68kdebugger->deleteLater();
            this->m68kdebugger = nullptr;
        });
    } else {
        this->m68kdebugger->activateWindow();
    }
}

void MainWindow::on_actionReset_Z80_triggered()
{

}

void MainWindow::on_actionReset_M68K_2_triggered()
{
    this->emulator->mainCpu()->reset();
}
