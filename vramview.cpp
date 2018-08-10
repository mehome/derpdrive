#include "vramview.h"
#include "ui_vramview.h"
#include "chips/vdp.h"

#define DISPLAY_PATTERNS_W 32
#define DISPLAY_PATTERNS_H 64

VRAMView::VRAMView(QWidget *parent) :
   QDialog(parent),
   ui(new Ui::VRAMView),
   vdp(nullptr)
{
   ui->setupUi(this);
   ui->graphicsView->scale(2, 2);
}

VRAMView::~VRAMView()
{
   delete ui;
}

void VRAMView::setVDP(VDP* vdp)
{
   this->vdp = vdp;
   this->render();

   connect(this->vdp, &VDP::dmaFinished, this, &VRAMView::render);
}

void VRAMView::render()
{
   QGraphicsScene* scene;

   int palette = ui->pallette->value();

   if(!ui->graphicsView->scene()) {
      scene = new QGraphicsScene(this);
      ui->graphicsView->setScene(scene);
   } else {
      scene = ui->graphicsView->scene();
   }

   scene->clear();

   QImage vramImage(256, 512, QImage::Format_ARGB32);
   vramImage.fill(Qt::black);

   for (quint16 y=0; y < DISPLAY_PATTERNS_H; y++) {
      for (quint16 x=0; x < DISPLAY_PATTERNS_W; x++) {
         this->vdp->debugBlit(&vramImage, (x + (y * DISPLAY_PATTERNS_W)) * 0x20, palette, x * 8, y * 8);
      }
   }

   int pixels = vramImage.width() * vramImage.height();
   QRgb* data = reinterpret_cast<QRgb*>(vramImage.bits());

   for(int b=0; b < pixels; b++) {
      data[b] |= 0xFF000000;
   }

   scene->addPixmap(QPixmap::fromImage(vramImage));

   QImage cramImage(16, 4, QImage::Format_ARGB32);
   this->vdp->debugCRamBlit(&cramImage);

   cramImage = cramImage.scaled(ui->cramImage->width(), ui->cramImage->height(), Qt::IgnoreAspectRatio, Qt::FastTransformation);

   ui->cramImage->setPixmap(QPixmap::fromImage(cramImage));
}

