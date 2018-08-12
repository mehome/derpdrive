#include "vdp.h"
#include "z80.h"
#include "motorola68000.h"

#include <QDebug>
#include <QPainter>
#include <QtEndian>
#include <signal.h>

#define VRAM_SIZE 0xFFFF
#define CRAM_SIZE 0x80
#define VSRAM_SIZE 0x50

enum ScreenMode {
    NTSC,
    PAL
};

enum Register {
    ModeRegister1     = 0x00,
    ModeRegister2     = 0x01,
    PlaneANameTable   = 0x02,
    WindowNameTable   = 0x03,
    PlaneBNameTable   = 0x04,
    SpriteTable       = 0x05,
    SpritePatternGenerator = 0x06,
    BackgroundColor   = 0x07,
    HorizontalInterruptCounter = 0x0A,
    ModeRegister3     = 0x0B,
    ModeRegister4     = 0x0C,
    HScrollData       = 0x0D,
    NameTablePatternGenerator = 0x0E,
    AutoIncrementValue = 0x0F,
    PlaneSize         = 0x10,
    WindowPlaneHPos   = 0x11,
    WindowPlaneVPos   = 0x12,
    DMALength0        = 0x13,
    DMALength1        = 0x14,
    DMASource0        = 0x15,
    DMASource1        = 0x16,
    DMASource2        = 0x17,
};

enum ModeRegister1 {
    MODE1_L     = 0x20,
    MODE1_IE1   = 0x10,
    MODE1_INVAL = 0x08,
    MODE1_PALSEL= 0x04,
    MODE1_M3    = 0x02,
    MODE1_DD    = 0x01,
};

enum ModeRegister2 {
    MODE2_VRAM  = 0x80,
    MODE2_DE    = 0x40,
    MODE2_IE0   = 0x20,
    MODE2_M1    = 0x10,
    MODE2_M2    = 0x08,
    MODE2_M5    = 0x04,
};

enum ModeRegister3 {
    MODE3_IE2   = 0x8,
    MODE3_VS    = 0x4,
    MODE3_HS    = 0x3,
    MODE3_HS1   = 0x2,
    MODE3_HS2   = 0x1,
};

enum ModeRegister4 {
    MODE4_RS0   = 0x80,
    MODE4_VSY   = 0x40,
    MODE4_HSY   = 0x20,
    MODE4_SPR   = 0x10,
    MODE4_SHI   = 0x08,
    MODE4_LSM   = 0x06,
    MODE4_LSM1  = 0x04,
    MODE4_LSM0  = 0x02,
    MODE4_RS1   = 0x01,
};

enum VDPCommand {
    NONE = 0,
    PENDING = 1,

    WRITE_REGISTER,
    VRAM_READ ,
    VRAM_WRITE,
    CRAM_WRITE ,
    VSRAM_READ,
    VSRAM_WRITE,
    CRAM_READ,
};

enum Plane {
    PLANEA,
    PLANEB,
    WINDOWPLANE,
};

struct SpriteCache {
    QRect   position;
    int     entry;

    // Raw sprite data
    quint16 x;
    quint16 y;
    quint8  w;
    quint8  h;
    quint8  link;
    quint8  palette;
    quint16 pattern;

    bool    priority;
    bool    vFlip;
    bool    hFlip;
};

const quint8 MasterSystemLuminance[]    = { 0x00, 0x55, 0xAA, 0xFF };
const quint8 GenesisLuminance[]         = { 0x00, 0x34, 0x57, 0x74, 0x90, 0xAC, 0xCE, 0xFF };

class VDPPrivate {
public:
    SDL_Renderer*     renderer;

    quint8*             cram;
    quint8*             vram;
    quint8*             vsram;

    Motorola68000* cpu;
    Z80*        z80;
    MemoryBus*  bus;

    quint8      registerData[25];
    quint8      selectedRegister;
    quint16     addressRegister;

    bool fifoEmpty;
    bool fifoFull;
    bool vertialInterruptPending;
    bool spriteOverflow;
    bool spriteCollision;
    bool oddFrame;
    bool vBlank;
    bool hBlank;
    bool dmaActive;
    bool writePending;

    bool displayActive;

    bool planeAEnabled;
    bool planeBEnabled;
    bool windowPlaneEnabled;
    bool spritesEnabled;

    quint8 command0;
    quint8 command1;
    int   commandCount;

    quint16 dataWord;

    int   command;
    quint8 commandByte;
    int   commandData;

    int   screenScanlines;
    int   screenWidth;
    int   horizontalInterruptCount;

    int   beamH;
    int   beamV;

    int   currentCycles;

    quint16 dmaLength;
    quint32 dmaSource;
    quint8  dmaType;
    bool    dmaDataWait;
    bool    dmaStarted;
    bool    dmaFinishedDbg;
    quint16 dmaFillWord;

    ScreenMode screenMode;

    int displayWidth;
    int displayHeight;
    int overscanWidth;
    int overscanHeight;

    int planeWidth;
    int planeHeight;

    int planeAScrollX;
    int planeAScrollY;
    int planeBScrollX;
    int planeBScrollY;

    QVector<QVector<quint32>>   colorCache;
    QVector<qint16>             scanlineScrollA;
    QVector<qint16>             scanlineScrollB;
    QList<SpriteCache>          spriteCache;

