#include "emulator.h"

#include <QThread>
#include <QTimer>
#include <QDebug>

#include <config.h>
#include <memorybus.h>
#include <chips/motorola68000.h>
#include <chips/z80.h>
#include <chips/vdp.h>
#include <chips/ym2612.h>
#include <cartridge.h>
#include <ram.h>
#include <systemversion.h>
#include <controller.h>
#include <extensionport.h>

class EmulatorPrivate {
public:
    MemoryBus*     bus;
    MemoryBus*     z80Bus;
    Cartridge*     cartridge;
    Motorola68000* cpu;
    VDP*           vdp;
    Z80*           z80;
    Ram*           ram;
    Ram*           soundRam;
    YM2612*        ym2612;
    SystemVersion* systemVersion;
    Controller*    controllerA;
    Controller*    controllerB;
    ExtensionPort* extensionPort;
    QTimer*        fpsTimer;
    int            cyclesCount;
    int            currentCycles;
    int            fpsCount;
    int            currentFps;

    long           masterClockRate;
    long           seventhCycleCount;
    long           fifteenthCycleCount;

public:
    EmulatorPrivate(Emulator* q)
        : q_ptr(q),
          cyclesCount(0),
          masterClockRate(0),
          fpsCount(0),
          currentFps(0),
          seventhCycleCount(0),
          fifteenthCycleCount(0)
    {

    }

public:
    Emulator* q_ptr;
    Q_DECLARE_PUBLIC(Emulator)
};

Emulator::Emulator(SDL_Renderer* renderer, QObject *parent)
    : QObject(parent),
      d_ptr(new EmulatorPrivate(this))
{
    Q_D(Emulator);

    d->fpsTimer = new QTimer(this);
    d->fpsTimer->setInterval(1000);
    d->fpsTimer->start();
    connect(d->fpsTimer, &QTimer::timeout, this, &Emulator::reportFps);

    d->bus         = new MemoryBus(M68K_BUS_SIZE, this);
    d->z80Bus      = new MemoryBus(Z80_BUS_SIZE, this);

    d->ram         = new Ram(0xFFFF, this);
    d->soundRam    = new Ram(0x1FFF, this);
    d->cpu         = new Motorola68000(this);
    d->z80         = new Z80(this);
    d->vdp         = new VDP(renderer, this);
    d->ym2612      = new YM2612(this);
    d->cartridge   = new Cartridge(this);
    d->systemVersion = new SystemVersion(this);
    d->controllerA = new Controller(this);
    d->controllerB = new Controller(this);
    d->extensionPort = new ExtensionPort(this);

    connect(d->vdp, &VDP::frameUpdated, this, &Emulator::updateFpsCount);

    /*
    * Bus Setup
    */
    qint32 deviceHandle;

    // Setup RAM
    deviceHandle = d->bus->attachDevice(d->ram);
    for (int i=0; i < 32; i++) {
        int base = 0xE00000 + 0x10000*i;
        d->bus->wire(base, base + 0xFFFF, 0x0, deviceHandle);
    }
    //d->bus->wire(0xFF0000, 0xFFFFFF, 0x0, deviceHandle);

    // Setup Cardtridge Slot
    deviceHandle = d->bus->attachDevice(d->cartridge);
    d->bus->wire(0x000000, 0x3FFFFF, 0x0,       deviceHandle);
    d->bus->wire(0xA130F0, 0xA13100, 0xA130F0,  deviceHandle);   // SSFII-style bank switching

    // Setup Z80 Area
    deviceHandle = d->bus->attachDevice(d->soundRam);
    d->bus->wire(0xA00000, 0xA01FFF, 0x0, deviceHandle);

    deviceHandle = d->bus->attachDevice(d->ym2612);
    d->bus->wire(0xA04000, 0xA04003, 0x0, deviceHandle);

    // Setup Z80 Bus
    deviceHandle = d->z80Bus->attachDevice(d->soundRam);
    d->z80Bus->wire(0x0000, 0x1FFF, 0x0, deviceHandle);

    deviceHandle = d->z80Bus->attachDevice(d->ym2612);
    d->z80Bus->wire(0x4000, 0x4003, 0x0, deviceHandle);

    // Setup Hardware version
    deviceHandle = d->bus->attachDevice(d->systemVersion);
    d->bus->wire(0xA10000, 0xA10001, 0x0, deviceHandle);

    // Setup Controller A
    deviceHandle = d->bus->attachDevice(d->controllerA);
    d->bus->wire(0xA10002, 0xA10003, 0x0, deviceHandle);
    d->bus->wire(0xA10008, 0xA10009, 0x2, deviceHandle);
    d->bus->wire(0xA1000E, 0xA1000F, 0x4, deviceHandle);
    d->bus->wire(0xA10010, 0xA10011, 0x6, deviceHandle);
    d->bus->wire(0xA10012, 0xA10013, 0x8, deviceHandle);

    // Setup Controller B
    deviceHandle = d->bus->attachDevice(d->controllerB);
    d->bus->wire(0xA10004, 0xA10005, 0x0, deviceHandle);
    d->bus->wire(0xA1000A, 0xA1000B, 0x2, deviceHandle);
    d->bus->wire(0xA10014, 0xA10015, 0x4, deviceHandle);
    d->bus->wire(0xA10016, 0xA10017, 0x6, deviceHandle);
    d->bus->wire(0xA10018, 0xA10019, 0x8, deviceHandle);

    // Setup Z80 Controls
    deviceHandle = d->bus->attachDevice(d->z80);
    d->bus->wire(0xA11100, 0xA11101, 0x0, deviceHandle);
    d->bus->wire(0xA11200, 0xA11201, 0x2, deviceHandle);

    // Setup Extension Port
    deviceHandle = d->bus->attachDevice(d->extensionPort);
    d->bus->wire(0xA10006, 0xA10007, 0x0,  deviceHandle);
    d->bus->wire(0xA1000C, 0xA1000D, 0x2,  deviceHandle);
    d->bus->wire(0xA1001A, 0xA1001B, 0x4,  deviceHandle);
    d->bus->wire(0xA1001C, 0xA1001D, 0x6,  deviceHandle);
    d->bus->wire(0xA1001E, 0xA1001F, 0x8,  deviceHandle);

    // Setup VDP
    d->vdp->attachZ80(d->z80);
    d->vdp->attachBus(d->bus);

    deviceHandle = d->bus->attachDevice(d->vdp);
    d->bus->wire(0xC00000, 0xC00001, 0x00, deviceHandle); // Data Port
    d->bus->wire(0xC00002, 0xC00003, 0x00, deviceHandle); // Data Port (Mirror)
    d->bus->wire(0xC00004, 0xC00005, 0x02, deviceHandle); // Control Port
    d->bus->wire(0xC00006, 0xC00007, 0x02, deviceHandle); // Control Port (Mirror)
    d->bus->wire(0xC00008, 0xC00009, 0x04, deviceHandle); // H/V Counter
    d->bus->wire(0xC0000A, 0xC0000B, 0x04, deviceHandle); // H/V Counter (Mirror)
    d->bus->wire(0xC0000C, 0xC0000D, 0x04, deviceHandle); // H/V Counter (Mirror)
    d->bus->wire(0xC0000E, 0xC0000F, 0x04, deviceHandle); // H/V Counter (Mirror)
    d->bus->wire(0xC00011, 0xC00012, 0x06, deviceHandle); // SN76489 PSG
    d->bus->wire(0xC00013, 0xC00014, 0x06, deviceHandle); // SN76489 PSG (Mirror)
    d->bus->wire(0xC00015, 0xC00016, 0x06, deviceHandle); // SN76489 PSG (Mirror)
    d->bus->wire(0xC0001C, 0xC0001D, 0x08, deviceHandle); // "Disable"/Debug register
    d->bus->wire(0xC0001E, 0xC0001F, 0x08, deviceHandle); // "Disable"/Debug register (Mirror)

    // Setup CPU
    d->cpu->attachBus(d->bus);
    d->z80->attachBus(d->z80Bus);

    // Setup Interrupt Lanes
    d->vdp->attachCpu(d->cpu);

}

