#include "ym2612.h"

#include <QDebug>

#include <SDL2/SDL.h>

enum YM2612Status {
    YM2612_BUSY = 0x80,
    YM2612_OVERFLOW_A = 0x02,
    YM2612_OVERFLOW_B = 0x01
};

enum Register {
    DAC     = 0x2A,
    DACEN   = 0x2B,
};

class YM2612Private {
public:
    quint8* registersPartI;
    quint8* registersPartII;
    quint8  status;

    quint8  partISelect;
    quint8  partIISelect;

    SDL_AudioDeviceID   audioDevice;
    SDL_AudioSpec       audioSpec;

    int         currentCycles;
    qint16*     buffer;
    int         bufferPos;

public:
    YM2612Private(YM2612* q)
        : q_ptr(q),
          partISelect(0),
          partIISelect(0),
          audioDevice(0),
          currentCycles(0),
          bufferPos(0)
    {
        this->registersPartI    = reinterpret_cast<quint8*>(malloc(0x100));
        this->registersPartII   = reinterpret_cast<quint8*>(malloc(0x100));

        // Reset registers
        memset(this->registersPartI, 0, 0x100);
        memset(this->registersPartII, 0, 0x100);

        // Open audio device
        SDL_AudioSpec spec;
        memset(&spec, 0, sizeof(SDL_AudioSpec));

        spec.freq = 22050;
        spec.format = AUDIO_S16SYS;
        spec.channels = 2;
        spec.samples = 4096;

        this->audioDevice = SDL_OpenAudioDevice(nullptr, 0, &spec, &this->audioSpec, 0);

        if(this->audioDevice > 0) {
            this->buffer = reinterpret_cast<qint16*>(malloc(this->audioSpec.size));

            qDebug() << "Audio initialized!";
            qDebug() << "Buffer Size" << this->audioSpec.size;
            SDL_PauseAudioDevice(this->audioDevice, 0);
        } else {
            this->audioDevice = 0;
        }
    }

    ~YM2612Private() {
        free(this->registersPartI);
        free(this->registersPartII);

        if (this->audioDevice)
            SDL_CloseAudioDevice(this->audioDevice);
    }

    void updateSample() {

    }

private:
    YM2612* q_ptr;
    Q_DECLARE_PUBLIC(YM2612)
};

YM2612::YM2612(QObject *parent)
    : QObject(parent),
      d_ptr(new YM2612Private(this))
{

}

YM2612::~YM2612()
{
    delete this->d_ptr;
}

int YM2612::peek(quint32 address, quint8 &val)
{
    Q_D(YM2612);
    Q_UNUSED(address);

    val = d->status;
    return NO_ERROR;
}

int YM2612::poke(quint32 address, quint8 val)
{
    Q_D(YM2612);

    /*if ( && address == 0x4001 && d->partISelect == DAC) {
        qDebug() << "YM2612 WR"
                 << QString::number(address, 16).rightJustified(6, '0')
                 << QString::number(val, 16).rightJustified(2, '0');
    }*/

    switch (address) {
    case 0x4000:
        d->partISelect = val;
        break;

    case 0x4001:
        d->registersPartI[d->partISelect] = val;
        if (d->partISelect == DAC)
            d->status |= YM2612_BUSY;

        break;

    case 0x4002:
        d->partIISelect = val;
        break;

    case 0x4003:
        d->registersPartII[d->partIISelect] = val;
        break;
    }

    return NO_ERROR;
}

void YM2612::clock(int cycles) {
    Q_D(YM2612);

    d->currentCycles -= cycles;

    while (d->currentCycles <= -345) {
        int sample = d->bufferPos * 2;
        qint16 data = 0;

        if (d->registersPartI[DACEN] & 0x80) {
            data = static_cast<qint16>((d->registersPartI[DAC] * 0xFF) - 0x8000);

            /*qDebug() << "Sample"
                     << QString::number(d->registersPartI[DAC], 16).rightJustified(2, '0')
                     << data;*/
        }

        d->buffer[sample + 0] = data; // L
        d->buffer[sample + 1] = data; // R

        d->bufferPos+=1;
        if (d->bufferPos >= d->audioSpec.samples) {
            qDebug() << SDL_GetQueuedAudioSize(d->audioDevice);

            SDL_QueueAudio(d->audioDevice, d->buffer, d->audioSpec.size);
            d->bufferPos = 0;
        }

        d->status &= ~YM2612_BUSY;

        d->currentCycles += 345;
    }
}
