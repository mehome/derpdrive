#ifndef M68KDEBUGGER_H
#define M68KDEBUGGER_H

#include <QDialog>

namespace Ui {
class M68KDebugger;
}

class Motorola68000;

class M68KDebugger : public QDialog
{
    Q_OBJECT

public:
    explicit M68KDebugger(QWidget *parent, Motorola68000* cpu);
    ~M68KDebugger();

private slots:
    void on_M68KDebugger_finished(int result);
    void on_debugStep_clicked();
    void on_debugRun_clicked();
    void on_debugPause_clicked();
    void on_addBreakpoint_clicked();

public slots:
    void singleStep(unsigned int pc, unsigned int opcode, QString instruction);
    void breakpointHit(unsigned int pc);

private:
    Ui::M68KDebugger *ui;
    Motorola68000* cpu;
};

#endif // M68KDEBUGGER_H
