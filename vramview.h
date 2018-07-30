#ifndef VRAMVIEW_H
#define VRAMVIEW_H

#include <QDialog>

namespace Ui {
   class VRAMView;
}

class VDP;
class VRAMView : public QDialog
{
      Q_OBJECT

   public:
      explicit VRAMView(QWidget *parent = 0);
      ~VRAMView();

      void setVDP(VDP* vdp);

   public slots:
      void render();

   private:
      Ui::VRAMView *ui;
      VDP*  vdp;
};

#endif // VRAMVIEW_H
