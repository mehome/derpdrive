#-------------------------------------------------
#
# Project created by QtCreator 2018-04-22T01:19:07
#
#-------------------------------------------------

QT       += core gui
#CONFIG   += console

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = derpdrive
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        mainwindow.cpp \
    chips/motorola68000.cpp \
    memorybus.cpp \
    emulator.cpp \
    cartridge.cpp \
    ram.cpp \
    chips/motorola68000private.cpp \
    systemversion.cpp \
    controller.cpp \
    extensionport.cpp \
    chips/vdp.cpp \
    device.cpp \
    chips/z80.cpp \
    chips/z80/z80emu.cpp \
    vramview.cpp \
    m68kdebugger.cpp \
    chips/ym2612.cpp

HEADERS += \
        mainwindow.h \
    chips/motorola68000.h \
    chips/m68k_instructions.h \
    memorybus.h \
    emulator.h \
    config.h \
    cartridge.h \
    ram.h \
    chips/motorola68000private.h \
    chips/z80/z80emu.h \
    chips/z80/z80user.h \
    chips/z80/z80config.h \
    chips/z80/tables.h \
    chips/z80/instructions.h \
    chips/z80/macros.h \
    systemversion.h \
    controller.h \
    extensionport.h \
    chips/vdp.h \
    device.h \
    chips/z80.h \
    vramview.h \
    m68kdebugger.h \
    chips/ym2612.h

FORMS += \
        mainwindow.ui \
    vramview.ui \
    m68kdebugger.ui
