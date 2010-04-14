#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include "comport_lin.h"
#include "wake.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    QRegExpValidator *validator = new QRegExpValidator(QRegExp("[0-9a-fA-F ]{1,}"), this);

    QStringList *sl = new QStringList;
    *sl << "Name" << "Addr" << "Cmd" << "Data" << "Enable" << "Start" << "Incoming data view";

    ui->setupUi(this);

    ui->leWakeData->setValidator(validator);
    ui->tableWidget->setHorizontalHeaderLabels(*sl);
    ui->tableWidget->setColumnWidth(0,100);
    ui->tableWidget->setColumnWidth(1,40);
    ui->tableWidget->setColumnWidth(2,40);
    ui->tableWidget->setColumnWidth(3,180);
    ui->tableWidget->setColumnWidth(4,52);
    ui->tableWidget->setColumnWidth(5,44);
    ui->tableWidget->setColumnWidth(6,180);

    ui->lePortData->setValidator(validator);
    //for (int i=0;i<20;i++) if (AccessPort(i)==1)  ui->cbxPort->addItem(QString("COM%1").arg(i));
    ui->cbxPort->addItem("/dev/ttyS1");
    ui->cbxPort->addItem("COM5");
    ui->cbxPort->addItem("COM8");
    ui->cbxPort->addItem("COM9");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

//---------------------------- Text2Hex: ------------------------------------
QByteArray intToByteArray(int value)
{
  QByteArray ba;
  ba.append(value);
  if ((value >> 8) & 0xff) ba.append(value >> 8);
  if ((value >> 16) & 0xff) ba.append(value >> 16);
  if ((value >> 24) & 0xff) ba.append(value >> 24);
  return ba;
}


void MainWindow::Text2Hex(QString s, QByteArray *ba)
{
    bool ok;
    QStringList list1 = s.split(" ", QString::SkipEmptyParts);
    list1 = list1.filter(QRegExp("^[0-9a-fA-F]{1,8}$"));
    for (int i=0;i<list1.size();i++)
      ba->append(intToByteArray(list1.at(i).toInt(&ok, 16)));
}

void MainWindow::on_pbConnect_clicked()
{
  char info[32];
  bool ok;

  if (!connected)
  {
    if (portOpen(ui->cbxPort->currentText().toLocal8Bit()) == EXIT_FAILURE)
      {ui->statusBar->showMessage("portOpen ERROR", 2000); return;}
    portSetOptions(ui->cbxSpeed->currentText().toInt(&ok,10),0);
    wake_init(portWrite, portRead);
//    if(dev->GetInfo(info) == EXIT_FAILURE) {ui->statusBar->showMessage("Get_Info ERROR", 2000); portClose(); return;}
    ui->statusBar->showMessage("On-Line",0);
    ui->statusBar->addPermanentWidget(&info_bar,0);
    info_bar.setText(QString(info));
    ui->pbConnect->setChecked(true);
    ui->pbConnect->setText("Disconnect");
    connected = true;
    ui->cbxPort->setEnabled(false);
  }
  else
  {
    portClose();
    ui->pbConnect->setText("Connect");
    ui->pbConnect->setChecked(false);
    ui->statusBar->showMessage("Disconnected",2000);
    info_bar.clear();
    connected = false;
    ui->cbxPort->setEnabled(true);
  }
}