Emulator::~Emulator()
{
    delete d_ptr;
}

void Emulator::reset()
{
    Q_D(Emulator);

    d->vdp->reset();
    d->cpu->reset();
    d->z80->reset();
}

void Emulator::emulate()
{
    Q_D(Emulator);

    int cycles = 0;

    while(cycles < 1067040) {
        d->vdp->clock(105);
        d->cpu->clock(60); // 60
        d->z80->clock(28); // 28

        cycles += 420;
        d->cyclesCount += 420;
    }

    // M86K = 1 / 7 (60/420)
    // Z80 = 1 / 15 (28/420)
    // FM = 1 / 7   (60/420)
    // VDP = 1 / 4  (105/420)

    //for()

    //NTSC: 896.050 Cycles
    //PAL: 1.067.040 Cycles
    //d->cpu->clock(152434);
    //d->fpsCount++;
}

bool Emulator::loadCartridge(QString file)
{
    Q_D(Emulator);

    d->cartridge->load(file);
    this->reset();
}

void Emulator::setClockRate(long clock)
{
    Q_D(Emulator);

    d->masterClockRate = clock;
}

Motorola68000 *Emulator::mainCpu() const
{
    Q_D(const Emulator);

    return d->cpu;
}

VDP*Emulator::vdp() const
{
    Q_D(const Emulator);

    return d->vdp;
}

void Emulator::reportFps() {
    Q_D(Emulator);

    qDebug() << "Cycles:" << d->cyclesCount << "Fps:" << d->fpsCount;
    d->cyclesCount = 0;
    d->fpsCount = 0;
}

void Emulator::updateFpsCount()
{
    Q_D(Emulator);

    d->fpsCount++;
}
