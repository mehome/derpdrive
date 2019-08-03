#include "ym2612.h"

#include <QDebug>

#include <math.h>
#include <SDL2/SDL.h>

enum YM2612Status {
    YM2612_BUSY = 0x80,
    YM2612_OVERFLOW_A = 0x02,
    YM2612_OVERFLOW_B = 0x01
};

enum Register {
    KEYSTATE    = 0x28,
    DAC         = 0x2A,
    DACEN       = 0x2B,
};

enum OPERATOR_STATE {
    OP_STATE_OFF,
    OP_STATE_ATTACK,
    OP_STATE_SUSTAIN,
    OP_STATE_DECAY,
};

struct Operator {
    int         state;
    int         totalLevel;
    int         algorithm;
    float       phase;
    float       frequency;
    float       envelopeScale;
    float       envelope;
    float       feedback;
    float       in;
    float       out;
    bool        am;
    quint8      ar;
    quint8      rs;
    quint8      d1r;
    quint8      d2r;
    quint8      rr;
};

struct Channel {
    Operator    op[4];
    float       frequency;
    float       out;
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

    float       waveform[1024];
    Channel     channel[6];

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

        // Compute basic waveform
        for(int i=0; i < 1024; i++) {
            double degree = (i / 1024.0) * 360.0;
            double radian = degree * M_PI / 180;

            this->waveform[i] = static_cast<float>(sin(radian));
        }

        memset(&this->channel, 0, sizeof(Channel) * 6);
    }

    ~YM2612Private() {
        free(this->registersPartI);
        free(this->registersPartII);

        if (this->audioDevice)
            SDL_CloseAudioDevice(this->audioDevice);
    }

    void updateOperator(int channel, int op) {
        Operator* opp = &this->channel[channel].op[op];


    }

    void updateEnvelope(int channel, int op) {

    }

    void modulateOperators() {

    }

    void updateChannels() {
        for (int c=0; c < 6; c++) {
            if (c == 5 && (this->registersPartI[DACEN] & 0x80)) {
                this->channel[c].out = this->registersPartI[DAC] / 255.0f - .5f;
            } else {
                if (c < 3)
                    this->channel[c].frequency = this->registersPartI[0xA0 + c] | (this->registersPartI[0xA4 + c] << 8);
                else
                    this->channel[c].frequency = this->registersPartII[0xA0 + c - 3] | (this->registersPartII[0xA4 + c - 3] << 8);

                for (int o=0; o < 4; o++) {
                    this->updateOperator(c, o);
                    this->updateEnvelope(c, o);
                }
            }
        }
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

        switch(d->partISelect) {
        case DAC:
            //d->status |= YM2612_BUSY;
            break;

        case KEYSTATE:
            d->channel[val & 0x7].op[0].state = val & 0x10 ? OP_STATE_ATTACK : OP_STATE_OFF;
            d->channel[val & 0x7].op[1].state = val & 0x20 ? OP_STATE_ATTACK : OP_STATE_OFF;
            d->channel[val & 0x7].op[2].state = val & 0x40 ? OP_STATE_ATTACK : OP_STATE_OFF;
            d->channel[val & 0x7].op[3].state = val & 0x80 ? OP_STATE_ATTACK : OP_STATE_OFF;
            break;
        }

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

    while (d->currentCycles <= -344) {
        int samplePos = d->bufferPos * 2;
        float sample = 0;

        d->updateChannels();
        d->modulateOperators();

        for (int c=0; c < 6; c++) {
            sample += d->channel[c].out;
        }

        // Limit signal
        if (sample > 1)
            sample = 1;

        if (sample < -1)
            sample = -1;

        d->buffer[samplePos + 0] = static_cast<qint16>(sample * 0x7FFF); // L
        d->buffer[samplePos + 1] = static_cast<qint16>(sample * 0x7FFF); // R

        d->bufferPos+=1;
        if (d->bufferPos >= d->audioSpec.samples) {
            qDebug() << SDL_GetQueuedAudioSize(d->audioDevice);

            SDL_QueueAudio(d->audioDevice, d->buffer, d->audioSpec.size);
            d->bufferPos = 0;
        }

        //d->status &= ~YM2612_BUSY;

        d->currentCycles += 344;
    }
}