    QImage          spriteBuffer;
    char*           frameBuffer;
    quint32*        scanLine;

    SDL_Texture*    frame;

public:
    explicit VDPPrivate(VDP* q, SDL_Renderer* renderer)
        : q_ptr(q),
          renderer(renderer),
          displayActive(false),
          planeAEnabled(true),
          planeBEnabled(true),
          windowPlaneEnabled(true),
          spritesEnabled(true),
          currentCycles(0),
          dmaLength(0),
          dmaSource(0),
          dmaType(0),
          dmaDataWait(false),
          dmaStarted(false),
          dmaFinishedDbg(false),
          dmaFillWord(0),
          screenMode(PAL),
          overscanWidth(423),
          overscanHeight(312),
          planeAScrollX(0),
          planeAScrollY(0),
          planeBScrollX(0),
          planeBScrollY(0)
    {
        this->vram  = static_cast<quint8*>(malloc(VRAM_SIZE));
        this->vsram = static_cast<quint8*>(malloc(VSRAM_SIZE));
        this->cram  = static_cast<quint8*>(malloc(CRAM_SIZE));

        this->frameBuffer       = static_cast<char*>(malloc(sizeof(quint32) * 512 * 512));
        memset(this->frameBuffer, 0, sizeof(quint32) * 512 * 512);

        this->spriteBuffer = QImage(512, 512, QImage::Format_ARGB32);

        this->frame             = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,     512,    512);
        SDL_SetTextureBlendMode(this->frame,            SDL_BLENDMODE_BLEND);

        this->colorCache.resize(4);
        this->scanlineScrollA.resize(256);
        this->scanlineScrollB.resize(256);

