#include "vdp.h"
#include "z80.h"
#include "motorola68000.h"

#include <QDebug>
#include <QPainter>
#include <QtEndian>

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
   MODE1_L     = 0x40,
   MODE1_IE1   = 0x20,
   MODE1_M3    = 0x02,
   MODE1_DE    = 0x01,
};

enum ModeRegister2 {
   MODE2_VRAM  = 0x80,
   MODE2_DE    = 0x40,
   MODE2_IE0   = 0x20,
   MODE2_M1    = 0x10,
   MODE2_M2    = 0x08,
   MODE2_M3    = 0x04,
};

enum ModeRegister3 {
   MODE3_IE2   = 0x8,
   MODE3_VS    = 0x4,
   MODE3_HS1   = 0x2,
   MODE3_HS2   = 0x1,
};

enum ModeRegister4 {
   MODE4_RS0   = 0x80,
   MODE4_VSY   = 0x40,
   MODE4_HSY   = 0x20,
   MODE4_SPR   = 0x10,
   MODE4_SHI   = 0x08,
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

class VDPPrivate {
   public:
      QByteArray        cram;
      QByteArray        vsram;
      QByteArray        vram;
      //quint16  cram[64];
      //quint8   vram[64 * 1024];

      Motorola68000* cpu;
      Z80*        z80;
      MemoryBus*  bus;

      //quint8 registerData[25];
      QByteArray  registerData;
      quint8      selectedRegister;
      quint32     addressRegister;

      bool fifoEmpty;
      bool fifoFull;
      bool vertialInterruptPending;
      bool spriteOverflow;
      bool spriteCollision;
      bool oddFrame;
      bool vBlank;
      bool hBlank;
      bool dmaActive;
      bool palMode;
      bool writePending;

      quint8 command0;
      quint8 command1;
      int   commandCount;

      quint16 dataWord;

      int   command;
      quint8 commandByte;
      int   commandData;

      int   scanlines;
      int   width;

      int   beamH;
      int   beamV;
      int   lineCounter;

      int   currentCycles;

      quint32 dmaLength;
      quint32 dmaSource;
      quint8 dmaType;

      QImage frame;
      QImage planeA;
      QImage planeB;
      QImage windowPlane;

   public:
      explicit VDPPrivate(VDP* q)
         : q_ptr(q),
           cram(QByteArray(64 * 2, 0)),
           vsram(QByteArray(40 * 2, 0)),
           vram(QByteArray(64*1024, 0)),
           registerData(QByteArray(25, 0))
      {
         this->frame = QImage(320, 240, QImage::Format_ARGB32);
         this->frame.fill(0x01000000);

         this->planeA = this->frame;
         this->planeB = this->frame;
         this->windowPlane = this->frame;
      }

      void handleCommand() {
         // Check if we have received all command bytes
         if (this->commandCount < 2)
            return;

         this->commandCount = 0;

         //Check for write register command
         if ((this->command0 & 0xC0) == 0x80) {
            this->selectedRegister = this->command0 & 0x1F;
            if (this->selectedRegister > 23)
               return;

            //qDebug() << "VDP Set" << QString::number(this->selectedRegister, 16) << "=" << QString::number(this->command1, 16);

            this->registerData[this->selectedRegister] = this->command1;
         } else {
            //qDebug() << "VDP CMD" << QString::number(this->command0, 16) << QString::number(this->command1, 16);

            if (!this->writePending) {
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

               if ((this->registerData[ModeRegister2] & MODE2_M1) && this->cpu->programCounter() <= 0x03FFFFF) {
                  qDebug() << "DISABLE MOTOROLA";
                  this->cpu->setDisabled(true);
               }

               this->prepareMemoryTransfer();
               //qDebug() << "VDP COMMAND" << QString::number(this->command0, 16) << QString::number(this->command1, 16);
            }
         }
      }

