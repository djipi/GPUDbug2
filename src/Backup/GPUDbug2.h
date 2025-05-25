#pragma once

//#include "common.h"
#include <QtWidgets/QMainWindow>
//#include <QFileDialog>
//#include <QMessageBox>
#include "ui_GPUDbug2.h"

class GPUDbug2 : public QMainWindow
{
    Q_OBJECT

    public:
        GPUDbug2(QWidget *parent = Q_NULLPTR);
        ~GPUDbug2(void);

    private slots:

    private:
        Ui::GPUDbug2 ui;

    private:
        void RefreshUI(void);
};