        /*this->frame = QImage(320, 240, QImage::Format_ARGB32);
        this->frame.fill(0x01000000);

        this->planeA = this->frame;
        this->planeB = this->frame;
        this->windowPlane = this->frame;
        this->spritePlane = this->frame;*/
    }

    void updateColorCache() {
        for (int r=0; r < 4; r++) {
            this->colorCache[r].resize(16);

            for (int c=0; c < 16; c++) {
                this->colorCache[r][c] = this->readColor(r, c);
            }
        }
    }

    void updateSpriteCache() {
        int spriteCount = 0;

        quint16 spriteTableAddress = static_cast<quint16>(this->registerData[SpriteTable] << 9);
        quint16 currentEntry = spriteTableAddress;

        this->spriteBuffer.fill(0);
        this->spriteCache.clear();

        // Read Sprite Table
        while(currentEntry && spriteCount < 80) {
            const quint16* entry = reinterpret_cast<const quint16*>((this->vram + currentEntry));

            quint16 posY = qFromBigEndian(entry[0]) & 0x3FF;
            quint16 posX = qFromBigEndian(entry[3]) & 0x1FF;
            quint8 w = ((qFromBigEndian(entry[1]) & 0x0C00) >> 10) + 1;
            quint8 h = ((qFromBigEndian(entry[1]) & 0x0300) >> 8) + 1;
            bool hFlip = qFromBigEndian(entry[2]) & 0x800;
            bool vFlip = qFromBigEndian(entry[2]) & 0x1000;
            quint16 pattern = (qFromBigEndian(entry[2]) & 0x7FF) << 5;
            quint8 palette = (qFromBigEndian(entry[2]) & 0x6000) >> 13;
            quint8 link = qFromBigEndian(entry[1]) & 0x7F;
            bool priority = qFromBigEndian(entry[2]) & 0x8000;

            /*qDebug() << QString::number(qFromBigEndian(entry[0]), 16).rightJustified(8, '0');
            qDebug() << QString::number(qFromBigEndian(entry[1]), 16).rightJustified(8, '0');
            qDebug() << QString::number(qFromBigEndian(entry[2]), 16).rightJustified(8, '0');
            qDebug() << QString::number(qFromBigEndian(entry[3]), 16).rightJustified(8, '0');*/

            this->spriteCache << (SpriteCache){
                QRect(posX, posY, w * 8, h * 8),
                (currentEntry - spriteTableAddress) / 8,
                posX,
                posY,
                w,
                h,
                link,
                palette,
                pattern,
                priority,
                vFlip,
                hFlip
            };

            spriteCount++;

            if (link)
                currentEntry = spriteTableAddress + (link * 8);
            else
                currentEntry = 0;
        }

        // Mask Sprites
        /*QMutableListIterator<SpriteCache> iSprite(this->spriteCache);
        while(iSprite.hasNext()) {
            SpriteCache& sprite = iSprite.next();
            if (sprite.priority && sprite.position.x() == 0) {
                QMutableListIterator<SpriteCache> iMaskSprite(this->spriteCache);

                while(iMaskSprite.hasNext()) {
                    SpriteCache& maskSprite = iMaskSprite.next();

                    if ( (maskSprite.entry != sprite.entry) &&
                         (!maskSprite.priority) &&
                         (maskSprite.position.top() >= sprite.position.top()) &&
                         (maskSprite.position.bottom() <= sprite.position.bottom())){

                        iMaskSprite.remove();
                    }
                }
            }
        }*/

        // Sort by Priority
        qSort(this->spriteCache.begin(), this->spriteCache.end(), [](const SpriteCache& a, const SpriteCache& b) {
            return (a.position.top() < b.position.top()) && (a.position.left() < b.position.left()) && (!a.priority && b.priority);
        });

        // Draw Sprites
        foreach (const SpriteCache& cache, this->spriteCache) {
            int posX = cache.x - 128;
            int posY = cache.y - 128;

            quint16 pattern = cache.pattern;

            for (int x = 0; x < cache.w; x++) {
                for (int y = 0; y < cache.h; y++) {
                    int spritePosX = 0;
                    int spritePosY = 0;

                    if (cache.hFlip)
                        spritePosX = ((cache.w -1) - x) * 8;
                    else
                        spritePosX = x * 8;

                    if (cache.vFlip)
                        spritePosY = ((cache.h -1) - y) * 8;
                    else
                        spritePosY = y * 8;

                    this->blitPattern(&this->spriteBuffer, pattern, cache.palette, cache.priority, cache.hFlip, cache.vFlip, posX + spritePosX, posY + spritePosY);
                    pattern += 0x20;
                }
            }
        }
    }

    void handleCommand(bool force = false) {
        this->dmaDataWait = false;

        // Check if we have received all command bytes
        if (this->commandCount < 2 || (force && !this->writePending))
            return;

        this->commandCount = 0;

        //Check for write register command
        if ((this->command0 & 0xC0) == 0x80) {
            this->selectedRegister = this->command0 & 0x1F;
            if (this->selectedRegister > 23)
                return;

            //qDebug() << "VDP Set" << QString::number(this->selectedRegister, 16) << "=" << QString::number(this->command1, 16).rightJustified(2, '0');

            this->registerData[this->selectedRegister] = this->command1;
        } else {
            //qDebug() << "VDP CMD" << QString::number(this->command0, 16) << QString::number(this->command1, 16);

            if (!this->writePending) {
                this->dmaActive = false;

                this->commandByte &= ~0x03;
                this->commandByte |= (this->command0 & 0xC0) >> 6;

                this->commandData &= ~0x3FFF;
                this->commandData |=
                        ((this->command0 & 0x3F) << 8) |
                        this->command1;

                this->writePending = true;
                this->command = PENDING;
            } else {
                this->commandByte &= ~0x3C;
                this->commandByte |= (this->command1 & 0xF0) >> 2;

                this->commandData &= ~0xC000;
                this->commandData |= (this->command1 & 0x3) << 14;

                this->writePending = false;

                this->prepareMemoryTransfer();
                //qDebug() << "VDP COMMAND" << QString::number(this->command0, 16) << QString::number(this->command1, 16);
            }
        }
    }

    void prepareMemoryTransfer() {
        if (this->command != PENDING || this->dmaActive || this->dmaDataWait)
            return;

        //qDebug() << "VDP CMD BYTE" << QString::number(this->commandByte, 16).rightJustified(2, '0');

        switch (this->commandByte & 0x0F) {
        case 0x0:
            //qDebug() << "VDP VRAM READ" << this->commandData;
            this->command = VRAM_READ;
            break;

        case 0x1:
            //qDebug() << "VDP VRAM WRITE" << this->commandData;
            this->command = VRAM_WRITE;
            break;

        case 0x3:
            //qDebug() << "VDP CRAM WRITE" << this->commandData;
            this->command = CRAM_WRITE;
            break;

        case 0x4:
            //qDebug() << "VDP VSRAM READ" << this->commandData;
            this->command = VSRAM_READ;
            break;

        case 0x5:
            //qDebug() << "VDP VSRAM WRITE" << this->commandData;
            this->command = VSRAM_WRITE;
            break;

        case 0x8:
            //qDebug() << "VDP CRAM READ" << this->commandData;
            this->command = CRAM_READ;
            break;

        default:
            this->command = NONE;
            break;
        }

        if (this->command != NONE) {
            if (this->registerData[ModeRegister2] & MODE2_M1)
                this->dmaActive = this->commandByte & 0x20;
            else
                this->dmaActive = false;

            this->dmaLength = static_cast<quint16>(
                        (this->registerData[DMALength0]) |
                        ((this->registerData[DMALength1]) << 8)
                        );

            /*qDebug() << "DMA SRC REG"
                     << QString::number(this->registerData[DMASource2], 16).rightJustified(2, '0')
                     << QString::number(this->registerData[DMASource1], 16).rightJustified(2, '0')
                     << QString::number(this->registerData[DMASource0], 16).rightJustified(2, '0');*/

            this->dmaSource = static_cast<quint32>(
                        (this->registerData[DMASource0]) |
                        ((this->registerData[DMASource1]) << 8) |
                        ((this->registerData[DMASource2] & 0x3F) << 16)
                        );

            //qDebug() << "DMA SRC" << QString::number(this->dmaSource, 16).rightJustified(6, '0', false);

            if (this->dmaActive)
                this->dmaType   = ((this->registerData[DMASource2] & 0xC0) >> 6);
            else
                this->dmaType   = 0;

            this->addressRegister = static_cast<quint16>(this->commandData);

            if (!(this->dmaType & 0x2)) {
                this->dmaSource |= static_cast<quint32>((this->registerData[DMASource2] & 0x40) << 16);
                this->dmaSource <<= 1;
                this->dmaType = 0;
            }

            if (this->command == VRAM_WRITE || this->command == VSRAM_WRITE || this->command == CRAM_WRITE) {
                if (!this->dmaActive || this->dmaType == 0x2) {
                    this->dmaDataWait = true;
                }
            }

            if ((this->command == CRAM_READ || this->command == CRAM_WRITE) && this->dmaLength > (64 * 2))
                this->dmaLength = (64 * 2);

            /*qDebug() << "DMA "
                     << "CMD:" << QString::number(this->commandByte, 16).rightJustified(2, '0')
                     << "Len:" << QString::number(this->dmaLength, 16).rightJustified(4, '0')
                     << "Src:" << QString::number(this->dmaSource, 16).rightJustified(6, '0')
                     << "Dst:" << QString::number(this->addressRegister, 16).rightJustified(4, '0')
                     << "Type:" << QString::number(this->dmaType, 16).rightJustified(2, '0')
                     << "DMA:" << this->dmaActive
                     << "DATA WAIT:" << this->dmaDataWait;*/

            if ( this->dmaType == 0 &&
                 this->dmaActive &&
                (this->command == VRAM_WRITE || this->command == CRAM_WRITE || this->command == VSRAM_WRITE) &&
                 !this->dmaDataWait) {//(this->registerData[ModeRegister2] & MODE2_M1) && this->cpu->programCounter() <= 0x03FFFFF && !this->dmaDataWait) {
                //qDebug() << "DISABLE MOTOROLA";
                this->cpu->setDisabled(true);
            }
        }
    }

    void performDirectWrite(quint16 data) {
        if (this->dmaType == 0x2) {
            //qDebug() << "Fill DWORD" << QString::number(data, 16).rightJustified(4, '0');

            this->dmaFillWord = data;
            this->dmaDataWait = false;
        } else {
            /*qDebug() << "Direct write"
                     << QString::number(data, 16).rightJustified(4,  '0')
                     << QString::number(this->addressRegister, 16).rightJustified(4, '0');*/

            switch (this->command) {
            case CRAM_WRITE:
                this->cram[(this->addressRegister & 0x7F)] = static_cast<char>(data & 0x00FF);
                this->cram[(this->addressRegister + 1) & 0x7F] = static_cast<char>((data & 0xFF00) >> 8);
                this->addressRegister += this->registerData[AutoIncrementValue];
                break;

            case VRAM_WRITE:
                this->vram[this->addressRegister & 0xFFFF] = static_cast<char>(data & 0x00FF);
                this->vram[(this->addressRegister + 1) & 0xFFFF] = static_cast<char>((data & 0xFF00) >> 8);
                this->addressRegister += this->registerData[AutoIncrementValue];
                break;

            case VSRAM_WRITE:
                this->addressRegister &= 0x7F;

                if(this->addressRegister < 0x4F) {
                    this->vsram[this->addressRegister] = static_cast<char>(data & 0x00FF);
                    this->vsram[this->addressRegister + 1] = static_cast<char>((data & 0xFF00) >> 8);
                }

                this->addressRegister += this->registerData[AutoIncrementValue];
                break;


            default:
                break;
            }
        }
    }

    void drawFrame() {

    }

    inline quint32 readColor(quint8 index) const {
        return this->readColor((index & 0x30) >> 4, (index & 0xF));
    }

    inline quint32 readColor(int row, int cell) const {
        int address = (row * 16 + cell) << 1;
        quint8 b0, b1;

        b0 = this->cram[address];
        b1 = this->cram[address + 1];

        return this->decodeColor(b0, b1) & (cell == 0 ? 0x00FFFFFF : 0xFFFFFFFF);
    }

    inline quint32 decodeColor(quint8 b0, quint8 b1) const {
        quint32 b, g, r;

        if (this->registerData[ModeRegister1] & MODE1_PALSEL) {
            b = GenesisLuminance[(b0 & 0x0E) >> 1];
            g = GenesisLuminance[(b1 & 0xE0) >> 5];
            r = GenesisLuminance[(b1 & 0x0E) >> 1];
        } else {
            b = MasterSystemLuminance[(b0 & 0x02) >> 1];
            g = MasterSystemLuminance[(b1 & 0x20) >> 5];
            r = MasterSystemLuminance[(b1 & 0x02) >> 1];
        }

        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    qint16 decodeDisplayScroll(quint16 val) const {
        val = qFromBigEndian(val);

        return static_cast<qint16>((val & 0x1FF) | (val & 0x200 ? 0x8000 : 0));
    }

    void blitPattern(QImage* buffer, quint16 address, int palette, bool priority, bool flipH, bool flipV, int dstX, int dstY, bool debug = false) const {
        address &= 0xFFFF;

        const quint8* pattern = reinterpret_cast<const quint8*>(this->vram + address);

        for (int y=0; y < 8; y++) {
            int pY = flipV ? 7 - y : y;
            int tY = dstY + y;

            if (tY < 0 || tY >= buffer->height())
                continue;

            quint32* scanline = reinterpret_cast<quint32*>(buffer->scanLine(tY));

            for (int x=0; x < 8; x++) {
                int pX = flipH ? 7 - x : x;
                int tX = dstX + x;

                if (tX < 0 || tX >= buffer->width())
                    continue;

                quint32 color;
                if (pX & 0x1)
                    color = this->colorCache[palette][pattern[(pY * 4) + (pX / 2)] & 0x0F];
                else
                    color = this->colorCache[palette][(pattern[(pY * 4) + (pX / 2)] & 0xF0) >> 4];

                if (!(color & 0xFF000000))
                    continue;

                if (debug) {
                    scanline[tX] = color;
                } else {
                    color &= 0x00FFFFFF;

                    if (scanline[tX] & 0xFF000000) {
                        if (priority)
                            scanline[tX] = color | 0xB0000000; // Set priority and collision bit
                        else
                            scanline[tX] |= 0x20000000; // Set collision bit
                    } else {
                        scanline[tX] = color | (priority ? 0x90000000 : 0x10000000);
                    }
                }
            }
        }
    }

    inline void tracePlane(int plane, int x, int y, quint32* color, bool* priority) {
        int baseAddress = 0;
        int planePixelW, planePixelH;
        int tileX, tileY, subtileX, subtileY;

        int patternW, patternH, factor;
        patternW = 8;

        if (this->registerData[ModeRegister4] & MODE4_LSM) {
            patternH = 16;
            factor = 2;
        } else {
            patternH = 8;
            factor = 1;
        }

        switch (plane) {
        case PLANEA:
            baseAddress = (this->registerData[PlaneANameTable] & 0x38) << 10;
            break;

        case PLANEB:
            baseAddress = (this->registerData[PlaneBNameTable] & 0x7) << 13;
            break;

        default:
            return;
        }

        // Compute tile coordinates
        planePixelW = this->planeWidth * patternW;
        planePixelH = this->planeHeight * patternH;

        subtileX = x % planePixelW;
        subtileY = y % planePixelH;

        if (subtileX < 0)
            subtileX += planePixelW;

        if (subtileY < 0)
            subtileY += planePixelH;

        tileX = subtileX / patternW;
        tileY = subtileY / patternH;

        subtileX %= patternW;
        subtileY %= patternH;


        // Read tile
        const quint16* nametable = reinterpret_cast<const quint16*>(this->vram + baseAddress);
        quint16 entry = qFromBigEndian(nametable[tileY * this->planeWidth + tileX]);

        quint16 artIndex = (entry & 0x7FF) << 5;
        quint8  palette = (entry & 0x6000) >> 13;
        *priority = !!(entry & 0x8000);

        // Horizontal Flip
        if (entry & 0x800)
            subtileX = (patternW - 1) - subtileX;

        // Vertical Flip
        if (entry & 0x1000)
            subtileY = (patternH - 1) - subtileY;

        // Divide by interlace factor
        subtileY /= factor;

        // Read pattern
        const quint8* pattern = reinterpret_cast<const quint8*>(this->vram + (artIndex & 0xFFF8));

        if (subtileX % 2)
            *color = this->colorCache[palette][(pattern[subtileY * 4 + subtileX / 2] & 0x0F)];
        else
            *color = this->colorCache[palette][(pattern[subtileY * 4 + subtileX / 2] & 0xF0) >> 4];
    }

    inline void traceSprites(int x, int y, quint32* color, bool* priority) {
        const quint32* spritePixels = reinterpret_cast<const quint32*>(this->spriteBuffer.constBits());
        quint32 bufferPixel = spritePixels[y * 512 + x];

        if (bufferPixel & 0xFF000000) {
            *color = bufferPixel | 0xFF000000;
            *priority = (bufferPixel & 0x80000000);
        }
    }

    inline quint32 tracePixel(int x, int y) {

        if (!this->displayActive)
            return 0xFF000000;
        else {
            quint32 bgColor = this->colorCache[(this->registerData[BackgroundColor] & 0x30) >> 4]
                                              [this->registerData[BackgroundColor] & 0xF] | 0xFF000000;

            quint32 mixColor = bgColor;
            quint32 planeAColor = 0;
            quint32 planeBColor = 0;
            quint32 spriteColor = 0;

            bool planeAPriority = false;
            bool planeBPriority = false;
            bool spritePriority = false;

            int planeAX = x, planeAY = y;
            int planeBX = x, planeBY = y;

            const quint16* hScrollData = reinterpret_cast<const quint16*>(this->vram + ((this->registerData[HScrollData] & 0x3F) << 10));
            const quint16* vScrollData = reinterpret_cast<const quint16*>(this->vsram);

            switch (this->registerData[ModeRegister3] & MODE3_HS) {
            case 0x00:
                planeAX = x - this->planeAScrollX;
                planeBX = x - this->planeBScrollX;
                break;

            case 0x02:
                planeAX = x - this->decodeDisplayScroll(hScrollData[(y/8) * 2 ]);
                planeBX = x - this->decodeDisplayScroll(hScrollData[(y/8) * 2 + 1]);
                break;

            case 0x03:
                planeAX = x - this->decodeDisplayScroll(hScrollData[y * 2]);
                planeBX = x - this->decodeDisplayScroll(hScrollData[y * 2 + 1]);
                break;
            }

            switch (this->registerData[ModeRegister3] & MODE3_VS) {
            case 0x00:
                planeAY = y - this->planeAScrollY;
                planeBY = y - this->planeBScrollY;
                break;

            case 0x01:
                planeAY = y - this->decodeDisplayScroll(vScrollData[x/16]);
                planeAY = y - this->decodeDisplayScroll(vScrollData[x/16]);
            }

            if (this->planeAEnabled)
                this->tracePlane(PLANEA, planeAX, planeAY, &planeAColor, &planeAPriority);

            if (this->planeBEnabled)
                this->tracePlane(PLANEB, planeBX, planeBY, &planeBColor, &planeBPriority);

            if (this->spritesEnabled)
                this->traceSprites(x, y, &spriteColor, &spritePriority);

            if ((planeBColor & 0xFF000000) && !planeBPriority)
                mixColor = planeBColor;

            if ((planeAColor & 0xFF000000) && !planeAPriority)
                mixColor = planeAColor;

            if ((spriteColor & 0xFF000000) && !spritePriority)
                mixColor = spriteColor;

            if ((planeBColor & 0xFF000000) && planeBPriority)
                mixColor = planeBColor;

            if ((planeAColor & 0xFF000000) && planeAPriority)
                mixColor = planeAColor;

            if ((spriteColor & 0xFF000000) && spritePriority)
                mixColor = spriteColor;

            return mixColor;
        }
    }

private:
    VDP* q_ptr;
    Q_DECLARE_PUBLIC(VDP)
};