void MainWindow::show_tx_log(unsigned char * clear_data, int size)
{
  unsigned char data[518];
  int len, i;
  QString s;

  if (ui->cbxLogLevel->currentIndex() == 0 || ui->cbxLogLevel->currentIndex() == 1) // full raw
  {
    len = wake_get_tx_raw_buffer(data);
    s = "<font color=#ff0000>TX: </font>";
    i = 0;
    s += QString("<font color=#c0c0c0>%1 </font>").arg(data[i++],2,16,QChar('0')); // FEND
    if (data[i] & 0x80)
      s += QString("<font color=black>%1 </font>").arg(data[i++],2,16,QChar('0')); // ADDR
    s += QString("<font color=#808080>%1 </font>").arg(data[i++],2,16,QChar('0')); // CMD
    s += QString("<font color=#808080>%1 </font>").arg(data[i++],2,16,QChar('0')); // N
    // data
    s += "<font color=#ff0000>";
    for(;i<len-1;i++)
    if (data[i] == 0xdb) // with stuffing
        {
          if (ui->cbxLogLevel->currentIndex() == 0)
          {
            s += QString("<font color=#b5a642>%1 ").arg(data[i++],2,16,QChar('0')); // stuffed data
            s += QString("%1 </font>").arg(data[i],2,16,QChar('0'));                // stuffed data
          }
          else switch (data[++i])
               {
               case 0xdc: s += "c0 "; break;
               case 0xdd: s += "db "; break;
               default:   s += "XX "; break;
               }
        } else s += QString("%1 ").arg(data[i],2,16,QChar('0')); // data
    s += "</font>";
    s += QString("<font color=#808080>%1 </font>").arg(data[len-1],2,16,QChar('0')); // crc
  }

  if (ui->cbxLogLevel->currentIndex() == 2) // logic only
  {
    s = "<font color=#ff0000>TX: ";
    for(i=0;i<size;i++) s += QString("%1 ").arg(clear_data[i],2,16,QChar('0')); // data
    s += "</font>";
  }
  if (ui->cbxASCII->isChecked())
  {
    s += "<font color=#0000ff>";
    for(i=0;i<size;i++) if (clear_data[i]>=' ') s += QChar(clear_data[i]); else s += '.';
    s += "</font>";
  }
  ui->teLog->append(s.toLocal8Bit());
}

void MainWindow::show_rx_log(unsigned char * clear_data, int size)
{
  unsigned char data[518];
  int len, i;
  QString s;

  if (ui->cbxLogLevel->currentIndex() == 0 || ui->cbxLogLevel->currentIndex() == 1) // raw
  {
    len = wake_get_rx_raw_buffer(data);
    s = "<font color=green>RX: ";
    i = 0;
    s += QString("<font color=#c0c0c0>%1 </font>").arg(data[i++],2,16,QChar('0')); // FEND
    if (data[i] & 0x80)
      s += QString("<font color=black>%1 </font>").arg(data[i++],2,16,QChar('0')); // ADDR
    s += QString("<font color=#808080>%1 </font>").arg(data[i++],2,16,QChar('0')); // CMD
    s += QString("<font color=#808080>%1 </font>").arg(data[i++],2,16,QChar('0')); // N
    s += "<font color=green>";
    for(;i<len-1;i++) s += QString("%1 ").arg(data[i],2,16,QChar('0')); // data
    s += "</font>";
    s += QString("<font color=#808080>%1 </font>").arg(data[len-1],2,16,QChar('0')); // crc
  }
  else
  {
    s = "<font color=green>RX: ";
    for(i=0;i<size;i++) s += QString("%1 ").arg(clear_data[i],2,16,QChar('0')); // data
    s += "</font>";
  }
  if (ui->cbxASCII->isChecked())
  {
    s += "<font color=#0000ff>";
    for(i=0;i<size;i++) if (clear_data[i]>=' ') s += QChar(clear_data[i]); else s += '.';
    s += "</font>";
  }
  ui->teLog->append(s.toLocal8Bit());
}

void MainWindow::on_pbSend_clicked()
{
  unsigned char data[518];
  QString s;
  unsigned char addr, cmd, len;
  QByteArray ba;
  int res;

  Text2Hex(ui->leWakeData->text(), &ba);
  if (res = wake_tx_frame(ui->hsbAddr->value(), ui->hsbCmd->value(), ba.size(), (unsigned char *)ba.constData()) < 0)
    {ui->teLog->append("wake_tx_frame error"); qDebug("%d", res); return;}
  show_tx_log((unsigned char *)ba.constData(), ba.size());

  if (wake_rx_frame(200, &addr, &cmd, &len, data) < 0)
   {ui->teLog->append("wake_rx_frame error"); return;}
  show_rx_log(data ,len);
}

void MainWindow::on_btClear_clicked()
{
  ui->teLog->clear();
}

