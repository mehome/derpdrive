#ifndef MOTOROLA68000PRIVATE_H
#define MOTOROLA68000PRIVATE_H

#include <QObject>
#include <QVector>

class MemoryBus;
class Motorola68000;
class Motorola68000Private;

typedef int (Motorola68000Private::*ExecutionPointer)(int, QString&, int);

struct DecodeEntry {
   public:
      quint16 signature;
      quint16 mask;
      ExecutionPointer execute;
};

struct RegisterData {
   public:
      QString name;
      quint32 mask;
      QString description;
};

typedef uint32_t Register;

class Motorola68000Private {
   public:
      int               tickCount;
      MemoryBus*        bus;

      QVector<Register>  registerData;

      // FPU
      QVector<double>   floatingPointRegister;
      quint32           fpcr;
      quint32           fpsr;
      quint32           fpiar;

   public:
      static RegisterData        registerInfo[];

      static DecodeEntry         decodeTable[];
      ExecutionPointer*       decodeCacheTable;

   public:
      enum ExecutionResult {
         EXECUTE_OK,
         EXECUTE_PRIVILEGED_OK,
         EXECUTE_BUS_ERROR,
         EXECUTE_ADDRESS_ERROR,
         EXECUTE_ILLEGAL_INSTRUCTION,
      };

      enum State {
         STATE_NORMAL,
         STATE_HALT,
         STATE_STOP,
         STATE_BREAK,
      };

      enum Size {
         BYTE,
         WORD,
         LONG,
      };

      enum RegisterIndex {
         D0_INDEX = 0,
         A0_INDEX = 8,
         USP_INDEX = 15,
         SSP_INDEX = 16,
         PC_INDEX = 17,
         SR_INDEX = 18,
      };

      enum Flags {
         C_FLAG   = 0x0001,
         V_FLAG   = 0x0002,
         Z_FLAG   = 0x0004,
         N_FLAG   = 0x0008,
         X_FLAG   = 0x0010,
         I0_FLAG  = 0x0100,
         I1_FLAG  = 0x0200,
         I2_FLAG  = 0x0400,
         S_FLAG   = 0x2000,
         T_FLAG   = 0x8000
      };

   public:
      Motorola68000Private(Motorola68000* q);

   public:
      ExecutionPointer  decodeInstruction(int opcode);

      void setRegister(int reg, unsigned int value, int size);

      int peek(quint32 address, unsigned int &value, int size);
      int poke(quint32 address, unsigned int value, int size);

      int processException(int vector);