VDP::VDP(SDL_Renderer *renderer, QObject *parent)
    : QObject(parent),
      Device(),
      d_ptr(new VDPPrivate(this, renderer))
{
    this->reset();
}

VDP::~VDP()
{
    delete d_ptr;
}

void VDP::attachBus(MemoryBus* bus)
{
    Q_D(VDP);

    d->bus = bus;
}

MemoryBus* VDP::bus() const
{
    Q_D(const VDP);

    return d->bus;
}

int VDP::clock(int cycles)
{
    Q_D(VDP);

    d->currentCycles -= cycles;

    while(d->currentCycles < -4) {

        //} else {
        //   d->cpu->setDisabled(false);
        //}

        // Check if display is enabled
        //if (d->registerData[ModeRegister1] & MODE1_DE) {
        if (d->beamV == 0 && d->beamH == 0) {
            if (this->interruptPending())
                this->clearInterrupt();

            if (d->dmaFinishedDbg) {
                emit this->dmaFinished();
                d->dmaFinishedDbg = false;
            }

            d->vertialInterruptPending = false;
            d->horizontalInterruptCount = d->registerData[HorizontalInterruptCounter];
             //d->readColor(d->registerData[BackgroundColor]));

            SDL_UpdateTexture(d->frame, nullptr, d->frameBuffer, sizeof(quint32) * 512);
            emit this->frameUpdated(d->frame);

            memset(d->frameBuffer, 0, sizeof(quint32) * 512 * 512);

            //qDebug() << "Frame Start";

            if(d->registerData[ModeRegister2] & MODE2_M2)
                d->screenScanlines = 30 * 8;
            else
                d->screenScanlines = 28 * 8;

            if(d->registerData[ModeRegister4] & (MODE4_RS0 | MODE4_RS1))
                d->screenWidth = 40 * 8;
            else
                d->screenWidth = 32 * 8;

            d->beamH = 0;

            // ======== Update internal registers ========

            d->updateColorCache();

            // Screen resolution
            d->screenScanlines  = (d->registerData[ModeRegister2] & MODE2_M2) ? 240 : 224;
            d->screenWidth      = ((d->registerData[ModeRegister4] & MODE4_RS0) && (d->registerData[ModeRegister4] & MODE4_RS1)) ? 320 : 256;

            // Background size
            switch ((d->registerData[PlaneSize] & 0x30) >> 4) {
            case 0x02:
            case 0x00: d->planeHeight = 32; break;
            case 0x01: d->planeHeight = 64; break;
            case 0x03: d->planeHeight = 128; break;
            }

            switch ((d->registerData[PlaneSize] & 0x03)) {
            case 0x02:
            case 0x00: d->planeWidth = 32; break;
            case 0x01: d->planeWidth = 64; break;
            case 0x03: d->planeWidth = 128; break;
            }

            // Sprites
            d->updateSpriteCache();

            // Scrolling
            d->scanlineScrollA.fill(0);
            d->scanlineScrollB.fill(0);

            d->planeAScrollY = 0;
            d->planeBScrollY = 0;

            d->planeAScrollX = 0;
            d->planeBScrollX = 0;

            if (!(d->registerData[ModeRegister3] & MODE3_VS)) {
                const quint16* scrollData = reinterpret_cast<const quint16*>(d->vsram);

                d->planeAScrollY = d->decodeDisplayScroll(scrollData[0]) * -1;
                d->planeBScrollY = d->decodeDisplayScroll(scrollData[1]) * -1;
            }

            if(!(d->registerData[ModeRegister3] & MODE3_HS)) {
                const quint16* scrollData = reinterpret_cast<const quint16*>(d->vram + ((d->registerData[HScrollData] & 0x3F) << 10));

                d->planeAScrollX = d->decodeDisplayScroll(scrollData[0]);
                d->planeBScrollX = d->decodeDisplayScroll(scrollData[1]);
            }
        }

        /*QRgb* scanlineData = (QRgb*)d->frame.scanLine(d->beamV);

        // Draw Background
        scanlineData[d->beamH] =  & 0x00FFFFFF;*/

        if (d->beamV == d->screenScanlines && d->beamH == d->overscanWidth) {
            if (d->registerData[ModeRegister2] & MODE2_IE0) {
                for(int i = d->screenWidth; i < 512; i++)
                    d->scanLine[i] = 0xFF880000;

                //qDebug() << "VBLANK";
                d->vertialInterruptPending = true;

                this->interruptRequest(6);
                d->currentCycles = 0;
            }

            d->z80->interrupt();
        }

        if (d->beamH == d->overscanWidth) {
            if (d->horizontalInterruptCount == 0) {
                d->horizontalInterruptCount = d->registerData[HorizontalInterruptCounter];

                if (d->registerData[ModeRegister1] & MODE1_IE1) {
                    for(int i = d->screenWidth; i < 512; i++)
                        d->scanLine[i] = 0xFF000088;

                    this->interruptRequest(4);
                    d->currentCycles = 0;
                }
            } else {
                d->horizontalInterruptCount--;
            }
        }

        if (d->beamH > d->overscanWidth) {
            d->beamH = 0;
            d->beamV++;

            d->updateColorCache();
        }

        if (d->beamH < d->screenWidth &&
            d->beamV < d->screenScanlines &&
            (d->registerData[ModeRegister2] & MODE2_DE) &&
            (((d->registerData[ModeRegister1] & MODE1_L) && (d->beamH < 8)) || !(d->registerData[ModeRegister1] & MODE1_L)))
            d->displayActive = true;
        else
            d->displayActive = false;

        d->scanLine = reinterpret_cast<quint32*>(static_cast<char*>(d->frameBuffer) + (d->beamV * (sizeof(quint32) * 512)));
        d->scanLine[d->beamH] = d->tracePixel(d->beamH, d->beamV);

        if (d->dmaActive && !d->dmaDataWait) {
            //while(true) {
            for(int i=0; i < (d->displayActive ? 4 : 88); i++) {

                quint8* target = nullptr;
                quint16 mask = 0;

                switch(d->command) {
                case VSRAM_WRITE:
                    target = reinterpret_cast<quint8*>(d->vsram);
                    mask = 0x3F;
                    break;

                case CRAM_WRITE:
                    target = reinterpret_cast<quint8*>(d->cram);
                    mask = 0x7F;
                    break;

                case VRAM_WRITE:
                    target = reinterpret_cast<quint8*>(d->vram);
                    mask = 0xFFFF;
                    break;

                default:
                    qDebug() << "Invalid DMA Command" << d->command;
                    break;
                }

                // Only perform DMA if we have valid information
                if (target) {
                    if (    (d->command != VSRAM_WRITE) ||
                            (d->command == VSRAM_WRITE && d->addressRegister <= 0x3F) ||
                            (d->command == CRAM_WRITE && d->addressRegister <= 0x7F)) {

                        d->addressRegister &= mask;

                        if (d->dmaType == 0) { // DMA WRITE
                            quint8 b0, b1;

                            if (d->dmaSource > 0x00FFFFFF) {
                                d->dmaSource &= 0x0000FFFF;
                                d->dmaSource |= 0xE00000;
                            }

                            d->bus->peek(d->dmaSource,      b0);
                            d->bus->peek(d->dmaSource + 1,  b1);

                            if (d->command == VRAM_WRITE && (d->addressRegister & 0x1)) {
                                target[(d->addressRegister & 0xFFFE)]       = b1;
                                target[(d->addressRegister & 0xFFFE) + 1]   = b0;
                            } else {
                                target[d->addressRegister & 0xFFFE ]        = b0;
                                target[(d->addressRegister & 0xFFFE) + 1]   = b1;
                            }

                            if (d->command == CRAM_WRITE) {
                                d->scanLine[d->beamH] = d->decodeColor(b0, b1);
                            }

                            d->dmaSource += 2;
                        } else if (d->dmaType == 0x2) { // DMA FILL
                            if (!d->dmaStarted && d->command == VRAM_WRITE)
                                target[d->addressRegister] = d->dmaFillWord & 0xFF;
                            else if (d->command != VRAM_WRITE)
                                target[d->addressRegister] = d->dmaFillWord & 0xFF;

                            target[d->addressRegister + 1] = (d->dmaFillWord >> 8) & 0xFF;
                        } else if (d->dmaType == 0x3) { // DMA COPY
                            d->dmaSource &= mask;

                            target[d->addressRegister]       = target[d->dmaSource];
                            target[d->addressRegister + 1]   = target[d->dmaSource + 1];

                            d->dmaSource += 2;
                        }
                    } else {
                        d->dmaLength = 1;
                    }

                    d->addressRegister += d->registerData[AutoIncrementValue];
                }

                d->dmaStarted = true;

                d->dmaLength--;
                if (!d->dmaLength) {
                    d->command = NONE;
                    d->dmaActive = false;
                    d->dmaStarted = false;
                    d->dmaFinishedDbg = true;
                    d->cpu->setDisabled(false);

                    //qDebug() << "ENABLE MOTOROLA";

                    break;
                }
            }
        }

        d->beamH++;

        if (d->beamV >= d->overscanHeight) {
            d->beamV = 0;
            d->beamH = 0;
        }

        //}

        d->currentCycles += 2;
    }

    return 0;
}

