#include "motorola68000private.h"
#include "memorybus.h"

#include <QDebug>

DecodeEntry Motorola68000Private::decodeTable[] = {
   #include "chips/m68k_instructions.h"
};

RegisterData Motorola68000Private::registerInfo[] = {
   {"D0", 0xffffffff, "Data Register 0"},
   {"D1", 0xffffffff, "Data Register 1"},
   {"D2", 0xffffffff, "Data Register 2"},
   {"D3", 0xffffffff, "Data Register 3"},
   {"D4", 0xffffffff, "Data Register 4"},
   {"D5", 0xffffffff, "Data Register 5"},
   {"D6", 0xffffffff, "Data Register 6"},
   {"D7", 0xffffffff, "Data Register 7"},

   {"A0", 0xffffffff, "Address Register 0"},
   {"A1", 0xffffffff, "Address Register 1"},
   {"A2", 0xffffffff, "Address Register 2"},
   {"A3", 0xffffffff, "Address Register 3"},
   {"A4", 0xffffffff, "Address Register 4"},
   {"A5", 0xffffffff, "Address Register 5"},
   {"A6", 0xffffffff, "Address Register 6"},
   {"A7", 0xffffffff, "Address Register 7 (User Stack Pointer)"},
   {"A7", 0xffffffff, "Address Register 7 (Supervisor Stack Pointer)"},

   {"PC", 0xffffffff, "Program Counter"},
   {"SR", 0x0000ffff,
      "Status Register: T-S--III-XNZVC\n T: Trace Mode\n"
      "S: Supervisory Mode\n"
      "I: Interrupt Mask Level\n"
      "X: Extend\n"
      "N: Negative\n"
      "Z: Zero\n"
      "V: Overflow\n"
      "C: Carry"}
};

Motorola68000Private::Motorola68000Private(Motorola68000* q)
   : q_ptr(q)
{
   this->registerData.resize(19);
   this->decodeCacheTable = new ExecutionPointer[65536];

   //Just Brute-Force the instruction table :D
   qDebug() << "Building instruction cache...";

   for(int opcode=0; opcode < 65536; opcode++) {
      for(int i=0; i < sizeof(Motorola68000Private::decodeTable) / sizeof(DecodeEntry); i++) {
         if ((opcode & Motorola68000Private::decodeTable[i].mask) == Motorola68000Private::decodeTable[i].signature) {
            this->decodeCacheTable[opcode] = Motorola68000Private::decodeTable[i].execute;
            goto ValidInstruction;
         }
      }

      this->decodeCacheTable[opcode] = &Motorola68000Private::ExecuteInvalid;

      ValidInstruction:
      continue;
   }

   qDebug() << "Done";
}

ExecutionPointer Motorola68000Private::decodeInstruction(int opcode)
{

   if (Motorola68000Private::decodeCacheTable[opcode] == 0) {

   }

   return Motorola68000Private::decodeCacheTable[opcode];
}

void Motorola68000Private::setRegister(int reg, unsigned int value, int size)
{
   switch (size) {
      case BYTE:
         this->registerData[reg] &= 0xffffff00;
         this->registerData[reg] |= (value & 0xff);
         break;

      case WORD:
         this->registerData[reg] &= 0xffff0000;
         this->registerData[reg] |= (value & 0xffff);
         break;

      case LONG:
         this->registerData[reg] &= 0x00000000;
         this->registerData[reg] |= (value & 0xffffffff);
         break;
   }
}

int Motorola68000Private::peek(quint32 address, unsigned int& value, int size)
{
   quint8 b1, b2, b3, b4;

   switch(size) {
      case BYTE:
         if (this->bus->peek(address, b1))
            return EXECUTE_BUS_ERROR;

         value = (unsigned int)b1;
         return EXECUTE_OK;

      case WORD:
         if ((address & 1) != 0)
            return EXECUTE_ADDRESS_ERROR;

         if (this->bus->peek(address, b1) ||
             this->bus->peek(address + 1, b2))
            return EXECUTE_BUS_ERROR;

         value = (((unsigned int)b1) << 8) | ((unsigned int) b2);
         return EXECUTE_OK;

      case LONG:
         if ((address & 1) != 0)
            return EXECUTE_ADDRESS_ERROR;

         if (this->bus->peek(address, b1) ||
             this->bus->peek(address + 1, b2) ||
             this->bus->peek(address + 2, b3) ||
             this->bus->peek(address + 3, b4))
            return EXECUTE_BUS_ERROR;

         value = ((unsigned int)b1 << 24) |
                 ((unsigned int)b2 << 16) |
                 ((unsigned int)b3 << 8)  |
                 ((unsigned int)b4 << 0);
         return EXECUTE_OK;

      default:
         return EXECUTE_ILLEGAL_INSTRUCTION;
   }
}

