#include "motorola68000private.h"
#include "memorybus.h"
#include "../device.h"

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
   : q_ptr(q),
     disabled(false),
     currentTicks(0),
     tracing(false)
{
   this->registerData.resize(19);
   this->decodeCacheTable = new ExecutionPointer[65536];

   //Just Brute-Force the instruction table :D
   qDebug() << "Building instruction cache...";

   for(int opcode=0; opcode < 65536; opcode++) {
      this->decodeCacheTable[opcode] = &Motorola68000Private::ExecuteInvalid;

      for(int i=0; i < sizeof(Motorola68000Private::decodeTable) / sizeof(DecodeEntry); i++) {
         if ((opcode & Motorola68000Private::decodeTable[i].mask) == Motorola68000Private::decodeTable[i].signature) {
            this->decodeCacheTable[opcode] = Motorola68000Private::decodeTable[i].execute;
            break;
         }
      }
   }

   qDebug() << "Done";
}

ExecutionPointer Motorola68000Private::decodeInstruction(int opcode)
{
   return Motorola68000Private::decodeCacheTable[opcode];
}

void Motorola68000Private::setRegister(int reg, unsigned int value, int size)
{
   if (reg == PC_INDEX) // Wrap PC
      value = value & 0x00FFFFFF;

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

   address = address & 0x00FFFFFF;

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
   address = address & 0x00FFFFFF;

   switch (size) {
      case BYTE:
         if (this->bus->poke(address, (quint8)value))
            return EXECUTE_BUS_ERROR;

         return EXECUTE_OK;

      case WORD:
         if ((address & 1) != 0)
            return EXECUTE_ADDRESS_ERROR;

         if (this->bus->poke(address, (quint8)(value >> 8)) ||
             this->bus->poke(address + 1, (quint8)value))
            return EXECUTE_BUS_ERROR;

         return EXECUTE_OK;

      case LONG:
         if ((address & 1) != 0)
            return EXECUTE_ADDRESS_ERROR;

         if (this->bus->poke(address + 0, (quint8)(value >> 24)) ||
             this->bus->poke(address + 1, (quint8)(value >> 16)) ||
             this->bus->poke(address + 2, (quint8)(value >> 8))  ||
             this->bus->poke(address + 3, (quint8)(value >> 0)))
            return EXECUTE_BUS_ERROR;

         return EXECUTE_OK;

      default:
         return EXECUTE_ILLEGAL_INSTRUCTION;
   }
}

int Motorola68000Private::push(int stackRegister, unsigned int value, int size)
{
   int displacement;

   if (size == BYTE)
      displacement = 1;
   else if (size == WORD)
      displacement = 2;
   else if (size == LONG)
      displacement = 4;

   this->setRegister(stackRegister, this->registerData[stackRegister] - displacement, LONG);
   return this->poke(this->registerData[stackRegister],
                     value,
                     size);
}

int Motorola68000Private::pop(int stackRegister, unsigned int& value, int size)
{
   int status;
   int displacement;

   if (size == BYTE)
      displacement = 1;
   else if (size == WORD)
      displacement = 2;
   else if (size == LONG)
      displacement = 4;

   if (( status = this->peek(this->registerData[stackRegister],
                             value,
                             size)) != EXECUTE_OK) {
      return status;
   }

   this->setRegister(stackRegister, this->registerData[stackRegister] + displacement, LONG);

   return EXECUTE_OK;
}

int Motorola68000Private::computeEffectiveAddress(quint32& address, int& in_register, QString& traceRecord, int mode_register, int size, bool trace)
{
   Register tmpRegister;
   unsigned int extend_word;
   int status;

   switch (mode_register >> 3) {
      case 0: // Data register direct
         address = (mode_register & 0x7) + D0_INDEX;

         if (trace)
            traceRecord += this->registerInfo[address].name;

         in_register = 1;
         return EXECUTE_OK;

      case 1: // Address register direct
         address = (mode_register & 0x7);
         if ((address == 7) && (this->registerData[SR_INDEX & S_FLAG]))
            address = SSP_INDEX;
         else
            address += A0_INDEX;

         if (trace)
            traceRecord += this->registerInfo[address].name;

         in_register = 1;
         return EXECUTE_OK;

      case 2: // Address register indirect
         if (size == BYTE || size == WORD)
            this->currentTicks += 4;
         else
            this->currentTicks += 8;

         tmpRegister = (mode_register & 0x7);
         if ((tmpRegister == 7) && (this->registerData[SR_INDEX] & S_FLAG))
            tmpRegister = SSP_INDEX;
         else
            tmpRegister += A0_INDEX;

         address = this->registerData[tmpRegister];

         if (trace)
            traceRecord += "(" + this->registerInfo[tmpRegister].name + ")";

         in_register = 0;
         return EXECUTE_OK;

      case 3: // Address register indirect with post-increment
         if (size == BYTE || size == WORD)
            this->currentTicks += 4;
         else
            this->currentTicks += 8;

         tmpRegister = (mode_register & 0x7);
         if ((tmpRegister == 7) && (this->registerData[SR_INDEX] & S_FLAG))
            tmpRegister = SSP_INDEX;
         else
            tmpRegister += A0_INDEX;

         address = this->registerData[tmpRegister];

         switch (size) {
            case BYTE:
               if ((tmpRegister == SSP_INDEX) || (tmpRegister == USP_INDEX))
                  this->registerData[tmpRegister] += 2;
               else
                  this->registerData[tmpRegister] += 1;
               break;

            case WORD:
               this->registerData[tmpRegister] += 2;
               break;

            case LONG:
               this->registerData[tmpRegister] += 4;
               break;
         }

         if (trace)
            traceRecord += "(" + this->registerInfo[tmpRegister].name + ")+";

         in_register = 0;
         return EXECUTE_OK;

      case 4: // Address register indirect with pre-decrement
         if (size == BYTE || size == WORD)
            this->currentTicks += 6;
         else
            this->currentTicks += 10;

         tmpRegister = (mode_register & 0x7);
         if ((tmpRegister == 7) && (this->registerData[SR_INDEX] & S_FLAG))
            tmpRegister = SSP_INDEX;
         else
            tmpRegister += A0_INDEX;

         switch (size) {
            case BYTE:
               if ((tmpRegister == SSP_INDEX) || (tmpRegister == USP_INDEX))
                  this->registerData[tmpRegister] -= 2;
               else
                  this->registerData[tmpRegister] -= 1;
               break;

            case WORD:
               this->registerData[tmpRegister] -= 2;
               break;

            case LONG:
               this->registerData[tmpRegister] -= 4;
               break;
         }

         address = this->registerData[tmpRegister];


         if (trace)
            traceRecord += "-(" + this->registerInfo[tmpRegister].name + ")";

         in_register = 0;
         return EXECUTE_OK;

      case 5: // Address register indirect with displacement
         if (size == BYTE || size == WORD)
            this->currentTicks += 8;
         else
            this->currentTicks += 12;

         tmpRegister = (mode_register & 0x7);
         if ((tmpRegister == 7) && (this->registerData[SR_INDEX] & S_FLAG))
            tmpRegister = SSP_INDEX;
         else
            tmpRegister += A0_INDEX;

         address = this->registerData[tmpRegister];

         status = this->peek(this->registerData[PC_INDEX], extend_word, WORD);
         if (status != EXECUTE_OK) {
            this->registerData[PC_INDEX] += 2;
            return status;
         } else
            this->registerData[PC_INDEX] += 2;

         address += this->signExtend(extend_word, WORD);

         if (trace)
            traceRecord += "($" +
                           QString::number(extend_word, 16).leftJustified(4, '0') +
                           "," +
                           this->registerInfo[tmpRegister].name +
                           ")";

         in_register = 0;
         return EXECUTE_OK;

      case 6: // Address register indirect with index and byte displacement
         if (size == BYTE || size == WORD)
            this->currentTicks += 10;
         else
            this->currentTicks += 14;

         tmpRegister = (mode_register & 0x7);
         if ((tmpRegister == 7) && (this->registerData[SR_INDEX] & S_FLAG))
            tmpRegister = SSP_INDEX;
         else
            tmpRegister += A0_INDEX;

         address = this->registerData[tmpRegister];

         if ((status = this->peek(this->registerData[PC_INDEX], extend_word, WORD)) != EXECUTE_OK) {
            this->registerData[PC_INDEX] += 2;
            return status;
         } else {
            this->registerData[PC_INDEX] += 2;
         }

         address += this->signExtend(extend_word & 0xFF, BYTE); // Add byte displacement
         if (trace)
            traceRecord += "$" +
                           QString::number(extend_word & 0xFF, 16).leftJustified(2, '0') +
                           "(" +
                           this->registerInfo[tmpRegister].name +
                           ",";

         // Get register number
         if (extend_word & 0x8000) {
            tmpRegister = ((extend_word >> 12) & 0x7);
            if ((tmpRegister == 7) && (this->registerData[SR_INDEX] & S_FLAG))
               tmpRegister = SSP_INDEX;
            else
               tmpRegister += A0_INDEX;
         } else {
            tmpRegister = ((extend_word >> 12) & 0x7) + D0_INDEX;
         }

         if (extend_word & 0x0800)
            address += this->registerData[tmpRegister];
         else
            address += this->signExtend(this->registerData[tmpRegister], WORD);

         if (trace) {
            traceRecord += this->registerInfo[tmpRegister].name;
            if (extend_word & 0x0800)
               traceRecord += ".L)";
            else
               traceRecord += ".W)";
         }

         in_register = 0;
         return EXECUTE_OK;

      case 7:
         switch (mode_register & 0x7) {
            case 0: // Absolute short address
               if (size == BYTE || size == WORD)
                  this->currentTicks += 8;
               else
                  this->currentTicks += 12;

               if ((status = this->peek(this->registerData[PC_INDEX], extend_word, WORD)) != EXECUTE_OK) {
                  this->registerData[PC_INDEX] += 2;
                  return status;
               } else {
                  this->registerData[PC_INDEX] += 2;
               }

               address = this->signExtend(extend_word, WORD);

               if(trace)
                  traceRecord += QString::number(extend_word, 16).rightJustified(4, '0') +
                                 ".W";

               in_register = 0;
               return EXECUTE_OK;

            case 1: // Absolute long address
               if (size == BYTE || size == WORD)
                  this->currentTicks += 12;
               else
                  this->currentTicks += 16;

               if ((status = this->peek(this->registerData[PC_INDEX], extend_word, LONG)) != EXECUTE_OK) {
                  this->registerData[PC_INDEX] += 4;
                  return status;
               } else {
                  this->registerData[PC_INDEX] += 4;
               }

               address = extend_word;

               if (trace)
                  traceRecord += QString::number(extend_word, 16).rightJustified(8, '0') +
                                 ".L";

               in_register = 0;
               return EXECUTE_OK;

            case 2: // Program counter with displacement
               if (size == BYTE || size == WORD)
                  this->currentTicks += 8;
               else
                  this->currentTicks += 12;

               address = this->registerData[PC_INDEX];
               if ((status = this->peek(this->registerData[PC_INDEX], extend_word, WORD)) != EXECUTE_OK) {
                  this->registerData[PC_INDEX] += 2;
                  return status;
               } else {
                  this->registerData[PC_INDEX] += 2;
               }

               address += this->signExtend(extend_word, WORD);

               if (trace)
                  traceRecord += "($" +
                                 QString::number(extend_word, 16).rightJustified(4, '0') +
                                 ",PC)";

               in_register = 0;
               return EXECUTE_OK;

            case 3: // Program counter with index and byte displacement
               if (size == BYTE || size == WORD)
                  this->currentTicks += 10;
               else
                  this->currentTicks += 14;

               address = this->registerData[PC_INDEX];
               if ((status = this->peek(this->registerData[PC_INDEX], extend_word, WORD)) != EXECUTE_OK) {
                  this->registerData[PC_INDEX] += 2;
                  return status;
               } else {
                  this->registerData[PC_INDEX] += 2;
               }

               address += this->signExtend(extend_word & 0xff, BYTE); // byte displacement

               if (trace)
                  traceRecord += "$" +
                                 QString::number(extend_word, 16).rightJustified(2, '0') +
                                 " (PC,";

               // Get register number
               if (extend_word & 0x8000) {
                  tmpRegister = ((extend_word >> 12) & 0x7);
                  if ((tmpRegister == 7) && (this->registerData[SR_INDEX] & S_FLAG))
                     tmpRegister = SSP_INDEX;
                  else
                     tmpRegister += A0_INDEX;
               } else {
                  tmpRegister = ((extend_word >> 12) & 0x7) + D0_INDEX;
               }

               if (extend_word & 0x0800)
                  address += this->registerData[tmpRegister];
               else
                  address += this->signExtend(this->registerData[tmpRegister], WORD);

               if (trace) {
                  traceRecord += this->registerInfo[tmpRegister].name;
                  if (extend_word & 0x0800)
                     traceRecord += ".L";
                  else
                     traceRecord += ".W";
               }

               in_register = 0;
               return EXECUTE_OK;

            case 4: // Immediate data
               if (size == BYTE || size == WORD)
                  this->currentTicks += 4;
               else
                  this->currentTicks += 8;

               address = this->registerData[PC_INDEX];

               if (size == BYTE)
                  ++address;

               if ((size == BYTE) || (size == WORD))
                  this->registerData[PC_INDEX] += 2;
               else
                  this->registerData[PC_INDEX] += 4;

               if (trace) {
                  // Fetch the immediate data
                  if ((status = this->peek(address, extend_word, size)) != EXECUTE_OK) {
                     traceRecord += "#$INVALID";
                  } else {
                     traceRecord += "#$";
                     switch (size) {
                        case BYTE:
                           traceRecord += QString::number(extend_word, 16).rightJustified(2, '0');
                           break;

                        case WORD:
                           traceRecord += QString::number(extend_word, 16).rightJustified(4, '0');
                           break;

                        case LONG:
                           traceRecord += QString::number(extend_word, 16).rightJustified(8, '0');
                           break;
                     }
                  }
               }

               in_register = 0;
               return EXECUTE_OK;
         }
   }

   return EXECUTE_ILLEGAL_INSTRUCTION;
}

