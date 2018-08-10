#include "motorola68000private.h"
#include "memorybus.h"
#include "../device.h"

#include "chips/m68k/m68k.h"

#include <QDebug>

Motorola68000Private* currentContext = nullptr;

Motorola68000Private::Motorola68000Private(Motorola68000* q)
    : disabled(false),
      debug(false),
      debugRun(false),
      context(nullptr),
      tracing(false),
      q_ptr(q)
{
    this->context = malloc(m68k_context_size());
    memset(this->context, 0, m68k_context_size());
    currentContext = this;

    m68k_init();
    m68k_set_context(this->context);
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
}

Motorola68000Private::~Motorola68000Private()
{

}

void Motorola68000Private::switchContext()
{
    if (currentContext == this)
        return;

    if (currentContext)
        m68k_get_context(currentContext->context);

    m68k_set_context(this->context);
    currentContext = this;
}

unsigned int m68k_read_disassembler_16(unsigned int address) {
    return m68k_read_memory_16(address);
}

unsigned int m68k_read_disassembler_32(unsigned int address) {
    return m68k_read_memory_32(address);
}

unsigned int m68k_read_memory_8(unsigned int address) {
    quint8 val = 0;
    Motorola68000Private* ctx = currentContext;

    address = address & 0x00FFFFFF;

    ctx->bus->peek(address, val);

    return val;
}

unsigned int m68k_read_memory_16(unsigned int address) {
    quint8 b0, b1 = 0;
    Motorola68000Private* ctx = currentContext;

    address = address & 0x00FFFFFF;

    ctx->bus->peek(address,      b0);
    ctx->bus->peek(address + 1,  b1);

    return static_cast<unsigned int>((b0 << 8) | b1);
}

unsigned int m68k_read_memory_32(unsigned int address) {
    quint8 b0, b1, b2, b3 = 0;
    Motorola68000Private* ctx = currentContext;

    address = address & 0x00FFFFFF;

    ctx->bus->peek(address,      b0);
    ctx->bus->peek(address + 1,  b1);
    ctx->bus->peek(address + 2,  b2);
    ctx->bus->peek(address + 3,  b3);

    return  static_cast<unsigned int>(  (b0 << 24)  |
                                        (b1 << 16)  |
                                        (b2 << 8)   |
                                        (b3 << 0));
}

void m68k_write_memory_8(unsigned int address, unsigned int val) {
    Motorola68000Private* ctx = currentContext;

    address = address & 0x00FFFFFF;

    // Hack for VDP 8-Bit "Anomaly"
    if  (address >= 0x00C00004 && address <= 0x00C00007) {
        ctx->bus->poke(address & 0x00FFFFFE,        static_cast<quint8>(val & 0xFF));
        ctx->bus->poke(address & 0x00FFFFFE + 1,    static_cast<quint8>(val & 0xFF));
    } else {
        ctx->bus->poke(address, static_cast<quint8>(val & 0xFF));
    }
}

void m68k_write_memory_16(unsigned int address, unsigned int val) {
    Motorola68000Private* ctx = currentContext;

    address = address & 0x00FFFFFF;

    ctx->bus->poke(address,     static_cast<quint8>((val >> 8) & 0xFF));
    ctx->bus->poke(address + 1, static_cast<quint8>(val & 0xFF));
}

void m68k_write_memory_32(unsigned int address, unsigned int val) {
    Motorola68000Private* ctx = currentContext;

    address = address & 0x00FFFFFF;

    ctx->bus->poke(address,     static_cast<quint8>((val >> 24) & 0xFF));
    ctx->bus->poke(address + 1, static_cast<quint8>((val >> 16) & 0xFF));
    ctx->bus->poke(address + 2, static_cast<quint8>((val >> 8) & 0xFF));
    ctx->bus->poke(address + 3, static_cast<quint8>(val & 0xFF));
}
