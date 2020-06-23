#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "stdint.h"
#include "math.h"

#include <QMessageBox>

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