      void prepareMemoryTransfer() {
         if (this->command != PENDING || this->dmaActive)
            return;

         //qDebug() << "VDP CMD BYTE" << QString::number(this->commandByte, 16);

         this->addressRegister = this->commandData;

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
            this->dmaActive = this->registerData[ModeRegister2] & MODE2_M1;

            this->dmaLength = ((quint8)this->registerData[DMALength0]) |
                              (((quint8)this->registerData[DMALength1]) << 8);

            this->dmaSource = ((quint8)this->registerData[DMASource0]) |
                              ((quint8)this->registerData[DMASource1] << 8) |
                              ((quint8)(this->registerData[DMASource2] & 0x3F) << 16);

            this->dmaType   = ((quint8)this->registerData[DMASource2] & 0xC0) >> 6;

            this->addressRegister = this->commandData;

            if (!(this->dmaType & 0x2)) {
               this->dmaSource |= ((quint8)this->registerData[DMASource2] & 0x40) << 16;
            }

            dmaSource <<= 1;

            if (dmaLength == 0xFFFF)
               dmaLength = 0x1FFFF;
            else if (dmaLength == 0x7FFF)
               dmaLength = 0xFFFF;
         }
      }

      void performDirectWrite(quint16 data) {
         switch (this->command) {
            case CRAM_WRITE:
               this->cram[this->addressRegister] = data & 0x00FF;
               this->cram[this->addressRegister + 1] = (data & 0xFF00) >> 8;
               this->addressRegister += this->registerData[AutoIncrementValue];
               break;

            case VRAM_WRITE:
               this->vram[this->addressRegister] = data & 0x00FF;
               this->vram[this->addressRegister + 1] = (data & 0xFF00) >> 8;
               this->addressRegister += this->registerData[AutoIncrementValue];
               break;

            case VSRAM_WRITE:
               this->vsram[this->addressRegister] = data & 0x00FF;
               this->vsram[this->addressRegister + 1] = (data & 0xFF00) >> 8;
               this->addressRegister += this->registerData[AutoIncrementValue];
               break;


            default:
               break;
         }
      }

      void drawFrame() {

      }

      inline quint32 readColor(quint8 index) {
         return this->readColor(false, (index & 0x30) >> 4, (index & 0xF));
      }

      inline quint32 readColor(bool priority, int row, int cell) {
         if (cell == 0)
            return 0x01000000;

         int address = (row * 16 * 2) + (cell * 2);

         quint8 b = (((this->cram[address] & 0xE) >> 1) * 255) / 7;
         quint8 g = (((this->cram[address + 1] & 0xE0) >> 5) * 255) / 7;
         quint8 r = (((this->cram[address + 1] & 0x0E) >> 1) * 255) / 7;

         return (priority ? 0xFF000000 : 0x0) | r << 16 | g << 8 | b;
      }

      void blitCharacter(QImage* buffer, quint16 address, int palette, bool priority, bool flipH, bool flipV, int x, int y, int w, int h) {
         quint8* art = (quint8*)(this->vram.data() + address);

         if (y > buffer->height() || x > buffer->width())
            return;

         int srcX = flipH ? 7 : 0;
         int srcY = flipV ? 7 : 0;
         QRgb* scanline = 0;

         for (int y1 = y; y1 < y+h; y1++) {
            if (y1 < 0)
               goto scanlineEnd;
            else if (y1 >= buffer->height())
               break;

            scanline = (QRgb*)buffer->scanLine(y1);

            for (int x1 = x; x1 < x+w; x1++) {
               if (x1 < 0)
                  goto pixelEnd;
               else if (x1 >= buffer->width())
                  break;

               //if (srcX == 0 || srcY == 0 || srcX == 7 || srcY  == 7)
               //   scanline[x1] = 0xFFFF0000;
               //else {
                  if (srcX % 2)
                     scanline[x1] = this->readColor(priority, palette, (art[srcY * 4 + srcX / 2] & 0x0F));
                  else
                     scanline[x1] = this->readColor(priority, palette, (art[srcY * 4 + srcX / 2] & 0xF0) >> 4);
               //}

               pixelEnd:
               if (flipH)
                  srcX--;
               else
                  srcX++;
            }

            scanlineEnd:
            if (flipV)
               srcY--;
            else
               srcY++;

            srcX = flipH ? 7 : 0;
         }
      }