   public:
      int ExecuteABCD(int opcode, QString& traceRecord, int trace);
      int ExecuteADD(int opcode, QString& traceRecord, int trace);
      int ExecuteADDA(int opcode, QString& traceRecord, int trace);
      int ExecuteADDI(int opcode, QString& traceRecord, int trace);
      int ExecuteADDQ(int opcode, QString& traceRecord, int trace);
      int ExecuteADDX(int opcode, QString& traceRecord, int trace);
      int ExecuteAND(int opcode, QString& traceRecord, int trace);
      int ExecuteANDI(int opcode, QString& traceRecord, int trace);
      int ExecuteANDItoCCR(int opcode, QString& traceRecord, int trace);
      int ExecuteANDItoSR(int opcode, QString& traceRecord, int trace);
      int ExecuteASL(int opcode, QString& traceRecord, int trace);
      int ExecuteASR(int opcode, QString& traceRecord, int trace);
      int ExecuteBRA(int opcode, QString& traceRecord, int trace);
      int ExecuteBREAK(int opcode, QString& traceRecord, int trace);
      int ExecuteBSR(int opcode, QString& traceRecord, int trace);
      int ExecuteBCC(int opcode, QString& traceRecord, int trace);
      int ExecuteBit(int opcode, QString& traceRecord, int trace);
      int ExecuteCHK(int opcode, QString& traceRecord, int trace);
      int ExecuteCLR(int opcode, QString& traceRecord, int trace);
      int ExecuteCMP(int opcode, QString& traceRecord, int trace);
      int ExecuteCMPA(int opcode, QString& traceRecord, int trace);
      int ExecuteCMPI(int opcode, QString& traceRecord, int trace);
      int ExecuteCMPM(int opcode, QString& traceRecord, int trace);
      int ExecuteDBCC(int opcode, QString& traceRecord, int trace);
      int ExecuteDIVS(int opcode, QString& traceRecord, int trace);
      int ExecuteDIVU(int opcode, QString& traceRecord, int trace);
      int ExecuteEOR(int opcode, QString& traceRecord, int trace);
      int ExecuteEORI(int opcode, QString& traceRecord, int trace);
      int ExecuteEORItoCCR(int opcode, QString& traceRecord, int trace);
      int ExecuteEORItoSR(int opcode, QString& traceRecord, int trace);
      int ExecuteEXG(int opcode, QString& traceRecord, int trace);
      int ExecuteEXT(int opcode, QString& traceRecord, int trace);
      int ExecuteILLEGAL(int opcode, QString& traceRecord, int trace);
      int ExecuteJMP(int opcode, QString& traceRecord, int trace);
      int ExecuteJSR(int opcode, QString& traceRecord, int trace);
      int ExecuteLEA(int opcode, QString& traceRecord, int trace);
      int ExecuteLINK(int opcode, QString& traceRecord, int trace);
      int ExecuteLINKWORD(int opcode, QString& traceRecord, int trace);
      int ExecuteLSL(int opcode, QString& traceRecord, int trace);
      int ExecuteLSR(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVE(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEA(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEM(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEP(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEQ(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEUSP(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEfromSR(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEfromCCR(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEfromUSP(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEtoCCR(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEtoSR(int opcode, QString& traceRecord, int trace);
      int ExecuteMOVEtoUSP(int opcode, QString& traceRecord, int trace);
      int ExecuteMULS(int opcode, QString& traceRecord, int trace);
      int ExecuteMULU(int opcode, QString& traceRecord, int trace);
      int ExecuteNBCD(int opcode, QString& traceRecord, int trace);
      int ExecuteNEG(int opcode, QString& traceRecord, int trace);
      int ExecuteNEGX(int opcode, QString description, int trace);
      int ExecuteNOP(int opcode, QString& traceRecord, int trace);
      int ExecuteNOT(int opcode, QString& traceRecord, int trace);
      int ExecuteOR(int opcode, QString& traceRecord, int trace);
      int ExecuteORI(int opcode, QString& traceRecord, int trace);
      int ExecuteORItoCCR(int opcode, QString& traceRecord, int trace);
      int ExecuteORItoSR(int opcode, QString& traceRecord, int trace);
      int ExecutePEA(int opcode, QString& traceRecord, int trace);
      int ExecuteRESET(int opcode, QString& traceRecord, int trace);
      int ExecuteROL(int opcode, QString& traceRecord, int trace);
      int ExecuteROR(int opcode, QString& traceRecord, int trace);
      int ExecuteROXL(int opcode, QString& traceRecord, int trace);
      int ExecuteROXR(int opcode, QString& traceRecord, int trace);
      int ExecuteRTE(int opcode, QString& traceRecord, int trace);
      int ExecuteRTR(int opcode, QString& traceRecord, int trace);
      int ExecuteRTS(int opcode, QString& traceRecord, int trace);
      int ExecuteRTD(int opcode, QString& traceRecord, int trace);
      int ExecuteSBCD(int opcode, QString& traceRecord, int trace);
      int ExecuteSTOP(int opcode, QString& traceRecord, int trace);
      int ExecuteSUB(int opcode, QString& traceRecord, int trace);
      int ExecuteSUBA(int opcode, QString& traceRecord, int trace);
      int ExecuteSUBI(int opcode, QString& traceRecord, int trace);
      int ExecuteSUBQ(int opcode, QString& traceRecord, int trace);
      int ExecuteSUBX(int opcode, QString& traceRecord, int trace);
      int ExecuteSWAP(int opcode, QString& traceRecord, int trace);
      int ExecuteSCC(int opcode, QString& traceRecord, int trace);
      int ExecuteTAS(int opcode, QString& traceRecord, int trace);
      int ExecuteTRAP(int opcode, QString& traceRecord, int trace);
      int ExecuteTRAPV(int opcode, QString& traceRecord, int trace);
      int ExecuteTST(int opcode, QString& traceRecord, int trace);
      int ExecuteUNLK(int opcode, QString& traceRecord, int trace);
      int ExecuteCCR(int opcode, QString& traceRecord, int trace);
      int ExecuteBKPT(int opcode, QString& traceRecord, int trace);
      int ExecuteInvalid(int opcode, QString& traceRecord, int trace);

      int ExecuteBusError(int opcode, QString& traceRecord, int trace);
      int ExecuteAddressError(int opcode, QString& traceRecord, int trace);

   public:
      Motorola68000* q_ptr;
      Q_DECLARE_PUBLIC(Motorola68000)
};

#endif // MOTOROLA68000PRIVATE_H
