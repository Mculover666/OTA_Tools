#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "stdint.h"
#include "math.h"

#include <QMessageBox>
#include <QClipboard>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_radioButton_clicked()
{
    ui->spinBox_3->setEnabled(false);
    ui->radioButton_7->setEnabled(false);
    ui->radioButton_8->setEnabled(false);
}

void MainWindow::on_radioButton_2_clicked()
{
    ui->spinBox_3->setEnabled(true);
    ui->radioButton_7->setEnabled(true);
    ui->radioButton_8->setEnabled(true);
}

void MainWindow::on_pushButton_3_clicked()
{
    /* 复制推荐分区表烧录地址到系统剪切板 */
    QString source        = ui->lineEdit_2->text();   // 要拷贝的内容
    QClipboard *clipboard = QApplication::clipboard();   //获取系统剪贴板指针
    clipboard->setText(source);                          //设置剪贴板内容</span>
}