      void drawPlane(QImage* buffer, quint16 address,  int width, int height) {
         for(int y = 0; y < height; y++) {
            for(int x = 0; x < width; x++) {
               quint16* nametable = (quint16*)(this->vram.data() + address);
               quint16 entry = qFromBigEndian(nametable[y * width + x]);

               quint16 artIndex = (entry & 0x3FF) << 5;
               quint8  palletteLine = (entry & 0x60) >> 5;

               this->blitCharacter(buffer, artIndex, palletteLine, entry & 0x8000, entry & 0x800, entry & 0x1000, x * 8, y * 8, 8, 8);
            }
         }
      }

   private:
      VDP* q_ptr;
      Q_DECLARE_PUBLIC(VDP)
};

VDP::VDP(QObject *parent)
   : QObject(parent),
     Device(),
     d_ptr(new VDPPrivate(this))
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

   int planeWidth, planeHeight;
   switch ((d->registerData[PlaneSize] & 0x30) >> 4) {
      case 0x00: planeHeight = 32; break;
      case 0x01: planeHeight = 64; break;
      case 0x02: planeHeight = 32; break;
      case 0x03: planeHeight = 128; break;
   }

   switch ((d->registerData[PlaneSize] & 0x03)) {
      case 0x00: planeWidth = 32; break;
      case 0x01: planeWidth = 64; break;
      case 0x02: planeWidth = 32; break;
      case 0x03: planeWidth = 128; break;
   }

   while(d->currentCycles < 0) {
      if (this->interruptPending())
         this->clearInterrupt();

      if (d->dmaActive) {
         /*qDebug() << "DMA Length" << QString::number(dmaLength, 16);
         qDebug() << "DMA Source" << QString::number(dmaSource, 16);
         qDebug() << "DMA Dest" << QString::number(d->addressRegister, 16);
         qDebug() << "DMA Type" << QString::number(dmaType, 16);*/

         //while(true) {
         if (d->dmaLength) {
            d->dmaLength--;
            d->dmaLength = d->dmaLength & 0x1FFFF;

            QByteArray* target = 0;

            switch(d->command) {
               case VSRAM_WRITE:
                  target = &d->vsram;
                  break;

               case CRAM_WRITE:
                  target = &d->cram;
                  break;

               case VRAM_WRITE:
                  target = &d->vram;
                  break;
            }

            quint8 b0, b1;

            d->bus->peek(d->dmaSource, b0);
            (*target)[d->addressRegister] = b0;

            d->bus->peek(d->dmaSource + 1, b1);
            (*target)[d->addressRegister + 1] = b1;

            d->addressRegister += d->registerData[AutoIncrementValue];
            d->dmaSource += 2;
         } else {
            d->command = NONE;
            d->dmaActive = false;
            d->cpu->setDisabled(false);

            qDebug() << "ENABLE MOTOROLA";
         }
      }

      //} else {
      //   d->cpu->setDisabled(false);
      //}

      // Check if display is enabled
      if (d->registerData[ModeRegister2] & MODE2_DE) {
         if (d->beamV == 0 && d->beamH == 0) {
            if(d->registerData[ModeRegister2] & MODE2_M2)
               d->scanlines = 30 * 8;
            else
               d->scanlines = 28 * 8;

            if(d->registerData[ModeRegister4] & (MODE4_RS0 | MODE4_RS1))
               d->width = 40 * 8;
            else
               d->width = 32 * 8;

            d->beamH = 0;

            // Draw Plane A
            d->drawPlane(&d->planeA, (d->registerData[PlaneANameTable] & 0x38) << 10, planeWidth, planeHeight);
         }


         QRgb* scanlineData = (QRgb*)d->frame.scanLine(d->beamV);

         // Draw Background
         scanlineData[d->beamH] = d->readColor(d->registerData[BackgroundColor]) & 0x00FFFFFF;

         if (d->beamH >= d->width) {
            d->beamH = 0;
            d->beamV++;
         }

         if (d->beamV >= 0xE0) {
            int pixels = d->frame.width() * d->frame.height();
            QRgb* frameBit = (QRgb*)d->frame.bits();
            QRgb* planeABit = (QRgb*)d->planeA.bits();
            QRgb* planeBBit = (QRgb*)d->planeB.bits();
            QRgb* windowBit = (QRgb*)d->windowPlane.bits();

            for(int b=0; b < pixels; b++) {
               if (planeABit[b] != 0x01000000 && ((planeABit[b] & 0xFF000000) || (frameBit[b] & 0xFF000000) == (planeABit[b] & 0xFF000000)))
                  frameBit[b] = planeABit[b];

               if (planeBBit[b] != 0x01000000 && ((planeBBit[b] & 0xFF000000) || (frameBit[b] & 0xFF000000) == (planeBBit[b] & 0xFF000000)))
                  frameBit[b] = planeBBit[b];

               if (windowBit[b] != 0x01000000 && ((windowBit[b] & 0xFF000000) || (frameBit[b] & 0xFF000000) == (windowBit[b] & 0xFF000000)))
                  frameBit[b] = windowBit[b];

               frameBit[b] |= 0xFF000000;
            }

            emit this->frameUpdated(&d->frame);

            if (d->registerData[ModeRegister2] & MODE2_IE0) {
               d->vertialInterruptPending = true;
               this->interruptRequest(6);

               d->z80->interrupt();
               d->currentCycles = 0;
            }
         }

         if (d->beamV == d->registerData[HorizontalInterruptCounter] && (d->registerData[ModeRegister1] & MODE1_IE1)) {
            this->interruptRequest(4);
         }

         d->beamH++;

         if (d->beamV >= d->scanlines) {
            d->beamV = 0;
            d->beamH = 0;
         }
      }

      d->currentCycles += 4;
   }

   return 0;
}

