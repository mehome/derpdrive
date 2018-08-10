#ifndef EMULATOR_H
#define EMULATOR_H

#include <QObject>
#include <SDL2/SDL.h>

class VDP;
class Motorola68000;

class EmulatorPrivate;
class Emulator : public QObject
{
    Q_OBJECT
public:
    explicit Emulator(SDL_Renderer* renderer, QObject *parent = nullptr);
    ~Emulator();

    void reset();
    void emulate();

    bool loadCartridge(QString file);

    void setClockRate(long clock);

    Motorola68000* mainCpu() const;
    VDP* vdp() const;

signals:

public slots:
    void reportFps();

private slots:
    void updateFpsCount();

private:
    EmulatorPrivate* d_ptr;
    Q_DECLARE_PRIVATE(Emulator)
};

#endif // EMULATOR_H
