#include "ym2612.h"

#include <QDebug>
#include <QTimer>

#include <math.h>
#include <SDL2/SDL.h>

//#define YM2612_CYCLES_PER_SAMPLE 158
#define YM2612_SINE_WAVE_SAMPLES 22050

#define PI 3.14159265

// See http://www.smspower.org/maxim/Documents/YM2612

enum YM2612Status {
    YM2612_BUSY = 0x80,
    YM2612_OVERFLOW_A = 0x02,
    YM2612_OVERFLOW_B = 0x01
};

enum YM2612TimerChannelControl {
    YM2612_LOAD_A   = 0x01,
    YM2612_LOAD_B   = 0x02,
    YM2612_ENABLE_A = 0x04,
    YM2612_ENABLE_B = 0x08,
    YM2612_RESET_A  = 0x10,
    YM2612_RESET_B  = 0x20,
    YM2612_CH3_MODE = 0xC0,
};

enum Register {
    LFO         = 0x22,
    TIMER_A_MSB = 0x24,
    TIMER_A_LSB = 0x25,
    TIMER_B     = 0x26,
    TIMER_MODE  = 0x27,
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
    double      phase;
    double      frequency;
    double      envelopeScale;
    double      envelope;
    double      feedback;
    double      out;
    double      amplitude;
    bool        am;
    quint8      ar;
    quint8      rs;
    quint8      d1r;
    quint8      d2r;
    quint8      t1l;
    quint8      rr;
    int         step;
};

struct Channel {
    Operator    op[4];
    double      frequency;
    double      out;
};

const double YM2612_OCTAVES[] = { 32.7, 65.41, 130.81, 261.63, 523.25, 1046.50, 2093.00, 4186.01 };

class YM2612Private {
public:
    quint8* registersPartI;
    quint8* registersPartII;
    quint8  status;

    quint8  partISelect;
    quint8  partIISelect;

    int     timerA;
    int     timerB;

    double  timerAState;
    double  timerBState;

    SDL_AudioDeviceID   audioDevice;
    SDL_AudioSpec       audioSpec;

    double      currentCycles;
    double      cyclesPerSample;
    qint16*     buffer;
    int         bufferPos;

    float       waveform[1024];
    Channel     channel[6];

    QTimer      frequencyDebugTimer;
    int         samplesQueued;
    double      sampleStepRate;