unsigned int Motorola68000Private::signExtend(unsigned int value, int size)
{
   switch (size) {
      case BYTE:
         if (value & 0x80)
            return ((value & 0xff) | 0xffffff00);
         else
            return (value & 0xff);

      case WORD:
         if (value & 0x8000)
            return ((value & 0xffff) | 0xffff0000);
         else
            return (value & 0xffff);

      default: // LONG
         return (value & 0xffffffff);
   }
}

void Motorola68000Private::setConditionCodes(unsigned int src, unsigned int dest, unsigned int result, int size, int operation, int mask)
{
   int S, D, R;

   switch (size) {
      case BYTE:
         S = (src >> 7) & 1;
         D = (dest >> 7) & 1;
         R = (result >> 7) & 1;
         result = result & 0xff;
         break;

      case WORD:
         S = (src >> 15) & 1;
         D = (dest >> 15) & 1;
         R = (result >> 15) & 1;
         result = result & 0xffff;
         break;

      case LONG:
         S = (src >> 31) & 1;
         D = (dest >> 31) & 1;
         R = (result >> 31) & 1;
         result = result & 0xffffffff;
         break;

      default:
         S = D = R = 0;
   }

   if (mask & C_FLAG) {
      if (operation == ADDITION) {
         if ((S && D) || (!R && D) || (S && !R))
            this->registerData[SR_INDEX] |= C_FLAG;
         else
            this->registerData[SR_INDEX] &= ~C_FLAG;
      } else if (operation == SUBTRACTION) {
         if ((S && !D) || (R && !D) || (S && R))
            this->registerData[SR_INDEX] |= C_FLAG;
         else
            this->registerData[SR_INDEX] &= ~C_FLAG;
      } else {
         this->registerData[SR_INDEX] &= ~C_FLAG;
      }
   }

   if (mask & V_FLAG) {
      if (operation == ADDITION) {
         if ((S && D && !R) || (!S && !D && R))
            this->registerData[SR_INDEX] |= V_FLAG;
         else
            this->registerData[SR_INDEX] &= ~V_FLAG;
      } else if (operation == SUBTRACTION) {
         if ((!S && D && !R) || (S && !D && R))
            this->registerData[SR_INDEX] |= V_FLAG;
         else
            this->registerData[SR_INDEX] &= ~V_FLAG;
      } else {
         this->registerData[SR_INDEX] &= ~V_FLAG;
      }
   }

   if (mask & Z_FLAG) {
      if (!result)
         this->registerData[SR_INDEX] |= Z_FLAG;
      else
         this->registerData[SR_INDEX] &= ~Z_FLAG;
   }

   if (mask & N_FLAG) {
      if (R)
         this->registerData[SR_INDEX] |= N_FLAG;
      else
         this->registerData[SR_INDEX] &= ~N_FLAG;
   }

   if (mask & X_FLAG) {
      if (operation == ADDITION) {
         if ((S && D) || (!R && D) || (S && !R))
            this->registerData[SR_INDEX] |= X_FLAG;
         else
            this->registerData[SR_INDEX] &= ~X_FLAG;
      } else if (operation == SUBTRACTION) {
         if ((S && !D) || (R && !D) || (S && R))
            this->registerData[SR_INDEX] |= X_FLAG;
         else
            this->registerData[SR_INDEX] &= ~X_FLAG;
      } else {
         this->registerData[SR_INDEX] &= ~X_FLAG;
      }
   }
}

int Motorola68000Private::checkConditionCodes(int code, QString& traceRecord, int trace)
{
   int branch = 0;
   Register sr;

   sr = this->registerData[SR_INDEX];
   switch (code) {
      case 4:
         branch = !(sr & C_FLAG);
         if (trace)
            traceRecord += "CC";
         break;

      case 5:
         branch = (sr & C_FLAG);
         if (trace)
            traceRecord += "CS";
         break;

      case 7:
         branch = (sr & Z_FLAG);
         if (trace)
            traceRecord += "EQ";
         break;

      case 1:
         branch = 0;
         if (trace)
            traceRecord += "F";
         break;

      case 12:
         branch = ((sr & N_FLAG) &&
                   (sr & V_FLAG)) ||
                  (!(sr & N_FLAG) &&
                   !(sr & V_FLAG));
         if (trace)
            traceRecord += "GE";

      case 14:
         branch = ((sr & N_FLAG) &&
                   (sr & V_FLAG) &&
                   !(sr & Z_FLAG)) ||
                  (!(sr & N_FLAG) &&
                   !(sr & V_FLAG) &&
                   !(sr & Z_FLAG));
         if (trace)
            traceRecord += "GT";
         break;

      case 2:
         branch = (!(sr & C_FLAG) &&
                   !(sr & Z_FLAG));
         if (trace)
            traceRecord += "HI";
         break;

      case 15:
         branch = (sr & Z_FLAG) ||
                  ((sr & N_FLAG) &&
                   !(sr & C_FLAG)) ||
                  (!(sr & N_FLAG) &&
                   (sr & V_FLAG));
         if (trace)
            traceRecord += "LE";
         break;

      case 3:
         branch = (sr & C_FLAG) || (sr & Z_FLAG);
         if (trace)
            traceRecord += "LS";
         break;

      case 13:
         branch = ((sr & N_FLAG) &&
                   !(sr & V_FLAG)) ||
                  (!(sr & N_FLAG) &&
                   (sr & V_FLAG));
         if (trace)
            traceRecord += "LT";
         break;

      case 11:
         branch = (sr & N_FLAG);
         if (trace)
            traceRecord += "MI";
         break;

      case 6:
         branch = !(sr & Z_FLAG);
         if (trace)
            traceRecord += "NE";
         break;

      case 10:
         branch = !(sr & N_FLAG);
         if (trace)
            traceRecord += "PL";

      case 0:
         branch = 1;
         if (trace)
            traceRecord += "T";
         break;

      case 8:
         branch = !(sr & V_FLAG);
         if (trace)
            traceRecord += "VC";
         break;

      case 9:
         branch = (sr & V_FLAG);
         if (trace)
            traceRecord += "VS";
         break;
   }

   return branch;
}