int Motorola68000Private::poke(quint32 address, unsigned int value, int size)
{
   switch (size) {
      case BYTE:
         if (this->bus->poke(address, (quint8)value))
            return EXECUTE_BUS_ERROR;

         return EXECUTE_OK;

      case WORD:
         if ((address & 1) != 0)
            return EXECUTE_ADDRESS_ERROR;

         if (this->bus->poke(address + 1, (quint8)value) ||
             this->bus->poke(address, (quint8)(value >> 8)))
            return EXECUTE_BUS_ERROR;

         return EXECUTE_OK;

      case LONG:
         if ((address & 1) != 0)
            return EXECUTE_ADDRESS_ERROR;

         if (this->bus->poke(address + 3, (quint8)(value >> 0))  ||
             this->bus->poke(address + 2, (quint8)(value >> 8))  ||
             this->bus->poke(address + 1, (quint8)(value >> 16)) ||
             this->bus->poke(address + 0, (quint8)(value >> 24)))
            return EXECUTE_BUS_ERROR;

         return EXECUTE_OK;

      default:
         return EXECUTE_ILLEGAL_INSTRUCTION;
   }
}

int Motorola68000Private::processException(int vector)
{
   int status;

   // Push the PC and SR onto supervisor stack
   this->setRegister(SSP_INDEX, this->registerData[SSP_INDEX] - 4, LONG);
   if ((status = this->poke(this->registerData[SSP_INDEX],
                            this->registerData[PC_INDEX],
                            LONG)) != EXECUTE_OK) {
      return status;
   }

   this->setRegister(SSP_INDEX, this->registerData[SSP_INDEX] - 2, LONG);
   if ((status = this->poke(this->registerData[SSP_INDEX],
                            this->registerData[SR_INDEX],
                            LONG)) != EXECUTE_OK) {
      return status;
   }

   // Switch to Supervisor mode
   this->registerData[SR_INDEX] |= S_FLAG;
   this->registerData[SR_INDEX] &= ~T_FLAG;

   // Load handler address from vector table
   uint32_t serviceAddress;
   if ((status = this->peek(vector * 4, serviceAddress, LONG)) != EXECUTE_OK) {
      return status;
   }

   // Jump to handler
   this->setRegister(PC_INDEX, serviceAddress, LONG);

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteABCD(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteADD(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteADDA(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteADDI(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteADDQ(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteADDX(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteAND(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteANDI(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteANDItoCCR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteANDItoSR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteASL(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteASR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteBRA(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteBREAK(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteBSR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteBCC(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteBit(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCHK(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCLR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCMP(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCMPA(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCMPI(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCMPM(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteDBCC(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteDIVS(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteDIVU(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEOR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEORI(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEORItoCCR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEORItoSR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEXG(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEXT(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteILLEGAL(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteJMP(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteJSR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteLEA(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteLINK(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteLINKWORD(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteLSL(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteLSR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVE(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEA(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEM(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEP(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEQ(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEUSP(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEfromSR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEfromCCR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEfromUSP(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEtoCCR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEtoSR(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEtoUSP(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMULS(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMULU(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteNBCD(int opcode, QString description, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteNEG(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteNEGX(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteNOP(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteNOT(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteOR(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteORI(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteORItoCCR(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteORItoSR(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecutePEA(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteRESET(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteROL(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteROR(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteROXL(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteROXR(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteRTE(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteRTR(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteRTS(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteRTD(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSBCD(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSTOP(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSUB(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSUBA(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSUBI(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSUBQ(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSUBX(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSWAP(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSCC(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteTAS(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteTRAP(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteTRAPV(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteTST(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteUNLK(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCCR(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteBKPT(int opcode, QString& traceRecord, int trace) {
   int status;

   // Move the PC back to the address of the instructoin
   this->setRegister(PC_INDEX, this->registerData[PC_INDEX] - 2, LONG);

   // Process illegal instructoin exception
   if ((status = this->processException(4)) != EXECUTE_OK)
      return status;

   if (trace)

}

int Motorola68000Private::ExecuteInvalid(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}


int Motorola68000Private::ExecuteBusError(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteAddressError(int opcode, QString& traceRecord, int trace) {
   return EXECUTE_ILLEGAL_INSTRUCTION;
}
