#include "m68kdebugger.h"
#include "ui_m68kdebugger.h"

#include "chips/motorola68000.h"

M68KDebugger::M68KDebugger(QWidget *parent, Motorola68000* cpu) :
    QDialog(parent),
    ui(new Ui::M68KDebugger),
    cpu(cpu)
{
    ui->setupUi(this);

    this->cpu->debugToggle(true);
    connect(this->cpu, &Motorola68000::singleStep, this, &M68KDebugger::singleStep);
    connect(this->cpu, &Motorola68000::breakpointReached, this, &M68KDebugger::breakpointHit);

    ui->tableWidget->setRowCount(18);

    for(int r=0; r < 19; r++) {
        ui->tableWidget->setItem(r, 0, new QTableWidgetItem(this->cpu->registerName(r)));
    }

    this->cpu->debugStep();
}

M68KDebugger::~M68KDebugger()
{
    delete ui;
}

void M68KDebugger::on_M68KDebugger_finished(int result)
{
    this->cpu->debugToggle(false);
}

void M68KDebugger::singleStep(unsigned int pc, unsigned int opcode, QString instruction)
{
    ui->debugStep->setEnabled(true);
    ui->debugPause->setEnabled(false);
    ui->debugRun->setEnabled(true);

    ui->listWidget->addItem(instruction);

    for(int r=0; r < 19; r++) {
        ui->tableWidget->setItem(r, 1, new QTableWidgetItem(QString::number(this->cpu->registerValue(r), 16).rightJustified(8, '0')));
    }
}

void M68KDebugger::breakpointHit(unsigned int pc)
{
    ui->debugStep->setEnabled(true);
    ui->debugRun->setEnabled(true);
    ui->debugPause->setEnabled(false);
}

void M68KDebugger::on_debugStep_clicked()
{
    ui->debugStep->setEnabled(false);
    this->cpu->debugStep();
}

void M68KDebugger::on_debugRun_clicked()
{
    ui->debugStep->setEnabled(false);
    ui->debugRun->setEnabled(false);
    ui->debugPause->setEnabled(true);

    this->cpu->debugRun();
}

void M68KDebugger::on_debugPause_clicked()
{
    ui->debugPause->setEnabled(false);

    this->cpu->debugPause();
}

void M68KDebugger::on_addBreakpoint_clicked()
{
    bool ok = false;
    unsigned int breakpoint = ui->breakpointAdr->text().toUInt(&ok, 16);

    if (ok) {
        ui->breakpointAdr->setText("");
        ui->breakpointList->addItem(QString::number(breakpoint, 16).rightJustified('0', 8));

        this->cpu->debugAddBreakpoint(breakpoint);
    }
}