void VDP::reset()
{
    Q_D(VDP);

    Device::reset();

    memset(&d->registerData, 0, 25);
    memset(d->vram,     0, VRAM_SIZE);
    memset(d->cram,     0, CRAM_SIZE);
    memset(d->vsram,    0, VSRAM_SIZE);

    d->fifoEmpty = true;
    d->fifoFull = false;
    d->vertialInterruptPending = false;
    d->spriteOverflow = false;
    d->spriteCollision = true;
    d->oddFrame = false;
    d->vBlank = false;
    d->hBlank = false;
    d->dmaActive = false;
    d->writePending = false;

    d->beamH = 0;
    d->beamV = 0;
    d->commandCount = 0;

    d->command = NONE;
    d->commandByte = 0;
    d->commandData = 0;

    d->updateColorCache();
}

void VDP::attachZ80(Z80* cpu)
{
    Q_D(VDP);

    d->z80 = cpu;
}

const QByteArray VDP::cram() const
{
    Q_D(const VDP);

    return QByteArray::fromRawData(reinterpret_cast<const char*>(d->cram), CRAM_SIZE);
}

const QByteArray VDP::vram() const
{
    Q_D(const VDP);

    return QByteArray::fromRawData(reinterpret_cast<const char*>(d->vram), VRAM_SIZE);
}

