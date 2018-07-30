#include "motorola68000.h"
#include "chips/motorola68000private.h"
#include "device.h"

#include <QDebug>
#include <QVector>
#include <QtEndian>

#include <signal.h>

Motorola68000::Motorola68000(QObject *parent)
   : QObject(parent),
     d_ptr(new Motorola68000Private(this))
{

}

Motorola68000::~Motorola68000()
{
   delete d_ptr;
}

void Motorola68000::attachBus(MemoryBus* bus)
{
   Q_D(Motorola68000);

   d->bus = bus;
}

void Motorola68000::setTracing(bool tracing)
{
   Q_D(Motorola68000);

   d->tracing = tracing;
}

bool Motorola68000::tracing() const
{
   Q_D(const Motorola68000);

   return d->tracing;
}

void Motorola68000::setDisabled(bool disabled)
{
   Q_D(Motorola68000);

   d->disabled = disabled;
}

int Motorola68000::clock(int ticks)
{
   Q_D(Motorola68000);

   d->currentTicks -= ticks;

   QString  traceMessage;

   while(d->currentTicks < 0) {
      if (d->disabled) {
         d->currentTicks = 0;
         return 0;
      }

      unsigned int opcode = 0;
      int      status = Motorola68000Private::EXECUTE_OK;

      if (d->tracing) {
         traceMessage = "0x";
         traceMessage += QString::number(d->registerData[Motorola68000Private::PC_INDEX], 16).rightJustified(8, '0');
         traceMessage += ' ';
      }

      if (d->state != Motorola68000Private::STATE_HALT) {
         // Handle interrupts
         bool interruptFlag = false;
         status = d->handleInterrupts(interruptFlag);

         if (!interruptFlag && status == Motorola68000Private::EXECUTE_OK) {

            // Make sure we arent waiting for interrupts
            if (d->state != Motorola68000Private::STATE_STOP) {
               // Fetch instructions
              status = d->peek(d->registerData[Motorola68000Private::PC_INDEX],
                               opcode,
                               Motorola68000Private::WORD);

              if (status == Motorola68000Private::EXECUTE_OK) {
                 d->registerData[Motorola68000Private::PC_INDEX] += 2;

                 // Execute instructoin
                 ExecutionPointer method = d->decodeInstruction(opcode);
                 status = (d->*method)(opcode, traceMessage, d->tracing);

                 // If the last instruction was not provileged then check for trace
                 if ((status == Motorola68000Private::EXECUTE_OK) &&
                     (d->registerData[Motorola68000Private::SR_INDEX] & Motorola68000Private::T_FLAG)) {
                     d->processException(9);
                 }
              }
            } else if(d->tracing) {

               traceMessage += "CPU is stopped";
            }
         }

         if (status == Motorola68000Private::EXECUTE_BUS_ERROR) {
            if (d->ExecuteBusError(opcode, traceMessage, d->tracing) != Motorola68000Private::EXECUTE_OK) {
               // Exception in exception handler, well... shit
               d->state = Motorola68000Private::STATE_HALT;
               if (d->tracing)
                  traceMessage += "Double Bus/Address Error! CPU halted";
            }
         } else if (status == Motorola68000Private::EXECUTE_ADDRESS_ERROR) {
            if (d->ExecuteAddressError(opcode, traceMessage, d->tracing) != Motorola68000Private::EXECUTE_OK) {
               // Exception in exception handler, well... shit
               d->state = Motorola68000Private::STATE_HALT;
               if (d->tracing)
                  traceMessage += "Double Bus/Address Error! CPU halted";
            }
         } else if (status == Motorola68000Private::EXECUTE_ILLEGAL_INSTRUCTION) {
            traceMessage += " !!Illegal instruction";
         }
      } else {
         if(d->tracing)
            traceMessage += "CPU is halted";

         d->currentTicks = 0;
      }

      if (d->tracing)
         qDebug().noquote() << traceMessage;
   }

   return 0;
}

void Motorola68000::reset()
{
   Q_D(Motorola68000);

   quint32 pc, ssp;

   /*
    * Reset Flags
    */
   d->registerData[Motorola68000Private::SR_INDEX] =
         Motorola68000Private::S_FLAG |
         Motorola68000Private::I0_FLAG |
         Motorola68000Private::I1_FLAG |
         Motorola68000Private::I2_FLAG;

   /*
    * Fetch Supervisor Stack Pointer
    */
   if (d->peek(0x000000, ssp, Motorola68000Private::LONG) != Motorola68000Private::EXECUTE_OK)
      d->setRegister(Motorola68000Private::SSP_INDEX, 0, Motorola68000Private::LONG);
   else
      d->setRegister(Motorola68000Private::SSP_INDEX, ssp, Motorola68000Private::LONG);

   /*
    * Fetch Program Counter
    */
   if (d->peek(0x000004, pc, Motorola68000Private::LONG) != Motorola68000Private::EXECUTE_OK)
      d->setRegister(Motorola68000Private::PC_INDEX, 0, Motorola68000Private::LONG);
   else
      d->setRegister(Motorola68000Private::PC_INDEX, pc, Motorola68000Private::LONG);

   d->state = Motorola68000Private::STATE_NORMAL;

   qDebug() << "SSP" << d->registerData[Motorola68000Private::SSP_INDEX];
   qDebug() << "Programm counter: " << d->registerData[Motorola68000Private::PC_INDEX];
}

void Motorola68000::interruptRequest(Device* device, int level)
{
   Q_D(Motorola68000);

   // We have seven levels of interrupts
   if (level > 7)
      level = 7;
   else if (level < 1)
      level = 1;

   d->pendingInterrupts.push(Motorola68000Private::PendingInterrupt(level, device));
}

void Motorola68000::clearInterrupt(Device* device, int level)
{
   Q_D(Motorola68000);

   while(!d->pendingInterrupts.empty())
      d->pendingInterrupts.pop();
}

quint32 Motorola68000::programCounter() const
{
   Q_D(const Motorola68000);

   return d->registerData[Motorola68000Private::PC_INDEX];
}