void MainWindow::on_pbPortSend_clicked()
{
  QByteArray ba;
  QString s;
  int res;

  Text2Hex(ui->lePortData->text(), &ba);
  res = portWrite((unsigned char *)ba.constData(), ba.size());
  if (res < 0) {ui->teLog->append("portWrite error"); return;}
  if (res != ba.size()) {ui->teLog->append("portWrite len error"); return;}

  s = "<font color=red>TX: ";
  foreach(int data, ba) s += QString("%1 ").arg(data,2,16,QChar('0'));
  s += "</font>";

  if (ui->cbxASCII->isChecked())
  {
    s += "<font color=#0000ff>";
    foreach(int data, ba) if (data>0x30) s += QChar(data); else s += '.';
    s += "</font>";
  }
  ui->teLog->append(s.toLocal8Bit());
}

void MainWindow::newRow(int row)
{
  ui->tableWidget->insertRow(row);
  ui->tableWidget->setRowHeight(row, 18);
  ui->tableWidget->verticalHeader()->setResizeMode(row, QHeaderView::Fixed);

  QTableWidgetItem *item0 = new QTableWidgetItem;
  ui->tableWidget->setItem(row,0, item0);
  ui->tableWidget->item(row,0)->setText(QString("Command %1").arg(row,3,10,QChar('0')));

  QTableWidgetItem *item1 = new QTableWidgetItem;
  ui->tableWidget->setItem(row,1, item1);
  ui->tableWidget->item(row,1)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  ui->tableWidget->item(row,1)->setText("00");

  QTableWidgetItem *item2 = new QTableWidgetItem;
  ui->tableWidget->setItem(row,2, item2);
  ui->tableWidget->item(row,2)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  ui->tableWidget->item(row,2)->setText(QString("%1").arg(row,2,16,QChar('0')));

  QTableWidgetItem *item3 = new QTableWidgetItem;
  ui->tableWidget->setItem(row,3, item3);
  ui->tableWidget->item(row,3)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
 // ui->tableWidget->item(row,3)->setFlags(Qt::ItemIsUserCheckable);

  QCheckBox *cbxSel = new QCheckBox;
  ui->tableWidget->setCellWidget(row,4,cbxSel);
  cbxSel->setText("t");
  cbxSel->setGeometry(10,0,18,18);


  QToolButton *run = new QToolButton;
  run->setIcon(QIcon(":/Start.ico"));
  ui->tableWidget->setCellWidget(row,5,run);

  QTableWidgetItem *item6 = new QTableWidgetItem;
  ui->tableWidget->setItem(row,6, item6);

  //signalMapper.setMapping(run, row);
  //connect(run, SIGNAL(clicked()), &signalMapper, SLOT (map()));

  ui->tableWidget->setCurrentCell(row,0);
}

void MainWindow::on_toolButton_clicked()
{
  newRow(ui->tableWidget->rowCount());
}

void MainWindow::on_pbSetSerial_clicked()
{
  unsigned char data[518];
  QString s;
  unsigned char addr, cmd, len;
  QByteArray ba = "\xEb\xDA\xDE\xCD";
  int res;

  on_pbConnect_clicked();
  ui->teLog->append("Connect");

  ba.append(ui->sbSerial->value() & 0xff);
  ba.append((ui->sbSerial->value() >> 8 )& 0xff);
  if (res = wake_tx_frame(0, 5, ba.size(), (unsigned char *)ba.constData()) < 0)
    {ui->teLog->append("wake_tx_frame error"); qDebug("%d", res); return;}
  if (wake_rx_frame(200, &addr, &cmd, &len, data) < 0)
   {ui->teLog->append("wake_rx_frame error"); return;}

  if (data[0] != 0)   {ui->teLog->append("set serial error"); return;}

  ba.clear();
  if (res = wake_tx_frame(0, 4, ba.size(), (unsigned char *)ba.constData()) < 0)
    {ui->teLog->append("wake_tx_frame error"); qDebug("%d", res); return;}
  if (wake_rx_frame(200, &addr, &cmd, &len, data) < 0)
    {ui->teLog->append("wake_rx_frame error"); return;}

  if ((data[1] != ((ui->sbSerial->value() >> 8) & 0xff)) || (data[0] != (ui->sbSerial->value() & 0xff)))
    {ui->teLog->append("check serial error"); return;}

  ui->teLog->append("Write and check Ok!");
  ui->sbSerial->setValue(ui->sbSerial->value()+1);

  on_pbConnect_clicked();
  ui->teLog->append("Increment serial and disconnect");
}
