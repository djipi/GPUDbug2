#include "GPUDbug2.h"
#include "qprocess.h"
#include "ui_GPUDbug2.h"


GPUDbug2::GPUDbug2(QWidget* parent)
    : QMainWindow(parent)
{
    // display the main UI
    ui.setupUi(this);
    RefreshUI();
}


GPUDbug2::~GPUDbug2(void)
{
}


// Set the information in the UI
void GPUDbug2::RefreshUI(void)
{
}
