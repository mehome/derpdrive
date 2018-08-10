#ifndef MOTOROLA68000PRIVATE_H
#define MOTOROLA68000PRIVATE_H

#include <QObject>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <queue>

class Device;

class MemoryBus;
class Motorola68000;
class Motorola68000Private;

class Motorola68000Private {
public:


public:
    bool                disabled;
    bool                debug;
    bool                debugRun;
    QList<unsigned int> breakpoins;
    MemoryBus*          bus;
    void*               context;

    bool                tracing;


public:


public:
    Motorola68000Private(Motorola68000* q);
    ~Motorola68000Private();

    void switchContext();

public:

public:
    Motorola68000* q_ptr;
    Q_DECLARE_PUBLIC(Motorola68000)
};

#endif // MOTOROLA68000PRIVATE_H