int Motorola68000Private::processException(int vector)
{
   int status;

   // Push the PC and SR onto supervisor stack
   if ((status = this->push(SSP_INDEX,
                            this->registerData[PC_INDEX],
                            LONG)) != EXECUTE_OK) {
      return status;
   }

   if ((status = this->push(SSP_INDEX,
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

int Motorola68000Private::handleInterrupts(bool& handleFlag)
{
   int status;
   handleFlag = false;

   // If there are no pending interrupts, return normalle
   if (this->pendingInterrupts.empty())
      return EXECUTE_OK;

   const PendingInterrupt& interrupt = this->pendingInterrupts.top();

   // Also return normally if any of the currently pending interrupts
   // are masked. Note that a check against the top of the queue is
   // sufficient, as the top entry has the highest level.
   const int interrupt_mask = (this->registerData[SR_INDEX] & 0x0700) >> 8;
   if (interrupt.level < interrupt_mask && interrupt.level != 7)
      return EXECUTE_OK;

   // Save a copy of the current SR so it can be stacked for entry
   Register tmpSR = this->registerData[SR_INDEX];

   // Set the interrupt mask in SR
   this->registerData[SR_INDEX] &= 0x0000f8ff;
   this->registerData[SR_INDEX] |= (interrupt.level << 8);

   // Change to supervisor mode and clear the trace mode
   this->registerData[SR_INDEX] |= S_FLAG;
   this->registerData[SR_INDEX] &= ~T_FLAG;

   // Interrupt has occured, so push the PC and the SR
   if ((status = this->push(SSP_INDEX, this->registerData[PC_INDEX], LONG)) != EXECUTE_OK)
      return status;

   if ((status = this->push(SSP_INDEX, tmpSR, WORD)) != EXECUTE_OK)
      return status;

   // Get the vector number by acknowledging to the device
   int vector = interrupt.device->interruptAcknowledge(interrupt.level);
   if (vector == Device::AUTOVECTOR_INTERRUPT)
      vector = 24 + interrupt.level;
   else if (vector == Device::SPURIOUS_INTERRUPT)
      vector = 24;

   // Get the interrupt service routine's address
   quint32 serviceAddress;
   if ((status = this->peek(vector * 4, serviceAddress, LONG)) != EXECUTE_OK)
      return status;

   // Change the program counter to the service routine's address
   this->setRegister(PC_INDEX, serviceAddress, LONG);

   // Indicate that an interrupt was serviced and remove it from
   // the queue of pending interrupts
   handleFlag = true;
   this->pendingInterrupts.pop();

   return EXECUTE_OK;
}

// ABCD Instruction
int Motorola68000Private::ExecuteABCD(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ABCD (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteADD(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag;
   quint32 ea_address, register_number;
   unsigned int result, ea_data;
   QString ea_description;

   size = (opcode & 0x00c0) >> 6;

   // Get the <ea> data address
   if ((status = this->computeEffectiveAddress(ea_address, in_register_flag, ea_description, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the <ea> data
   if (in_register_flag)
      ea_data = this->registerData[ea_address];
   else if ((status = this->peek(ea_address, ea_data, size)) != EXECUTE_OK)
      return status;

   // Get the register number
   register_number =D0_INDEX + ((opcode & 0x0e00) >> 9);

   if (trace) {
      traceRecord += "ADD";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   if (opcode & 0x0100) {
      if (trace)
         traceRecord += this->registerInfo[register_number].name +
                        ","+
                        ea_description;

      result = this->registerData[register_number] + ea_data;
      this->setConditionCodes(this->registerData[register_number], ea_data, result, size,
                              ADDITION, C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
      if ((status = this->poke(ea_address, result, size)) != EXECUTE_OK)
         return status;
   } else {
      if (trace)
         traceRecord += ea_description +
                        "," +
                        this->registerInfo[register_number].name;

      result = ea_data + this->registerData[register_number];

      this->setConditionCodes(ea_data, this->registerData[register_number], result, size,
                              ADDITION, C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
      this->setRegister(register_number, result, size);
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteADDA(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag;
   quint32 ea_address, register_number;
   unsigned int result, ea_data;
   QString ea_description;

   if (opcode & 0x100)
      size = LONG;
   else
      size = WORD;

   // Get the <ea> data address
   if ((status = this->computeEffectiveAddress(ea_address, in_register_flag, ea_description,
                                               opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the <ea> data
   if (in_register_flag)
      ea_data = this->registerData[ea_address];
   else if ((status = this->peek(ea_address, ea_data, size)) != EXECUTE_OK)
      return status;

   ea_data = this->signExtend(ea_data, size);

   // Get the register number
   register_number = A0_INDEX + ((opcode & 0x0e00) >> 9);

   // Adjust register_number if it's A7 and we're in supervisor mode
   if ((register_number == USP_INDEX) && (this->registerData[SR_INDEX] & S_FLAG))
      register_number = SSP_INDEX;

   result = ea_data + this->registerData[register_number];
   this->setRegister(register_number, result, LONG);

   if (trace) {
      traceRecord += "ADDA";
      if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
      traceRecord += ea_address +
                     "," +
                     this->registerInfo[register_number].name;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteADDI(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register;
   quint32 dest_addr, src_addr;
   unsigned int result, src, dest;

   size = (opcode & 0x00c0) >> 6;

   if (trace) {
      traceRecord += "ADDI";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   // Get the immediate data pointer
   if ((status = this->computeEffectiveAddress(src_addr, in_register, traceRecord, 0x3c, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the immediate data
   if ((status = this->peek(src_addr, src, size)) != EXECUTE_OK)
      return status;

   if (trace)
      traceRecord += ",";

   // Get the destination data pointer
   if ((status = this->computeEffectiveAddress(dest_addr, in_register, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   if (in_register) {
      dest = this->registerData[dest_addr];
      result = dest + src;

      this->setConditionCodes(src, dest, result, size, ADDITION,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
      this->setRegister(dest_addr, result, size);
   } else {
      if ((status = this->peek(dest_addr, dest, size)) != EXECUTE_OK)
         return status;

      result = dest + src;

      this->setConditionCodes(src, dest, result, size, ADDITION,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
      if ((status = this->poke(dest_addr, result, size)) != EXECUTE_OK)
         return status;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteADDQ(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag;
   quint32 ea_address, immediate_data;
   unsigned int result, ea_data;

   size = (opcode & 0x00c0) >> 6;

   // Get the immediate data out of the opcode
   if ((immediate_data = (opcode & 0x0e00) >> 9) == 0)
      immediate_data = 0;

   if (trace) {
      traceRecord += "ADDQ";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";

      traceRecord += "#$" +
                     QString::number(immediate_data, 16) +
                     ",";
   }

   // Get the <ea> data address
   if ((status = this->computeEffectiveAddress(ea_address, in_register_flag, traceRecord,
                                               opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the <ea> data
   if (in_register_flag)
      ea_data = this->registerData[ea_address];
   else if ((status = this->peek(ea_address, ea_data, size)) != EXECUTE_OK)
      return status;

   result = immediate_data + ea_data;

   if (in_register_flag) {
      if ((ea_address >= A0_INDEX) && (ea_address <= SSP_INDEX))
         this->setRegister(ea_address, result, LONG);
      else {
         this->setConditionCodes(immediate_data, ea_data, result, size, ADDITION,
                                 C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
         this->setRegister(ea_address, result, size);
      }
   } else {
      this->setConditionCodes(immediate_data, ea_data, result, size, ADDITION,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
      if ((status = this->poke(ea_address, result, size)) != EXECUTE_OK)
         return status;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteADDX(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ADDX (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteAND(int opcode, QString& traceRecord, int trace) {
   traceRecord += "AND (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteANDI(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register;
   quint32 dest_addr, src_addr;
   unsigned int result, src, dest;

   size = (opcode & 0x00c0) >> 6;

   if (trace) {
      traceRecord += "ANDI";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   // Get the immediate data pointer
   if ((status = this->computeEffectiveAddress(src_addr, in_register, traceRecord, 0x3c, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the immediate data
   if ((status = this->peek(src_addr, src, size)) != EXECUTE_OK)
      return status;

   if (trace)
      traceRecord += ",";

   // Get the destination data pointer
   if ((status = this->computeEffectiveAddress(dest_addr, in_register, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   if (in_register) {
      dest = this->registerData[dest_addr];
      result = dest & src;

      this->setConditionCodes(src, dest, result, size, OTHER,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG);
      this->setRegister(dest_addr, result, size);
   } else {
      if ((status = this->peek(dest_addr, dest, size)) != EXECUTE_OK)
         return status;

      result = dest & src;

      this->setConditionCodes(src, dest, result, size, OTHER,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG);

      if ((status = this->poke(dest_addr, result, size)) != EXECUTE_OK)
         return status;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteANDItoCCR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ANDI to CCR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteANDItoSR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ANDI to SR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteASL(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag, shift_count;
   quint32 address;
   unsigned int data;

   size = (opcode & 0x00c0) >> 6;

   if (trace)
      traceRecord += "ASL";

   // Check to see if this is a memory or register rotate
   if (size == 3) {
      size = WORD; // Memor always rotates a word

      if (trace)
         traceRecord += ".W ";

      // Get the address
      if ((status = this->computeEffectiveAddress(address, in_register_flag, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
         return status;

      // Fetch the data
      if ((status = this->peek(address, data, size)) != EXECUTE_OK)
         return status;

      // Shift the data to the left by one bit.
      data = data << 1;

      // Store the shifted data
      if ((status = this->poke(address, data, size)) != EXECUTE_OK)
         return status;

      this->setConditionCodes(0, 0, data, size, OTHER,
                              N_FLAG | Z_FLAG | X_FLAG | C_FLAG | V_FLAG);

      if (data & 0x00010000) {
         this->registerData[SR_INDEX] |= C_FLAG;
         this->registerData[SR_INDEX] |= X_FLAG;
      }

      if (!((data & 0x000180000) == 0x00180000) &&
          !((data & 0x000180000) == 0x0000000))
         this->registerData[SR_INDEX] |= V_FLAG;
   } else {
      if (trace) {
         if (size == BYTE)
            traceRecord += ".B ";
         else if (size == WORD)
            traceRecord += ".W ";
         else
            traceRecord += ".L ";
      }

      // Compute the shift count
      if (opcode & 32) {
         shift_count = this->registerData[D0_INDEX + ((opcode & 0x0e00) >> 9)] & 0x3f;
         if (trace)
            traceRecord += this->registerInfo[D0_INDEX + ((opcode & 0x0e00) >> 9)].name;
      } else {
         if ((shift_count = (opcode & 0x0e00) >> 9) == 0)
            shift_count = 8;

         if (trace)
            traceRecord += "#$" +
                           QString::number(shift_count, 16);
      }

      if (trace)
         traceRecord += "," +
                        this->registerInfo[D0_INDEX + (opcode & 7)].name;

      unsigned int carry = 0, overflow = 0;
      unsigned int carry_mask, overflow_mask;

      // Setup MSB
      if (size == BYTE) {
         carry_mask = 0x80;
         overflow_mask = 0xc0;
      } else if (size == WORD) {
         carry_mask = 0x8000;
         overflow_mask = 0xc000;
      } else {
         carry_mask    = 0x80000000;
         overflow_mask = 0xc0000000;
      }

      // Perform the shift on the data
      data = this->registerData[D0_INDEX + (opcode & 7)];
      for (int t = 0; t < shift_count; ++t) {
         carry = data & carry_mask;
         if (!((data & overflow_mask) == overflow_mask) &&
             !((data & overflow_mask) == 0))
            overflow_mask |= 1;

         data = data << 1;
      }

      this->setRegister(D0_INDEX + (opcode & 7), data, size);
      this->setConditionCodes(0, 0, data, size, OTHER,
                              N_FLAG | Z_FLAG | X_FLAG | C_FLAG | V_FLAG);

      if (overflow)
         this->registerData[SR_INDEX] |= V_FLAG;
      if (carry) {
         this->registerData[SR_INDEX] |= C_FLAG;
         this->registerData[SR_INDEX] |= X_FLAG;
      }
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteASR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ASR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteBRA(int opcode, QString& traceRecord, int trace) {
   unsigned int displacement;
   int status;

   if (trace)
      traceRecord += "BRA";

   this->currentTicks += 10;

   // Compute displacement
   if ((displacement = opcode & 0xff) == 0) {
      // Fetch the word displacement data
      status = this->peek(this->registerData[PC_INDEX], displacement, WORD);
      if (status != EXECUTE_OK)
         return status;

      displacement = this->signExtend(displacement, WORD);

      if (trace)
         traceRecord += ".W $" +
                        QString::number(displacement, 16).rightJustified(4, '0');
   } else {
      displacement = this->signExtend(displacement, BYTE);
      if (trace)
         traceRecord += ".B $" +
                        QString::number(displacement, 16).rightJustified(2, '0');
   }

   this->setRegister(PC_INDEX, this->registerData[PC_INDEX] + displacement, LONG);
   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteBREAK(int opcode, QString& traceRecord, int trace) {
   traceRecord += "BREAK (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteBSR(int opcode, QString& traceRecord, int trace) {
   unsigned int displacement;
   int status;
   quint32 address;

   if (trace)
      traceRecord += "BSR";

   this->currentTicks += 18;

   // Compute the displacement
   if ((displacement = opcode & 0xff) == 0) {
      // Fetch the word displacement data
      status = this->peek(this->registerData[PC_INDEX], displacement, WORD);
      if (status != EXECUTE_OK)
         return status;

      displacement = this->signExtend(displacement, WORD);

      if (trace)
         traceRecord += ".W $" +
                        QString::number(displacement, 16).rightJustified(4, '0');
   } else {
      displacement = this->signExtend(displacement, BYTE);
      if (trace)
         traceRecord += ".B $" +
                        QString::number(displacement, 16).rightJustified(2, '0');
   }

   // Push the PC onto the stack
   if (this->registerData[SR_INDEX] & S_FLAG) {
      this->setRegister(SSP_INDEX, this->registerData[SSP_INDEX] - 4, LONG);
      address = this->registerData[SSP_INDEX];
   } else {
      this->setRegister(USP_INDEX, this->registerData[USP_INDEX] - 4, LONG);
      address = this->registerData[USP_INDEX];
   }

   if ((opcode & 0xff) == 0) {
      if ((status = this->poke(address, this->registerData[PC_INDEX] + 2, LONG)) != EXECUTE_OK)
         return status;
   } else {
      if ((status = this->poke(address, this->registerData[PC_INDEX], LONG)) != EXECUTE_OK)
         return status;
   }

   this->setRegister(PC_INDEX, this->registerData[PC_INDEX] + displacement, LONG);

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteBCC(int opcode, QString& traceRecord, int trace) {
   unsigned int displacement, branch;
   int status, size;

   if ((displacement = opcode & 0xff) == 0) {
      // Fetch the word displacement data
      status = this->peek(this->registerData[PC_INDEX], displacement, WORD);
      if (status != EXECUTE_OK)
         return status;
   }

   if (trace)
      traceRecord += "B";

   branch = this->checkConditionCodes((opcode & 0x0f00) >> 8, traceRecord, trace);

   if ((opcode & 0xff) == 0) {
      size = WORD;

      if (trace)
         traceRecord += ".W $" +
                        QString::number(displacement, 16).rightJustified(4, '0');

      displacement = this->signExtend(displacement, WORD);
   } else {
      size = BYTE;

      if (trace)
         traceRecord += ".B $" +
                        QString::number(displacement, 16).rightJustified(2, '0');

      displacement = this->signExtend(displacement, BYTE);
   }

   if (branch) {
      this->currentTicks += 10;
      this->setRegister(PC_INDEX, this->registerData[PC_INDEX] + displacement, LONG);
   } else if (size == WORD) {
      if (size == BYTE)
         this->currentTicks += 8;
      else
         this->currentTicks += 12;

      this->setRegister(PC_INDEX, this->registerData[PC_INDEX] + 2, LONG);
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteBit(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag;
   quint32 ea_address, register_number;
   unsigned int ea_data, bit_number;
   QString ea_description;

   this->currentTicks += 2; // TODO Problably incorrect

   // Get the bit number we're supposed to be checking
   if (opcode & 256) {
      register_number = D0_INDEX + ((opcode & 0x0e00) >> 9);
      bit_number = this->registerData[register_number];
      if (trace)
         ea_description = this->registerInfo[register_number].name;
   } else {
      quint32 address;

      // Get the immediate data pointer
      if ((status = this->computeEffectiveAddress(address, in_register_flag,
                                                  ea_description, 0x3c, WORD, trace)) != EXECUTE_OK)
         return status;

      // Fetch the immediate data
      if ((status = this->peek(address, bit_number, WORD)) != EXECUTE_OK)
         return status;
   }

   if (trace)
      ea_description += ",";

   // Determine the size of the operation (BYTE or LONG)
   if ((opcode & 0x38) == 0)
      size = LONG;
   else
      size = BYTE;

   // Get the <ea> data address
   if ((status = this->computeEffectiveAddress(ea_address, in_register_flag, ea_description,
                                               opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the <ea> data
   if (in_register_flag) {
      ea_data = this->registerData[ea_address];
      bit_number = (1 << (bit_number & 0x1F));
   } else {
      if ((status = this->peek(ea_address, ea_data, BYTE)) != EXECUTE_OK)
         return status;

      bit_number = (1 << (bit_number & 0x07));
   }

   // Set the Zero Flag
   if (ea_data & bit_number)
      this->registerData[SR_INDEX] &= ~Z_FLAG;
   else
      this->registerData[SR_INDEX] |= Z_FLAG;

   switch ((opcode & 0x00c0) >> 6) {
      case 0: // BTST
         if (trace)
            traceRecord += "BTST";
         break;

      case 1: // BCHG
         if (trace)
            traceRecord += "BCHG";

         if (ea_data & bit_number)
            ea_data &= ~bit_number;
         else
            ea_data |= bit_number;
         break;

      case 2: // BCLR
         if (trace)
            traceRecord += "BCLR";
         ea_data &= ~bit_number;
         break;

      case 3: // BSET
         if (trace)
            traceRecord += "BSET";
         ea_data |= bit_number;
         break;
   }

   // If it's not BTST then write the result back
   if (((opcode & 0x00c0) >> 6) != 0) {
      if (in_register_flag)
         this->setRegister(ea_address, ea_data, LONG);
      else if ((status = this->poke(ea_address, ea_data, BYTE)) != EXECUTE_OK)
         return status;
   }

   if (trace) {
      if (size == BYTE)
         traceRecord += ".B ";
      else
         traceRecord += ".L ";

      traceRecord += ea_description;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteCHK(int opcode, QString& traceRecord, int trace) {
   traceRecord += "CHK (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCLR(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register;
   quint32 address;

   size = (opcode & 0x00c0) >> 6;

   if (trace) {
      traceRecord += "CLR";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   if ((status = this->computeEffectiveAddress(address, in_register, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   if (in_register) {
      this->setConditionCodes(0, 0, 0, size, OTHER, C_FLAG | V_FLAG | Z_FLAG | N_FLAG);
      this->setRegister(address, 0, size);
   } else {
      this->setConditionCodes(0, 0, 0, size, OTHER, C_FLAG | V_FLAG | Z_FLAG | N_FLAG);
      if ((status = this->poke(address, 0, size)) != EXECUTE_OK)
         return status;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteCMP(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag;
   quint32 ea_address, register_number;
   unsigned int result, ea_data;

   size = (opcode & 0x00c0) >> 0;

   if (trace) {
      traceRecord += "CMP";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   // Get the data address
   if ((status = this->computeEffectiveAddress(ea_address, in_register_flag, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the data
   if (in_register_flag)
      ea_data = this->registerData[ea_address];
   else if ((status = this->peek(ea_address, ea_data, size)) != EXECUTE_OK)
      return status;

   // Get the register number
   register_number = D0_INDEX + ((opcode & 0x0e00) >> 9);

   result = this->registerData[register_number] - ea_data;
   this->setConditionCodes(ea_data, this->registerData[register_number], result, size,
                           SUBTRACTION, C_FLAG | V_FLAG | Z_FLAG | N_FLAG);

   if (trace)
      traceRecord += "," +
                     this->registerInfo[register_number].name;

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteCMPA(int opcode, QString& traceRecord, int trace) {
   traceRecord += "CMPA (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCMPI(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register;
   quint32 dest_addr, src_addr;
   unsigned int result, src, dest;

   size = (opcode & 0x00c0) >> 6;

   if (trace) {
      traceRecord += "CMPI";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   // Get the immediate data pointer
   if ((status = this->computeEffectiveAddress(src_addr, in_register, traceRecord, 0x3c, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the immediate data
   if ((status = this->peek(src_addr, src, size)) != EXECUTE_OK)
      return status;

   if (trace)
      traceRecord += ",";

   // Get the destination data pointer
   if ((status = this->computeEffectiveAddress(dest_addr, in_register, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   if (in_register) {
      dest = this->registerData[dest_addr];
      result = dest - src;

      this->setConditionCodes(src, dest, result, size, SUBTRACTION,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG);
   } else {
      if ((status = this->peek(dest_addr, dest, size)) != EXECUTE_ADDRESS_ERROR)
         return status;

      result = dest - src;

      this->setConditionCodes(src, dest, result, size, SUBTRACTION,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG);
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteCMPM(int opcode, QString& traceRecord, int trace) {
   traceRecord += "CMPM (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteDBCC(int opcode, QString& traceRecord, int trace) {
   unsigned int displacement, register_number, condition_code;
   int status;

   if (trace)
      traceRecord += "DB";

   if ((status = this->peek(this->registerData[PC_INDEX], displacement, WORD)) != EXECUTE_OK)
      return status;

   displacement = this->signExtend(displacement, WORD);

   // Check the condition code
   condition_code = this->checkConditionCodes((opcode & 0xf00) >> 8, traceRecord, trace);

   // Get the register number that we are counting with
   register_number = D0_INDEX + (opcode & 7);

   if (trace)
      traceRecord += ".W " +
                     this->registerInfo[register_number].name +
                     ",$" +
                     QString::number(displacement, 16).rightJustified(4, '0');

   // If condition code is not true then perform decrement and branch
   if (!condition_code) {
      this->setRegister(register_number, this->registerData[register_number] - 1, WORD);
      if ((this->registerData[register_number] & 0xffff) == 0xffff) {
         this->currentTicks += 14;
         this->setRegister(PC_INDEX, this->registerData[PC_INDEX] + 2, LONG);
      } else {
         this->currentTicks += 10;
         this->setRegister(PC_INDEX, this->registerData[PC_INDEX] + displacement, LONG);
      }
   } else {
      this->currentTicks += 12;
      this->setRegister(PC_INDEX, this->registerData[PC_INDEX] + 2, LONG);
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteDIVS(int opcode, QString& traceRecord, int trace) {
   traceRecord += "DIVS (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteDIVU(int opcode, QString& traceRecord, int trace) {
   traceRecord += "DIVU (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEOR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "EOR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEORI(int opcode, QString& traceRecord, int trace) {
   traceRecord += "EORI (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEORItoCCR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "EORI to CCR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEORItoSR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "EORI to SR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEXG(int opcode, QString& traceRecord, int trace) {
   traceRecord += "EXG (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteEXT(int opcode, QString& traceRecord, int trace) {
   traceRecord += "EXT (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteILLEGAL(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ILLEGAL (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteJMP(int opcode, QString& traceRecord, int trace) {
   int status, in_register_flag;
   quint32 address;
   QString ea_description;

   // Get the effective address
   if ((status = this->computeEffectiveAddress(address, in_register_flag, ea_description,
                                               opcode & 0x3f, LONG, trace)) != EXECUTE_OK)
      return status;

   this->setRegister(PC_INDEX, address, LONG);

   if (trace)
      traceRecord += "JMP " + ea_description;

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteJSR(int opcode, QString& traceRecord, int trace) {
   int status, in_register_flag;
   quint32 address, stack_adress;
   QString ea_description;

   if ((status = this->computeEffectiveAddress(address, in_register_flag, ea_description,
                                               opcode & 0x3f, LONG, trace)) != EXECUTE_OK)
      return status;

   if (this->registerData[SR_INDEX] & S_FLAG) {
      this->setRegister(SSP_INDEX, this->registerData[SSP_INDEX] - 4, LONG);
      stack_adress = this->registerData[SSP_INDEX];
   } else {
      this->setRegister(USP_INDEX, this->registerData[USP_INDEX] - 4, LONG);
      stack_adress = this->registerData[USP_INDEX];
   }

   if ((status = this->poke(stack_adress, this->registerData[PC_INDEX], LONG)) != EXECUTE_OK)
      return status;

   this->setRegister(PC_INDEX, address, LONG);

   if (trace)
      traceRecord += "JSR " +
                     ea_description;

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteLEA(int opcode, QString& traceRecord, int trace) {
   int status, in_register_flag;
   quint32 address, register_number;

   if (trace)
      traceRecord += "LEA.L $";

   if ((status = this->computeEffectiveAddress(address, in_register_flag, traceRecord, opcode & 0x3f, LONG, trace)) != EXECUTE_OK)
      return status;

   // Get the address register number
   register_number = A0_INDEX + ((opcode & 0x0e00) >> 9);

   // Adjust register_numer if it's A7 and we're in supervisor mode
   if ((register_number == USP_INDEX) && (this->registerData[SR_INDEX] & S_FLAG))
      register_number = SSP_INDEX;

   this->setRegister(register_number, address, LONG);

   if (trace)
      traceRecord += "," +
                     this->registerInfo[register_number].name;

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteLINK(int opcode, QString& traceRecord, int trace) {
   traceRecord += "LINK (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteLINKWORD(int opcode, QString& traceRecord, int trace) {
   traceRecord += "LINKWORD (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteLSL(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag, shift_count;
   quint32 address;
   unsigned int data;

   size = (opcode & 0x00c0) >> 6;

   if (trace)
      traceRecord += "LSL";

   // Check to see if this is a memory or register rotate
   if (size == 3) {
      size = WORD; // Memor always rotates a word

      if (trace)
         traceRecord += ".W ";

      // Get the address
      if ((status = this->computeEffectiveAddress(address, in_register_flag, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
         return status;

      // Fetch the data
      if ((status = this->peek(address, data, size)) != EXECUTE_OK)
         return status;

      // Shift the data to the left by one bit.
      data = data << 1;

      // Store the shifted data
      if ((status = this->poke(address, data, size)) != EXECUTE_OK)
         return status;

      this->setConditionCodes(0, 0, data, size, OTHER,
                              N_FLAG | Z_FLAG | X_FLAG | C_FLAG | V_FLAG);

      if (data & 0x00010000) {
         this->registerData[SR_INDEX] |= C_FLAG;
         this->registerData[SR_INDEX] |= X_FLAG;
      }
   } else {
      if (trace) {
         if (size == BYTE)
            traceRecord += ".B ";
         else if (size == WORD)
            traceRecord += ".W ";
         else
            traceRecord += ".L ";
      }

      // Compute the shift count
      if (opcode & 32) {
         shift_count = this->registerData[D0_INDEX + ((opcode & 0x0e00) >> 9)] & 0x3f;
         if (trace)
            traceRecord += this->registerInfo[D0_INDEX + ((opcode & 0x0e00) >> 9)].name;
      } else {
         if ((shift_count = (opcode & 0x0e00) >> 9) == 0)
            shift_count = 8;

         if (trace)
            traceRecord += "#$" +
                           QString::number(shift_count, 16);
      }

      if (trace)
         traceRecord += "," +
                        this->registerInfo[D0_INDEX + (opcode & 7)].name;

      unsigned int carry = 0;
      unsigned int carry_mask;

      // Setup MSB
      if (size == BYTE)
         carry_mask = 0x80;
      else if (size == WORD)
         carry_mask = 0x8000;
      else
         carry_mask = 0x80000000;

      // Perform the shift on the data
      data = this->registerData[D0_INDEX + (opcode & 7)];
      for (int t = 0; t < shift_count; ++t) {
         carry = data & carry_mask;
         data = data << 1;
      }

      this->setRegister(D0_INDEX + (opcode & 7), data, size);
      this->setConditionCodes(0, 0, data, size, OTHER,
                              N_FLAG | Z_FLAG | X_FLAG | C_FLAG | V_FLAG);

      if (carry) {
         this->registerData[SR_INDEX] |= C_FLAG;
         this->registerData[SR_INDEX] |= X_FLAG;
      }
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteLSR(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag, shift_count;
   quint32 address;
   unsigned int data;

   size = (opcode & 0x00c0) >> 6;

   if (trace)
      traceRecord += "LSR";

   // Check to see if this is a memory or register shift
   if (size == 3) {
      size = WORD; // Memory always shifts a word

      if (trace)
         traceRecord += ".W ";

      if ((status = this->computeEffectiveAddress(address, in_register_flag,
                                                  traceRecord, opcode & 0x3f, size,
                                                  trace)) != EXECUTE_OK)
         return status;

      // Fetch the data
      if ((status = this->peek(address, data, size)) != EXECUTE_OK)
         return status;

      this->setConditionCodes(0, 0, (data >> 1) & 0x7fff, size, OTHER,
                              N_FLAG | Z_FLAG | X_FLAG | C_FLAG | V_FLAG);
      if (data & 0x0001) {
         this->registerData[SR_INDEX] |= C_FLAG;
         this->registerData[SR_INDEX] |= X_FLAG;
      }

      // Shift the data to the right by one bit
      data = (data >> 1) & 0x7fff;

      if ((status = this->poke(address, data, size)) != EXECUTE_OK)
         return status;
   } else {
      if (trace) {
         switch (size) {
            case BYTE:
               traceRecord += ".B ";
               break;

            case WORD:
               traceRecord += ".W ";
               break;

            default:
               traceRecord += ".L ";
               break;
         }
      }

      // Compute the shift count
      if (opcode & 32) {
         shift_count = this->registerData[D0_INDEX + ((opcode & 0x0e00) >> 9)] & 0x3f;
         if (trace)
            traceRecord += this->registerInfo[D0_INDEX + ((opcode & 0x0e00) >> 9)].name;
      } else {
         if ((shift_count = (opcode & 0x0e00) >> 9) == 0)
            shift_count = 8;

         if (trace)
            traceRecord += "#$" +
                           QString::number(shift_count);
      }


      if (trace)
         traceRecord += ","+
                        this->registerInfo[D0_INDEX + (opcode & 0x7)].name;

      unsigned int carry= 0, clear_mask;

      // Setup masks
      if (size == BYTE)
         clear_mask = 0x7f;
      else if (size == WORD)
         clear_mask = 0x7fff;
      else
         clear_mask = 0x7fffffff;

      // Perform the shift on the data
      data = this->registerData[D0_INDEX + (opcode & 0x7)];
      for (int t=0; t < shift_count; ++t) {
         carry = data & 0x1;
         data = (data >> 1) & clear_mask;
      }

      this->setRegister(D0_INDEX + (opcode & 0x7), data, size);
      this->setConditionCodes(0, 0, data, size, OTHER,
                              N_FLAG | Z_FLAG | X_FLAG | C_FLAG | V_FLAG);

      if (carry) {
         this->registerData[SR_INDEX] |= C_FLAG;
         this->registerData[SR_INDEX] |= X_FLAG;
      }
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteMOVE(int opcode, QString& traceRecord, int trace) {
   int status, in_register_flag, size;
   quint32 src_address, dest_address;
   unsigned int src;

   this->currentTicks += 4;

   if (trace)
      traceRecord += "MOVE";

   switch ((opcode & 0x3000) >> 12) {
      case 1:
         size = BYTE;
         if (trace)
            traceRecord += ".B ";
         break;

      case 3:
         size = WORD;
         if (trace)
            traceRecord += ".W ";
         break;

      case 2:
         size = LONG;
         if (trace)
            traceRecord += ".L ";
         break;

      default:
         return EXECUTE_ADDRESS_ERROR;
   }

   // Get the source effective address
   if ((status = this->computeEffectiveAddress(src_address, in_register_flag, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   if (in_register_flag)
      src = this->registerData[src_address];
   else if ((status = this->peek(src_address, src, size)) != EXECUTE_OK)
      return status;

   if (trace)
      traceRecord += ",";

   // Get the destination effective address
   if ((status = this->computeEffectiveAddress(dest_address,
                                               in_register_flag,
                                               traceRecord,
                                               ((opcode & 0x01c0) >> 3) | ((opcode & 0x0e00) >> 9),
                                               size,
                                               trace)) != EXECUTE_OK)
      return status;

   if (in_register_flag)
      this->setRegister(dest_address, src, size);
   else if ((status = this->poke(dest_address, src, size)) != EXECUTE_OK)
      return status;

   this->setConditionCodes(0, 0, src, size, OTHER, N_FLAG | Z_FLAG | V_FLAG | C_FLAG);

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteMOVEA(int opcode, QString& traceRecord, int trace) {
   int status, in_register_flag, size;
   quint32 src_address, dest_address;
   unsigned int src;
   QString ea_description;

   switch ((opcode & 0x3000) >> 12) {
      case 3:
         size = WORD;
         break;

      case 2:
         size = LONG;
         break;

      default:
         size = 0;
         break;
   }

   // Get the source effective address
   if ((status = this->computeEffectiveAddress(src_address, in_register_flag, ea_description,
                                               opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   if (in_register_flag)
      src = this->registerData[src_address];
   else if ((status = this->peek(src_address, src, size)) != EXECUTE_OK)
      return status;

   if (size == WORD)
      src = this->signExtend(src, WORD);

   if (trace)
      ea_description += ",";

   // Get the destination effective address
   if ((status = this->computeEffectiveAddress(dest_address, in_register_flag, ea_description,
                                               ((opcode & 0x01c0) >> 3) | ((opcode & 0xe00) >> 9),
                                               LONG, trace)) != EXECUTE_OK)
      return status;

   if (in_register_flag)
      this->setRegister(dest_address, src, LONG);
   else if ((status = this->poke(dest_address, src, LONG)) != EXECUTE_OK)
      return status;

   if (trace) {
      traceRecord += "MOVEA";
      if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";

      traceRecord += ea_description;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteMOVEM(int opcode, QString& traceRecord, int trace) {
   int status, in_register_flag, size;
   quint32 address, offset, reg;
   unsigned int list, data;
   QString ea_description;

   // Determine size and offset
   if (opcode & 64) {
      offset = 4;
      size = LONG;
   } else {
      offset = 2;
      size = WORD;
   }

   if (trace) {
      traceRecord += "MOVEM";
      if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   if ((status = this->peek(this->registerData[PC_INDEX], list, WORD)) != EXECUTE_OK)
      return status;

   this->setRegister(PC_INDEX, this->registerData[PC_INDEX] + 2, LONG);

   // Get the effective address (if this isn't predecrement)
   if ((opcode & 0x38) != 32) {
      if ((status = this->computeEffectiveAddress(address, in_register_flag, ea_description,
                                                  opcode & 0x3f, size, trace)) != EXECUTE_OK)
         return status;
   }

   if ((opcode & 0x38) == 32) { // Predecrement mode
      if ((this->registerData[SR_INDEX] & S_FLAG) &&
          ((A0_INDEX + (opcode & 7)) == USP_INDEX))
         reg = SSP_INDEX;
      else
         reg = A0_INDEX + (opcode & 7);

      address = this->registerData[reg];
      if (trace)
         ea_description += "-("+
                           this->registerInfo[reg].name +
                           ")";

      for (unsigned int t = A0_INDEX + 7;;) {
         if (list & (1 << (A0_INDEX + 7 - t))) {
            if ((this->registerData[SR_INDEX] & S_FLAG) && (t == USP_INDEX))
               reg = SSP_INDEX;
            else
               reg = t;

            address -= offset;
            if ((status = this->poke(address, this->registerData[reg], size)) != EXECUTE_OK)
               return status;

            if (trace)
               traceRecord += this->registerInfo[reg].name + " ";
         }

         if (t == D0_INDEX)
            break;
         else
            t--;
      }

      if (trace)
         traceRecord += ea_description + ",";
   } else { //Postdecrement or control mode
      if (trace)
         traceRecord += ea_description + ",";

      for (unsigned int t = D0_INDEX; t <= A0_INDEX + 7; ++t) {
         if (list & (1 << (t - D0_INDEX))) {
            if ((this->registerData[SR_INDEX] & S_FLAG) && (t == USP_INDEX))
               reg = SSP_INDEX;
            else
               reg = t;

            if (opcode & 1024) {
               if ((status = this->peek(address, data, size)) != EXECUTE_OK)
                  return status;

               this->setRegister(reg, data, size);
            } else {
               if ((status = this->poke(address, this->registerData[reg], size)) != EXECUTE_OK)
                  return status;
            }

            address += offset;

            if (trace) {
               traceRecord += this->registerInfo[reg].name + " ";
            }
         }
      }
   }

   if (((opcode & 0x38) == 32) || ((opcode & 0x38) == 24)) {
      if ((this->registerData[SR_INDEX] & S_FLAG) && ((opcode & 7) == 7))
         this->setRegister(SSP_INDEX, address, LONG);
      else
         this->setRegister(A0_INDEX + (opcode & 7), address, LONG);
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteMOVEP(int opcode, QString& traceRecord, int trace) {
   Register Dn;
   int in_register_flag;
   quint32 src_address, dest_address;
   QString ea_description;
   int status;

   // Get the data register we're working with
   Dn = D0_INDEX + ((opcode & 0x0e00) >> 9);

   switch ((opcode & 0x01c0) >> 6) {
      case 4:
      {
         // Get the source effective address
         if ((status = this->computeEffectiveAddress(src_address, in_register_flag, ea_description,
                                                     (0x20 | (opcode & 0x7)), WORD, trace)) != EXECUTE_OK)
            return status;

         unsigned int b0, b1;

         if ((status = this->peek((src_address + 0), b1, BYTE)) != EXECUTE_OK)
            return status;
         if ((status = this->peek((src_address + 2), b0, BYTE)) != EXECUTE_OK)
            return status;

         this->setRegister(Dn, (b1 << 8) | b0, WORD);

         if (trace)
            traceRecord += "MOVEP.W " +
                           ea_description + ",D" +
                           QString::number(Dn, 16);
         break;
      }

      case 5: // LONG from Memory to Register
      {
         // Get the source effective address
         if ((status = this->computeEffectiveAddress(src_address, in_register_flag, ea_description,
                                                     (0x28 | (opcode & 0x7)), LONG, trace)) != EXECUTE_OK)
            return status;

         unsigned int b0, b1, b2, b3;

         if ((status = this->peek((src_address + 0), b3, BYTE)) != EXECUTE_OK)
            return status;
         if ((status = this->peek((src_address + 2), b2, BYTE)) != EXECUTE_OK)
            return status;
         if ((status = this->peek((src_address + 4), b1, BYTE)) != EXECUTE_OK)
            return status;
         if ((status = this->peek((src_address + 6), b0, BYTE)) != EXECUTE_OK)
            return status;

         this->setRegister(Dn, (b3 << 24) | (b2 << 16) | (b1 << 8) | b0, LONG);

         if (trace)
            traceRecord += "MOVEP.L " +
                           ea_description +
                           ",D" +
                           QString::number(Dn, 16);

         break;
      }

      case 6: // WORD from Register to Memory
      {
         // Get the destination address
         if ((status = this->computeEffectiveAddress(dest_address, in_register_flag, ea_description,
                                                     (0x28 | (opcode & 0x7)), WORD, trace)) != EXECUTE_OK)
            return status;

         unsigned int value = this->registerData[Dn];

         if ((status = this->poke((dest_address + 0), value >> 8, BYTE)) != EXECUTE_OK)
            return status;
         if ((status = this->poke((dest_address + 2), value, BYTE)) != EXECUTE_OK)
            return status;

         if (trace)
            traceRecord += "MOVEP.W D" +
                           QString::number(Dn, 16) +
                           "," + ea_description;

         break;
      }

      case 7: // LONG from Register to Memory
      {
         // Get the destination address
         if ((status = this->computeEffectiveAddress(dest_address, in_register_flag, ea_description,
                                                     (0x28 | (opcode & 0x7)), LONG, trace)) != EXECUTE_OK)
            return status;

         unsigned value = this->registerData[Dn];

         if ((status = this->poke((dest_address + 0), value >> 24, BYTE)) != EXECUTE_OK)
            return status;
         if ((status = this->poke((dest_address + 2), value >> 16, BYTE)) != EXECUTE_OK)
            return status;
         if ((status = this->poke((dest_address + 4), value >> 8, BYTE)) != EXECUTE_OK)
            return status;
         if ((status = this->poke((dest_address + 6), value, BYTE)) != EXECUTE_OK)
            return status;

         if (trace)
            traceRecord += "MOVEP.L D" +
                           QString::number(Dn, 16) +
                           ","+ea_description;
         break;
      }
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteMOVEQ(int opcode, QString& traceRecord, int trace) {
   Register register_number;
   unsigned int data;

   // Get the destination data register number
   register_number = D0_INDEX + ((opcode >> 9) & 0x07);

   // Get the immediate data
   data = this->signExtend((opcode & 0xFF), BYTE);

   this->setConditionCodes(0, 0, data, LONG, OTHER, C_FLAG | V_FLAG | Z_FLAG | N_FLAG);
   this->setRegister(register_number, data, LONG);

   if (trace)
      traceRecord += "MOVEQ.L #$" +
                     QString::number(data, 16).rightJustified(2, '0') +
                     "," +
                     this->registerInfo[register_number].name;

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteMOVEUSP(int opcode, QString& traceRecord, int trace) {
   traceRecord += "MOVEUSP (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEfromSR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "MOVE from SR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEfromCCR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "MOVE from CCR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEfromUSP(int opcode, QString& traceRecord, int trace) {
   traceRecord += "MOVE from USP (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEtoCCR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "MOVE to CCR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMOVEtoSR(int opcode, QString& traceRecord, int trace) {
   int status, in_register;
   quint32 address;
   unsigned int data;
   QString ea_description;

   // Make sure we're in supervisor mode or trap
   if (!(this->registerData[SR_INDEX] & S_FLAG)) {
      this->setRegister(PC_INDEX, this->registerData[PC_INDEX] - 2, LONG);
      if ((status = this->processException(8)) != EXECUTE_OK)
         return status;

      if (trace)
         traceRecord += "Privilege Violation Exception";
      return EXECUTE_PRIVILEGED_OK;
   }

   // Get the destination data pointer
   if ((status = this->computeEffectiveAddress(address, in_register, ea_description,
                                               opcode & 0x3f, WORD, trace)) != EXECUTE_OK)
      return status;

   if (in_register)
      data = this->registerData[address];
   else if ((status = this->peek(address, data, WORD)) != EXECUTE_OK)
      return status;

   this->setRegister(SR_INDEX, data, WORD);

   if (trace)
      traceRecord += "MOVE.W " +
                     ea_description +
                     ",SR";

   return EXECUTE_PRIVILEGED_OK;
}

int Motorola68000Private::ExecuteMOVEtoUSP(int opcode, QString& traceRecord, int trace) {
   int status, register_number;
   QString ea_description;

   // Make sure we're in supervisor mode or trap
   if (!(this->registerData[SR_INDEX] & S_FLAG)) {
      this->setRegister(PC_INDEX, this->registerData[PC_INDEX] - 2, LONG);
      if ((status = this->processException(8)) != EXECUTE_OK)
         return status;

      if (trace)
         traceRecord += "Privilege Violation Exception";
      return EXECUTE_PRIVILEGED_OK;
   }

   // Get the address register index
   if ((opcode & 7) == 7)
      register_number = SSP_INDEX;
   else
      register_number = A0_INDEX + (opcode & 7);

   this->setRegister(USP_INDEX, this->registerData[register_number], LONG);
   if (trace)
      traceRecord += "MOVE.L " +
                     this->registerInfo[register_number].name +
                     ",USP";

   return EXECUTE_PRIVILEGED_OK;
}

int Motorola68000Private::ExecuteMULS(int opcode, QString& traceRecord, int trace) {
   traceRecord += "MULS (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteMULU(int opcode, QString& traceRecord, int trace) {
   traceRecord += "MULU (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteNBCD(int opcode, QString& traceRecord, int trace) {
   traceRecord += "NBCD (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteNEG(int opcode, QString& traceRecord, int trace) {
   traceRecord += "NEG (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteNEGX(int opcode, QString& traceRecord, int trace) {
   traceRecord += "NEGX (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteNOP(int opcode, QString& traceRecord, int trace) {
   Q_UNUSED(opcode);

   this->currentTicks += 4;

   if(trace)
      traceRecord += "NOP";

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteNOT(int opcode, QString& traceRecord, int trace) {
   traceRecord += "NOT (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteOR(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag;
   quint32 ea_address, register_number;
   unsigned int result, ea_data;

   QString ea_record;

   size = (opcode & 0x00c0) >> 6;

   if (trace) {
      traceRecord += "OR";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   // Get the <ea> data address
   if ((status = this->computeEffectiveAddress(ea_address, in_register_flag, ea_record, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the <ea> data
   if (in_register_flag)
      ea_data = this->registerData[ea_address];
   else if ((status = this->peek(ea_address, ea_data, size)) != EXECUTE_OK)
      return status;

   // Get the register number
   register_number = D0_INDEX + ((opcode & 0x0e00) >> 9);

   if (opcode & 0x0100) {
      if (trace)
         traceRecord += this->registerInfo[register_number].name +
                        "," +
                        ea_record;

      result = this->registerData[register_number] | ea_data;

      this->setConditionCodes(this->registerData[register_number], ea_data, result, size,
                              OTHER, V_FLAG | C_FLAG | Z_FLAG | N_FLAG);

      if ((status = this->poke(ea_address, result, size)) != EXECUTE_OK)
         return status;
   } else {
      if (trace)
         traceRecord += ea_record +
                        "," +
                        this->registerInfo[register_number].name;

      result = ea_data | this->registerData[register_number];

      this->setConditionCodes(ea_data, this->registerData[register_number], result, size,
                              OTHER, V_FLAG | C_FLAG | Z_FLAG | N_FLAG);
      this->setRegister(register_number, result, size);
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteORI(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register;
   quint32 dest_addr, src_addr;
   unsigned int result, src, dest;

   size = (opcode & 0x00c0) >> 6;

   if (trace) {
      traceRecord += "ORI";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   // Get the immediate data pointer
   if ((status = this->computeEffectiveAddress(src_addr, in_register, traceRecord, 0x3c, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the immediate data
   if ((status = this->peek(src_addr, src, size)) != EXECUTE_OK)
      return status;

   if (trace)
      traceRecord += ",";

   // Get the destination data pointer
   if ((status = this->computeEffectiveAddress(dest_addr, in_register, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   if (in_register) {
      dest = this->registerData[dest_addr];
      result = dest | src;

      this->setConditionCodes(src, dest, result, size, OTHER, C_FLAG | V_FLAG | Z_FLAG | N_FLAG);
      this->setRegister(dest_addr, result, size);
   } else {
      if ((status = this->peek(dest_addr, dest, size)) != EXECUTE_OK)
         return status;

      result = dest | src;
      this->setConditionCodes(src, dest, result, size, OTHER, C_FLAG | V_FLAG | Z_FLAG | N_FLAG);

      if ((status = this->poke(dest_addr, result, size)) != EXECUTE_OK)
         return status;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteORItoCCR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ORI to CCR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteORItoSR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ORI to SR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecutePEA(int opcode, QString& traceRecord, int trace) {
   traceRecord += "PEA (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteRESET(int opcode, QString& traceRecord, int trace) {
   traceRecord += "RESET (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteROL(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag, shift_count;
   quint32 address;
   unsigned int data;

   size = (opcode & 0x00c0) >> 6;

   if (trace)
      traceRecord += "ROL";

   // Check to see if this is a memory or register rotate
   if (size == 3) {
      size = WORD; // Memor always rotates a word

      if (trace)
         traceRecord += ".W ";

      // Get the address
      if ((status = this->computeEffectiveAddress(address, in_register_flag, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
         return status;

      // Fetch the data
      if ((status = this->peek(address, data, size)) != EXECUTE_OK)
         return status;

      // Rotate the data to the left by one bit
      if (data & 0x8000)
         data = (data << 1) | 0x0001;
      else
         data = (data << 1);

      // Store the shifted data
      if ((status = this->poke(address, data, size)) != EXECUTE_OK)
         return status;

      this->setConditionCodes(0, 0, data, size, OTHER,
                              N_FLAG | Z_FLAG | C_FLAG | V_FLAG);

      if (data & 0x0001)
         this->registerData[SR_INDEX] |= C_FLAG;
   } else {
      if (trace) {
         if (size == BYTE)
            traceRecord += ".B ";
         else if (size == WORD)
            traceRecord += ".W ";
         else
            traceRecord += ".L ";
      }

      // Compute the shift count
      if (opcode & 32) {
         shift_count = this->registerData[D0_INDEX + ((opcode & 0x0e00) >> 9)] & 0x3f;
         if (trace)
            traceRecord += this->registerInfo[D0_INDEX + ((opcode & 0x0e00) >> 9)].name;
      } else {
         if ((shift_count = (opcode & 0x0e00) >> 9) == 0)
            shift_count = 8;

         if (trace)
            traceRecord += "#$" +
                           QString::number(shift_count, 16);
      }

      if (trace)
         traceRecord += "," +
                        this->registerInfo[D0_INDEX + (opcode & 7)].name;

      unsigned int msb;

      // Setup MSB
      if (size == BYTE)
         msb = 0x80;
      else if (size == WORD)
         msb = 0x8000;
      else
         msb = 0x80000000;

      // Perform the shift on the data
      data = this->registerData[D0_INDEX + (opcode & 7)];
      for (int t = 0; t < shift_count; ++t) {
         if (data & msb)
            data = (data << 1) | 1;
         else
            data = (data << 1);
      }

      this->setRegister(D0_INDEX + (opcode & 7), data, size);
      this->setConditionCodes(0, 0, data, size, OTHER,
                              N_FLAG | Z_FLAG | C_FLAG | V_FLAG);

      if (data & 1)
         this->registerData[SR_INDEX] |= C_FLAG;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteROR(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag, shift_count;
   quint32 address;
   unsigned int data;

   size = (opcode & 0x00c0) >> 6;

   if (trace)
      traceRecord += "ROR";

   // Check to see if this is a memory or register rotate
   if (size == 3) {
      size = WORD; // Memor always rotates a word

      if (trace)
         traceRecord += ".W ";

      // Get the address
      if ((status = this->computeEffectiveAddress(address, in_register_flag, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
         return status;

      // Fetch the data
      if ((status = this->peek(address, data, size)) != EXECUTE_OK)
         return status;

      // Rotate the data to the right by one bit
      if (data & 1)
         data = 0x8000 | (data >> 1);
      else
         data = (data >> 1) & 0x7fff;

      // Store the shifted data
      if ((status = this->poke(address, data, size)) != EXECUTE_OK)
         return status;

      this->setConditionCodes(0, 0, data, size, OTHER,
                              N_FLAG | Z_FLAG | C_FLAG | V_FLAG);

      if (data & 0x8000)
         this->registerData[SR_INDEX] |= C_FLAG;
   } else {
      if (trace) {
         if (size == BYTE)
            traceRecord += ".B ";
         else if (size == WORD)
            traceRecord += ".W ";
         else
            traceRecord += ".L ";
      }

      // Compute the shift count
      if (opcode & 32) {
         shift_count = this->registerData[D0_INDEX + ((opcode & 0x0e00) >> 9)] & 0x3f;
         if (trace)
            traceRecord += this->registerInfo[D0_INDEX + ((opcode & 0x0e00) >> 9)].name;
      } else {
         if ((shift_count = (opcode & 0x0e00) >> 9) == 0)
            shift_count = 8;

         if (trace)
            traceRecord += "#$" +
                           QString::number(shift_count, 16);
      }

      if (trace)
         traceRecord += "," +
                        this->registerInfo[D0_INDEX + (opcode & 7)].name;

      unsigned int msb;

      // Setup MSB
      if (size == BYTE)
         msb = 0x80;
      else if (size == WORD)
         msb = 0x8000;
      else
         msb = 0x80000000;

      // Perform the shift on the data
      data = this->registerData[D0_INDEX + (opcode & 7)];
      for (int t = 0; t < shift_count; ++t) {
         if (data & msb)
            data = msb | (data >> 1);
         else
            data = (data >> 1) & ~msb;
      }

      this->setRegister(D0_INDEX + (opcode & 7), data, size);
      this->setConditionCodes(0, 0, data, size, OTHER,
                              N_FLAG | Z_FLAG | C_FLAG | V_FLAG);

      if (data & msb)
         this->registerData[SR_INDEX] |= C_FLAG;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteROXL(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ROXL (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteROXR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "ROXR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteRTE(int opcode, QString& traceRecord, int trace) {
   int status;
   unsigned int sr, pc;

   // Make sure we're in supervisor mode or trap
   if (!(this->registerData[SR_INDEX] & S_FLAG)) {
      this->setRegister(PC_INDEX, this->registerData[PC_INDEX] - 2, LONG);
      if ((status = this->processException(8)) != EXECUTE_OK)
         return status;

      if (trace)
         traceRecord += "Privilege Violation Exception";
      return EXECUTE_PRIVILEGED_OK;
   }

   // Pop the SR off the stack
   if ((status = this->pop(SSP_INDEX, sr, WORD)) != EXECUTE_OK)
      return status;

   this->setRegister(SR_INDEX, sr, WORD);

   // Pop the PC off the stack
   if ((status = this->pop(SSP_INDEX, pc, LONG)) != EXECUTE_OK)
      return status;

   this->setRegister(PC_INDEX, pc, LONG);

   if (trace)
      traceRecord += "RTE";

   return EXECUTE_PRIVILEGED_OK;
}

int Motorola68000Private::ExecuteRTR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "RTR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteRTS(int opcode, QString& traceRecord, int trace) {
   int status;
   unsigned int pc;
   int stackRegister;

   this->currentTicks += 16;

   if (trace)
      traceRecord += "RTS";

   // Determine which stack pointer to use
   if (this->registerData[SR_INDEX] & S_FLAG)
      stackRegister = SSP_INDEX;
   else
      stackRegister = USP_INDEX;

   // Pop the PC off the stack
   if ((status = this->pop(stackRegister, pc, LONG)) != EXECUTE_OK)
      return status;

   this->setRegister(PC_INDEX, pc, LONG);

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteRTD(int opcode, QString& traceRecord, int trace) {
   traceRecord += "RTD (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSBCD(int opcode, QString& traceRecord, int trace) {
   traceRecord += "SBCD (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSTOP(int opcode, QString& traceRecord, int trace) {
   traceRecord += "STOP (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSUB(int opcode, QString& traceRecord, int trace) {
   traceRecord += "SUB (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSUBA(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag;
   quint32 ea_address, register_number;
   unsigned int result, ea_data;
   QString ea_description;

   if (opcode & 0x100)
      size = LONG;
   else
      size = WORD;

   // Get the <ea> data address
   if ((status = this->computeEffectiveAddress(ea_address, in_register_flag, ea_description,
                                               opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the <ea> data
   if (in_register_flag)
      ea_data = this->registerData[ea_address];
   else if ((status = this->peek(ea_address, ea_data, size)) != EXECUTE_OK)
      return status;

   ea_data = this->signExtend(ea_data, size);

   // Get the register number
   register_number = A0_INDEX + ((opcode & 0x0e00) >> 9);

   // Adjust register_number if it's A7 and we're in supervisor mode
   if ((register_number == USP_INDEX) && (this->registerData[SR_INDEX] & S_FLAG))
      register_number = SSP_INDEX;

   result = this->registerData[register_number] - ea_data;
   this->setRegister(register_number, result, LONG);

   if (trace) {
      traceRecord += "SUBA";
      if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
      traceRecord += ea_address +
                     "," +
                     this->registerInfo[register_number].name;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteSUBI(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register;
   quint32 dest_addr, src_addr;
   unsigned int result, src, dest;

   size = (opcode & 0x00c0) >> 6;

   if (trace) {
      traceRecord += "SUBI";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
   }

   // Get the immediate data pointer
   if ((status = this->computeEffectiveAddress(src_addr, in_register, traceRecord, 0x3c, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the immediate data
   if ((status = this->peek(src_addr, src, size)) != EXECUTE_OK)
      return status;

   if (trace)
      traceRecord += ",";

   // Get the destination data pointer
   if ((status = this->computeEffectiveAddress(dest_addr, in_register, traceRecord, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   if (in_register) {
      dest = this->registerData[dest_addr];
      result = dest - src;

      this->setConditionCodes(src, dest, result, size, SUBTRACTION,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
      this->setRegister(dest_addr, result, size);
   } else {
      if ((status = this->peek(dest_addr, dest, size)) != EXECUTE_OK)
         return status;

      result = dest - src;

      this->setConditionCodes(src, dest, result, size, SUBTRACTION,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
      if ((status = this->poke(dest_addr, result, size)) != EXECUTE_OK)
         return status;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteSUBQ(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag;
   quint32 ea_address, immediate_data;
   unsigned int result, ea_data;
   QString ea_description;

   size = (opcode & 0x00c0) >> 6;

   if ((immediate_data = (opcode & 0x0e00) >> 9) == 0)
      immediate_data = 0;

   // Get the <ea> data address
   if ((status = this->computeEffectiveAddress(ea_address, in_register_flag, ea_description, opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the <ea> data
   if (in_register_flag)
      ea_data = this->registerData[ea_address];
   else if ((status = this->peek(ea_address, ea_data, size)) != EXECUTE_OK)
      return status;

   if (in_register_flag) {
      result = ea_data - immediate_data;
      if ((ea_address >= A0_INDEX) && (ea_address <= SSP_INDEX))
         this->setRegister(ea_address, result, LONG);
      else {
         this->setConditionCodes(immediate_data, ea_data, result, size, SUBTRACTION,
                                 C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
         this->setRegister(ea_address, result, size);
      }
   } else {
      result = ea_data - immediate_data;
      this->setConditionCodes(immediate_data, ea_data, result, size, SUBTRACTION,
                              C_FLAG | V_FLAG | Z_FLAG | N_FLAG | X_FLAG);
      if ((status = this->poke(ea_address, result, size)) != EXECUTE_OK)
         return status;
   }

   if (trace) {
      traceRecord += "SUBQ";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";
      traceRecord += "#$" + QString::number(immediate_data, 16);
      traceRecord += "," + ea_description;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteSUBX(int opcode, QString& traceRecord, int trace) {
   traceRecord += "SUBX (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteSWAP(int opcode, QString& traceRecord, int trace) {
   int register_number;
   unsigned int data;

   this->currentTicks += 4;

   register_number = D0_INDEX + (opcode & 7);
   data = this->registerData[register_number];
   data = ((data >> 16) & 0xffff) | ((data << 16) & 0xffff0000);
   this->setRegister(register_number, data, LONG);
   this->setConditionCodes(0, 0, data, LONG, OTHER, C_FLAG | V_FLAG | Z_FLAG | N_FLAG);

   if (trace)
      traceRecord += "SWAP.W " +
                     this->registerInfo[register_number].name;

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteSCC(int opcode, QString& traceRecord, int trace) {
   traceRecord += "SCC (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteTAS(int opcode, QString& traceRecord, int trace) {
   traceRecord += "TAS (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteTRAP(int opcode, QString& traceRecord, int trace) {
   traceRecord += "TRAP (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteTRAPV(int opcode, QString& traceRecord, int trace) {
   traceRecord += "TRAPV (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteTST(int opcode, QString& traceRecord, int trace) {
   int status, size, in_register_flag;
   quint32 address;
   unsigned int data;
   QString ea_description;

   size = ((opcode & 0x00c0) >> 6);

   // get the effective address
   if ((status = this->computeEffectiveAddress(address, in_register_flag, ea_description,
                                               opcode & 0x3f, size, trace)) != EXECUTE_OK)
      return status;

   // Fetch the data
   if (in_register_flag)
      data = this->registerData[address];
   else if ((status = this->peek(address, data, size)) != EXECUTE_OK)
      return status;

   this->setConditionCodes(0, 0, data, size, OTHER, C_FLAG | V_FLAG | Z_FLAG | N_FLAG);

   if (trace) {
      traceRecord += "TST";
      if (size == BYTE)
         traceRecord += ".B ";
      else if (size == WORD)
         traceRecord += ".W ";
      else if (size == LONG)
         traceRecord += ".L ";

      traceRecord += ea_description;
   }

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteUNLK(int opcode, QString& traceRecord, int trace) {
   traceRecord += "UNLK (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteCCR(int opcode, QString& traceRecord, int trace) {
   traceRecord += "CCR (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteBKPT(int opcode, QString& traceRecord, int trace) {
   traceRecord += "BKPT (Unimplemented)";
   return EXECUTE_ILLEGAL_INSTRUCTION;
}

int Motorola68000Private::ExecuteInvalid(int opcode, QString& traceRecord, int trace) {
   Q_UNUSED(opcode);

   int status;

   // Move the PC back to the address of the instructoin
   this->setRegister(PC_INDEX, this->registerData[PC_INDEX] - 2, LONG);

   // Process illegal instructoin exception
   if ((status = this->processException(4)) != EXECUTE_OK)
      return status;

   if (trace)
      traceRecord += "Illegal Instruction";

   return EXECUTE_OK;
}


int Motorola68000Private::ExecuteBusError(int opcode, QString& traceRecord, int trace) {
   int status;

   // Push PC
   if ((status = this->push(SSP_INDEX,
                            this->registerData[PC_INDEX],
                            LONG)) != EXECUTE_OK) {
      return status;
   }

   // Push SR
   if ((status = this->poke(SSP_INDEX,
                            this->registerData[SR_INDEX],
                            WORD)) != EXECUTE_OK) {
      return status;
   }

   // Push Opcode
   if ((status = this->push(SSP_INDEX,
                            opcode,
                            WORD)) != EXECUTE_OK) {
      return status;
   }

   // Push memory address at the fault
   if ((status = this->push(SSP_INDEX,
                            this->bus->lastExceptionAddress(),
                            LONG)) != EXECUTE_OK) {
      return status;
   }

   // Push status information word
   // TODO: This is wrong, please fix
   if ((status = this->push(SSP_INDEX,
                            9,
                            WORD)) != EXECUTE_OK) {
      return status;
   }

   // Get jump address from vector table
   quint32 address;
   if ((status = this->peek(0x08, address, LONG)) != EXECUTE_OK) {
      return status;
   }

   // Change to Supervisor mode
   this->registerData[SR_INDEX] |= S_FLAG;
   this->registerData[SR_INDEX] &= ~T_FLAG;

   // Jump to exception handler
   this->setRegister(PC_INDEX, address, LONG);

   if (trace)
      traceRecord += "Bus Error Exception";

   return EXECUTE_OK;
}

int Motorola68000Private::ExecuteAddressError(int opcode, QString& traceRecord, int trace) {
   int status;

   // Push PC
   if ((status = this->push(SSP_INDEX,
                            this->registerData[PC_INDEX],
                            LONG)) != EXECUTE_OK) {
      return status;
   }

   // Push SR
   if ((status = this->poke(SSP_INDEX,
                            this->registerData[SR_INDEX],
                            WORD)) != EXECUTE_OK) {
      return status;
   }

   // Push Opcode
   if ((status = this->push(SSP_INDEX,
                            opcode,
                            WORD)) != EXECUTE_OK) {
      return status;
   }

   // Push memory address at the fault
   if ((status = this->push(SSP_INDEX,
                            this->bus->lastExceptionAddress(),
                            LONG)) != EXECUTE_OK) {
      return status;
   }

   // Push status information word
   // TODO: This is wrong, please fix
   if ((status = this->push(SSP_INDEX,
                            9,
                            WORD)) != EXECUTE_OK) {
      return status;
   }

   // Get jump address from vector table
   quint32 address;
   if ((status = this->peek(0x0c, address, LONG)) != EXECUTE_OK) {
      return status;
   }

   // Change to Supervisor mode
   this->registerData[SR_INDEX] |= S_FLAG;
   this->registerData[SR_INDEX] &= ~T_FLAG;

   // Jump to exception handler
   this->setRegister(PC_INDEX, address, LONG);

   if (trace)
      traceRecord += "Address Error Exception";

   return EXECUTE_OK;
}
