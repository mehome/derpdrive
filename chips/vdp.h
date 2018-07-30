#ifndef VDP_H
#define VDP_H

#include <QObject>
#include <memorybus.h>
#include <device.h>
#include <QPixmap>

class Motorola68000;
class Z80;

class VDPPrivate;
class VDP
      : public QObject,
        public IMemory,
        public Device
{
      Q_OBJECT

   public:
      explicit VDP(QObject *parent = nullptr);
      ~VDP();

      void           attachBus(MemoryBus* bus);
      MemoryBus*     bus() const;

      int            clock(int cycles);

      virtual void   reset() override;

      int            peek(quint32 address, quint8& val);
      int            poke(quint32 address, quint8 val);

      void           attachCpu(Motorola68000* cpu);
      void           attachZ80(Z80* cpu);

      const QByteArray  cram() const;
      const QByteArray  vram() const;

      void           debugBlit(QImage* buffer, quint16 address, int palette, int x, int y) const;

   signals:
      void           frameUpdated(QImage* frame);
      void           dmaFinished();

   public slots:

   private:
      VDPPrivate* d_ptr;
      Q_DECLARE_PRIVATE(VDP)
};

#endif // VDP_H