void VDP::reset()
{
   Q_D(VDP);

   Device::reset();

   d->fifoEmpty = true;
   d->fifoFull = false;
   d->vertialInterruptPending = false;
   d->spriteOverflow = false;
   d->spriteCollision = true;
   d->oddFrame = false;
   d->vBlank = false;
   d->hBlank = false;
   d->dmaActive = false;
   d->palMode = true;
   d->writePending = false;

   d->beamH = 0;
   d->beamV = 0;
   d->lineCounter = d->registerData[HorizontalInterruptCounter];
   d->commandCount = 0;

   d->command = NONE;
   d->commandByte = 0;
   d->commandData = 0;
}

void VDP::attachZ80(Z80* cpu)
{
   Q_D(VDP);

   d->z80 = cpu;
}

int VDP::peek(quint32 address, quint8& val)
{
   Q_D(VDP);

   //qDebug() << "VDO READ" << address;

   switch(address) {
      case 0x00:
      case 0x01:
         d->prepareMemoryTransfer();
         return NO_ERROR;

      case 0x02:
         d->prepareMemoryTransfer();

         val = 0x34 |
               (d->fifoEmpty ? 0x2 : 0x0) |
               (d->fifoFull ? 0x1 : 0x0);

         return NO_ERROR;


      case 0x03:
         d->prepareMemoryTransfer();

         val = (d->vertialInterruptPending ? 0x80 : 0x00) |
               (d->spriteOverflow ? 0x40 : 0x00) |
               (d->spriteCollision ? 0x20 : 0x00) |
               (d->oddFrame ? 0x10 : 0x00) |
               (d->vBlank ? 0x08 : 0x00) |
               (d->hBlank ? 0x04 : 0x00) |
               (d->dmaActive ? 0x02 : 0x00) |
               (d->palMode ? 0x01 : 0x00);

         return NO_ERROR;
   }

   return NO_ERROR;
}

int VDP::poke(quint32 address, quint8 val)
{
   Q_D(VDP);

   //qDebug() << "VDP DATA WRITE" << address << "=" << val;

   switch(address) {
      case 0x00:
         d->dataWord &= ~0x00FF;
         d->dataWord |= val;

         d->prepareMemoryTransfer();
         return NO_ERROR;

      case 0x01:
         d->dataWord &= ~0xFF00;
         d->dataWord |= val << 8;

         d->prepareMemoryTransfer();
         d->performDirectWrite(d->dataWord);
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
