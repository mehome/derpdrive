#ifndef MOTOROLA68000_H
#define MOTOROLA68000_H

#include <QObject>

#include <memorybus.h>

class Device;

class Motorola68000Private;
class Motorola68000 : public QObject
{
      Q_OBJECT
   public:
      explicit Motorola68000(QObject *parent = nullptr);
      ~Motorola68000();

      // Setup
      void  attachBus(MemoryBus* bus);

      // Options
      void  setTracing(bool tracing);
      bool  tracing() const;

      // Hardware functions
      void  setDisabled(bool disabled);
      int   clock(int ticks);
      void  reset();
      void  interruptRequest(Device* device, int level);
      void  clearInterrupt(Device* device, int level);

      quint32  programCounter() const;

      // Debug
      QString               registerName(int reg) const;
      unsigned int          registerValue(int reg) const;
      QVector<unsigned int> breakpoints() const;

   signals:
      void singleStep(unsigned int pc, unsigned int opcode, QString instruction);
      void debugError(unsigned int pc, unsigned int opcode, QString instruction);
      void breakpointReached(unsigned int pc);

   public slots:
      void debugToggle(bool debug);
      void debugStep();
      void debugRun();
      void debugPause();
      void debugAddBreakpoint(unsigned int adress);
      void debugRemoveBreakpoint(unsigned int adress);

   private:
      Motorola68000Private* d_ptr;
      Q_DECLARE_PRIVATE(Motorola68000)
};

#endif // MOTOROLA68000_H