void VDP::debugCRamBlit(QImage *buffer)
{
    Q_D(const VDP);

    for (int r=0; r < 4; r++) {
        for (int c=0; c < 16; c++) {
            buffer->setPixel(c, r, d->colorCache[r][c]);
        }
    }
}

void VDP::debugBlit(QImage* buffer, quint16 address, int palette, int x, int y) const
{
    Q_D(const VDP);

    d->blitPattern(buffer, address, palette, false, false, false, x, y, true);
}

void VDP::setPlaneA(bool enabled)
{
    Q_D(VDP);

    d->planeAEnabled = enabled;
}

void VDP::setPlaneB(bool enabled)
{
    Q_D(VDP);

    d->planeBEnabled = enabled;
}

void VDP::setWindowPlane(bool enabled)
{
    Q_D(VDP);

    d->windowPlaneEnabled = enabled;
}

void VDP::setSprites(bool enabled)
{
    Q_D(VDP);

    d->spritesEnabled = enabled;
}

int VDP::peek(quint32 address, quint8& val)
{
    Q_D(VDP);

    //qDebug() << "VDP READ" << address;

    switch(address) {
    case 0x00:       
    case 0x01:
        d->commandCount = 0;
        d->writePending = false;

        quint8* target;
        int mask;

        switch(d->command) {
        case VSRAM_WRITE:
            target = d->vsram;
            mask = 0x7F;
            break;

        case CRAM_WRITE:
            target = d->cram;
            mask = 0x7F;
            break;

        case VRAM_WRITE:
            target = d->vram;
            mask = 0xFFFF;
            break;

        default:
            val = 0;
            return NO_ERROR;
        }

        val = target[(d->addressRegister & mask) + val];

        if (address == 0x1)
            d->addressRegister += d->registerData[AutoIncrementValue];

        return NO_ERROR;

    case 0x02:
        d->commandCount = 0;
        d->writePending = false;
        //d->handleCommand(true);
        //d->prepareMemoryTransfer();

        val = 0x34 |
                (d->fifoEmpty ? 0x2 : 0x0) |
                (d->fifoFull ? 0x1 : 0x0);

        return NO_ERROR;


    case 0x03:
        d->commandCount = 0;
        d->writePending = false;
        d->dmaDataWait = false;

        d->handleCommand(true);
        d->prepareMemoryTransfer();

        val = (d->vertialInterruptPending ? 0x80 : 0x00) |
                (d->spriteOverflow ? 0x40 : 0x00) |
                (d->spriteCollision ? 0x20 : 0x00) |
                (d->oddFrame ? 0x10 : 0x00) |
                (d->vBlank ? 0x08 : 0x00) |
                (d->hBlank ? 0x04 : 0x00) |
                (d->dmaActive ? 0x02 : 0x00) |
                (d->screenMode == PAL ? 0x01 : 0x00);

        return NO_ERROR;
    }

    return NO_ERROR;
}

