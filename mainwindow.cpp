#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "vramview.h"
#include "m68kdebugger.h"

#include "chips/vdp.h"

#include <QTimer>
#include <QFileDialog>

#include <SDL2/SDL.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m68kdebugger(nullptr)
{
    ui->setupUi(this);

    this->frameTimer = new QTimer(this);
    this->frameTimer->setInterval(0);
    //this->frameTimer->start();
    connect(this->frameTimer, &QTimer::timeout, this, &MainWindow::emulateFrame);

    ui->label->setAttribute(Qt::WA_PaintOnScreen);
    ui->label->setUpdatesEnabled(false);

    this->renderWnd = SDL_CreateWindowFrom(reinterpret_cast<const void*>(ui->label->winId()));
    this->renderer = SDL_CreateRenderer(this->renderWnd, -1, SDL_RENDERER_ACCELERATED);


    this->emulator = new Emulator(this->renderer, this);
    this->emulator->setClockRate(53203424);

    connect(this->emulator->vdp(),  &VDP::frameUpdated, this,                   &MainWindow::updateFrame);
    connect(ui->actionPlane_A,      &QAction::toggled,  this->emulator->vdp(),  &VDP::setPlaneA);
    connect(ui->actionPlane_B,      &QAction::toggled,  this->emulator->vdp(),  &VDP::setPlaneB);
    connect(ui->actionWindow_Plane, &QAction::toggled,  this->emulator->vdp(),  &VDP::setWindowPlane);
    connect(ui->actionSprites,      &QAction::toggled,  this->emulator->vdp(),  &VDP::setSprites);

    if (qApp->arguments().contains("--debug") || qApp->arguments().contains("-d"))
        this->on_actionDebugger_M68K_triggered();

    connect(this->emulator->mainCpu(), &Motorola68000::debugError, [&](unsigned int pc, unsigned int opcode, QString instruction) {
        this->on_actionDebugger_M68K_triggered();
        this->m68kdebugger->singleStep(pc, opcode, instruction);
    });
}

MainWindow::~MainWindow()
{
    SDL_DestroyRenderer(this->renderer);
    SDL_DestroyWindow(this->renderWnd);

    delete ui;
}

void MainWindow::emulateFrame()
{
    this->emulator->emulate();
}

void MainWindow::updateFrame(void* frame)
{
    //SDL_UpdateTexture(this->currentFrame, nullptr, frame->constBits(), frame->bytesPerLine());

    SDL_Rect rect;
    rect.x = 0; rect.y = 0;
    rect.w = ui->label->width();
    rect.h = ui->label->height();

    SDL_SetRenderTarget(this->renderer, nullptr);
    SDL_SetRenderDrawColor(this->renderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(this->renderer);
    SDL_RenderCopy(this->renderer, reinterpret_cast<SDL_Texture*>(frame), &rect, &rect);
    SDL_RenderPresent(this->renderer);
    //ui->label->setPixmap(QPixmap::fromImage(*frame));
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

void MainWindow::on_actionLoad_ROM_triggered()
{
    this->frameTimer->stop();

    QString fileName = QFileDialog::getOpenFileName(this, "Select ROM File...", QDir::currentPath(), "Cartridge File (*.bin *.smd *.md)");

    if (fileName.isEmpty())
        return;

    this->emulator->loadCartridge(fileName);
    this->frameTimer->start();
}