    double      sineWave[YM2612_SINE_WAVE_SAMPLES];

public:
    YM2612Private(YM2612* q)
        : q_ptr(q),
          partISelect(0),
          partIISelect(0),
          timerA(0),
          timerB(0),
          timerAState(0),
          timerBState(0),
          audioDevice(0),
          currentCycles(0),
          cyclesPerSample(0),
          bufferPos(0),
          samplesQueued(0)
    {
        this->registersPartI    = reinterpret_cast<quint8*>(malloc(0x100));
        this->registersPartII   = reinterpret_cast<quint8*>(malloc(0x100));

        // Reset registers
        memset(this->registersPartI, 0, 0x100);
        memset(this->registersPartII, 0, 0x100);

        // Calculate Sine Wave
        for(int i=0; i < YM2612_SINE_WAVE_SAMPLES; i++) {
            this->sineWave[i] = sin((static_cast<double>(i) / YM2612_SINE_WAVE_SAMPLES) * 360.0 * PI / 180.0);
        }

        // Open audio device
        SDL_AudioSpec spec;
        memset(&spec, 0, sizeof(SDL_AudioSpec));

        spec.freq = 44100;
        spec.format = AUDIO_S16SYS;
        spec.channels = 2;
        spec.samples = 512;

        this->audioDevice = SDL_OpenAudioDevice(nullptr, 0, &spec, &this->audioSpec, 0);

        if(this->audioDevice > 0) {
            this->buffer = reinterpret_cast<qint16*>(malloc(this->audioSpec.size));
            this->cyclesPerSample = 7600489.0 / spec.freq; // Only works for PAL
            this->sampleStepRate = this->cyclesPerSample / 7600489.0;

            qDebug() << "Audio initialized!";
            qDebug() << "Buffer Size" << this->audioSpec.size;
            qDebug() << "Frequency" << this->audioSpec.freq;
            qDebug() << "Samples" << this->audioSpec.samples;
            qDebug() << "Cycles per sample" << this->cyclesPerSample;
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

        this->frequencyDebugTimer.setInterval(1000);
        //this->frequencyDebugTimer.start();
    }

    ~YM2612Private() {
        free(this->registersPartI);
        free(this->registersPartII);

        if (this->audioDevice)
            SDL_CloseAudioDevice(this->audioDevice);
    }

    void updateOperator(int channel, int op, double fm) {
        Operator* opp = &this->channel[channel].op[op];

        opp->out = this->sineWave[static_cast<int>(floor(opp->phase * YM2612_SINE_WAVE_SAMPLES)) - 1];
        opp->out *= opp->amplitude / 127.0;
        opp->out *= opp->totalLevel / 127.0;

        opp->phase += (opp->frequency * this->sampleStepRate) + fm;

        if (opp->phase > 1 || opp->phase < 0)
            opp->phase -= floor(opp->phase);
    }

    void updateEnvelope(int channel, int op) {
        switch(this->channel[channel].op[op].state) {
        case OP_STATE_ATTACK:
            if (this->channel[channel].op[op].ar)
                this->channel[channel].op[op].amplitude += this->channel[channel].op[op].ar;
            else
                this->channel[channel].op[op].amplitude = 127;

            if (this->channel[channel].op[op].amplitude >= 127)
                this->channel[channel].op[op].state = OP_STATE_SUSTAIN;
            break;

        case OP_STATE_SUSTAIN:
            this->channel[channel].op[op].amplitude -= this->channel[channel].op[op].d1r;
            if (this->channel[channel].op[op].amplitude <= this->channel[channel].op[op].t1l) {
                this->channel[channel].op[op].state = OP_STATE_DECAY;
                this->channel[channel].op[op].amplitude = this->channel[channel].op[op].t1l;
            }
            break;

        case OP_STATE_DECAY:
            this->channel[channel].op[op].amplitude -= this->channel[channel].op[op].d2r;
            break;

        case OP_STATE_OFF:
            this->channel[channel].op[op].amplitude -= this->channel[channel].op[op].rr;
            break;
        }

        if (this->channel[channel].op[op].amplitude > 127)
            this->channel[channel].op[op].amplitude = 127;
        else if (this->channel[channel].op[op].amplitude < 0)
            this->channel[channel].op[op].amplitude = 0;

        //if (this->channel[channel].op[op].amplitude)
        //qDebug() << channel << op << this->channel[channel].op[op].state << this->channel[channel].op[op].amplitude;

        /*
        if (this->channel[channel].op[op].state != OP_STATE_OFF) {
            this->channel[channel].op[op].amplitude = (127 - this->channel[channel].op[op].totalLevel) / 127.0;
        } else {
            this->channel[channel].op[op].amplitude = 0;
        }*/
    }

    void modulateOperators() {

    }

    void updateChannels() {
        for (int c=0; c < 6; c++) {
            Channel* channel = &this->channel[c];

            if (c == 5 && (this->registersPartI[DACEN] & 0x80)) {
                channel->out = (this->registersPartI[DAC] - 128.0) / 128.0; //this->registersPartI[DAC] / 255.0 - .5;
            } else {
                const quint8* registers;

                int block;
                int offset;

                if (c < 3)
                    registers = this->registersPartI;
                else
                    registers = this->registersPartII;

                block = (registers[0xA4 + c % 3] & 0x38) >> 3;
                offset = registers[0xA0 + c % 3] | ((registers[0xA4 + c % 3] & 0x07) << 8);

                channel->frequency = offset * pow(2.0, block);
                channel->frequency *= 8000000;
                channel->frequency /= pow(2, 20);
                channel->frequency /= 144;

                //channel->frequency = YM2612_OCTAVES[block] + offset;

                /*if (this->channel[c].op[0].state != OP_STATE_OFF)
                    qDebug() << "CH" << c << "BLK" << block << "OFF" << offset << "FREQ" << this->channel[c].frequency;*/

                for (int o=0; o < 4; o++) {
                    int multiplicator = registers[0x30 + (c % 3) + o * 4] & 0x0F;

                    channel->op[o].frequency = channel->frequency * (multiplicator == 0 ? 0.5 : multiplicator);
                    channel->op[o].totalLevel = 127 - registers[0x30 + (c % 3) + o * 4] & 0x7F;
                    channel->op[o].rs = (registers[0x50 + (c % 3) + o * 4] >> 5);
                    channel->op[o].ar = (registers[0x50 + (c % 3) + o * 4] & 0x0F << 1);
                    channel->op[o].d1r = (registers[0x60 + (c % 3) + o * 4] & 0x0F << 1);
                    channel->op[o].d2r = (registers[0x70 + (c % 3) + o * 4] & 0x0F << 1);
                    channel->op[o].rr  = (registers[0x80 + (c % 3) + o * 4] & 0x0F << 1) + 1;
                    channel->op[o].t1l = (registers[0x80 + (c % 3) + o * 4] & 0xF0 >> 4) * 8;

                    this->updateEnvelope(c, o);
                }

                switch(registers[0xB0 + c % 3] & 0x07) {
                case 0:
                    this->updateOperator(c, 0, 0);
                    this->updateOperator(c, 2, channel->op[0].out);
                    this->updateOperator(c, 1, channel->op[2].out);
                    this->updateOperator(c, 3, channel->op[1].out);
                    channel->out = channel->op[3].out;
                    break;

                case 1:
                    this->updateOperator(c, 0, 0);
                    this->updateOperator(c, 2, 0);
                    this->updateOperator(c, 1, channel->op[0].out +channel->op[2].out);
                    this->updateOperator(c, 3, channel->op[1].out);
                    channel->out = channel->op[3].out;
                    break;

                case 2:
                    this->updateOperator(c, 0, 0);
                    this->updateOperator(c, 2, 0);
                    this->updateOperator(c, 1, channel->op[2].out);
                    this->updateOperator(c, 3, channel->op[0].out + channel->op[1].out);
                    channel->out = channel->op[3].out;
                    break;

                case 3:
                    this->updateOperator(c, 0, 0);
                    this->updateOperator(c, 2, channel->op[0].out);
                    this->updateOperator(c, 1, 0);
                    this->updateOperator(c, 3, channel->op[2].out + channel->op[1].out);
                    channel->out = channel->op[3].out;
                    break;

                case 4:
                    this->updateOperator(c, 0, 0);
                    this->updateOperator(c, 2, channel->op[0].out);
                    this->updateOperator(c, 1, 0);
                    this->updateOperator(c, 3, channel->op[1].out);
                    channel->out = channel->op[2].out + channel->op[3].out;
                    break;

                case 5:
                    this->updateOperator(c, 0, 0);
                    this->updateOperator(c, 2, channel->op[0].out);
                    this->updateOperator(c, 1, channel->op[0].out);
                    this->updateOperator(c, 3, channel->op[0].out);
                    channel->out = channel->op[1].out + channel->op[2].out + channel->op[3].out;
                    break;

                case 6:
                    this->updateOperator(c, 0, 0);
                    this->updateOperator(c, 2, 0);
                    this->updateOperator(c, 1, channel->op[0].out);
                    this->updateOperator(c, 3, 0);
                    channel->out = channel->op[1].out + channel->op[2].out + channel->op[3].out;
                    break;

                case 7:
                    this->updateOperator(c, 0, 0);
                    this->updateOperator(c, 2, 0);
                    this->updateOperator(c, 1, 0);
                    this->updateOperator(c, 3, 0);
                    channel->out = channel->op[0].out + channel->op[1].out + channel->op[2].out + channel->op[3].out;
                    break;
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
    Q_D(YM2612);

    QObject::connect(&d->frequencyDebugTimer, &QTimer::timeout, this, &YM2612::reportSampleFrequency);
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

    /*if (address == 0x4001 && d->partISelect == DAC) {
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
        case TIMER_MODE:
            if (val & YM2612_LOAD_A) {
                d->timerA = 18 * (1024 - ((d->registersPartI[TIMER_A_MSB] << 2) | d->registersPartI[TIMER_A_LSB]));
                d->timerAState = floor(d->timerA * 136.8);
            } else {
                d->timerA = 0;
                d->timerAState = 0;
            }

            if (val & YM2612_LOAD_B) {
                d->timerB = 288 * (256 - d->registersPartI[TIMER_B]);
                d->timerBState = floor(d->timerB * 136.8);
            } else {
                d->timerB = 0;
                d->timerBState = 0;
            }

            if (val & YM2612_RESET_A) {
                d->status &= ~YM2612_OVERFLOW_A;
            }

            if (val & YM2612_RESET_B)
                d->status &= ~YM2612_OVERFLOW_B;

            break;

        case DAC:
            d->status |= YM2612_BUSY;
            break;

        case DACEN:
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

    default:
        qWarning() << "YM2612 WR at invalid address" <<  QString::number(address, 16).rightJustified(6, '0');
        break;
    }

    return NO_ERROR;
}

void YM2612::clock(int cycles) {
    Q_D(YM2612);

    d->currentCycles += cycles;

    while (d->currentCycles > 0) {
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

        d->samplesQueued++;

        d->bufferPos+=1;
        if (d->bufferPos >= d->audioSpec.samples) {
            //qDebug() << SDL_GetQueuedAudioSize(d->audioDevice) << d->audioSpec.size;

            // Just Queue Audio if the Buffer consumed. Else, we just drop the audio
            if (SDL_GetQueuedAudioSize(d->audioDevice) < (d->audioSpec.size * 3))
                SDL_QueueAudio(d->audioDevice, d->buffer, d->audioSpec.size);

            d->bufferPos = 0;
        }

        if (d->timerAState > 0) {
            d->timerAState -= d->cyclesPerSample;
            if (d->timerAState <= 0) {
                if (d->registersPartI[TIMER_MODE] & YM2612_ENABLE_A)
                    d->status |= YM2612_OVERFLOW_A;

                d->timerAState = floor(d->timerA * 136.8);
            }
        }

        if (d->timerBState > 0) {
            d->timerBState -= d->cyclesPerSample;
            if (d->timerBState <= 0) {
                if (d->registersPartI[TIMER_MODE] & YM2612_ENABLE_B)
                    d->status |= YM2612_OVERFLOW_B;

                d->timerBState = floor(d->timerB * 136.8);
                //qDebug() << "YM2612 TIMER B OVERFLOW";
            }
        }

        d->status &= ~YM2612_BUSY;

        d->currentCycles -= d->cyclesPerSample;
    }
}

void YM2612::reportSampleFrequency() {
    Q_D(YM2612);

    qDebug() << "YM2612 FREQUENCY" << d->samplesQueued;
    d->samplesQueued = 0;
}
