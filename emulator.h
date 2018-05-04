#ifndef EMULATOR_H
#define EMULATOR_H

#include <QObject>

class EmulatorPrivate;
class Emulator : public QObject
{
      Q_OBJECT
   public:
      explicit Emulator(QObject *parent = nullptr);

      void startEmulation();
      void stopEmulation();

   signals:

   public slots:

   private:
      EmulatorPrivate* d_ptr;
      Q_DECLARE_PRIVATE(Emulator)
};

#endif // EMULATOR_H