int VDP::poke(quint32 address, quint8 val)
{
    Q_D(VDP);

    /*qDebug() << "VDP DATA WRITE"
             << QString::number(address, 16).rightJustified(4, '0')
             << "="
             << QString::number(val, 16).rightJustified(2, '0');*/

    switch(address) {
    case 0x00:
        d->dataWord &= ~0x00FF;
        d->dataWord |= val;

        if (d->dmaDataWait)
            d->prepareMemoryTransfer();

        return NO_ERROR;

    case 0x01:
        d->dataWord &= ~0xFF00;
        d->dataWord |= val << 8;

        if (d->dmaDataWait) {
            d->prepareMemoryTransfer();
            d->performDirectWrite(d->dataWord);
        /*} else {
            //qDebug() << "FILL DWORD" << d->dmaFillWord;

            d->dmaFillWord = d->dataWord;
            d->dmaDataWait = false;*/
        }
        return NO_ERROR;

    case 0x02:
        d->command0 = val;
        d->commandCount++;
        d->handleCommand();
        return NO_ERROR;

    case 0x03:
        d->command1 = val;
        d->commandCount++;
        d->handleCommand();
        return NO_ERROR;
    }
    return NO_ERROR;
}

void VDP::attachCpu(Motorola68000* cpu)
{
    Q_D(VDP);

    Device::attachCpu(cpu);

    d->cpu = cpu;
}
