#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>

// MainWindow constructor: sets up the UI and initializes the display
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    updateUI();
}

// Destructor (no special cleanup needed)
MainWindow::~MainWindow() {}

// Sets up the UI layout and connects signals to slots
void MainWindow::setupUI() {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QHBoxLayout *mainLayout = new QHBoxLayout(central);

    // --- Left: Register banks ---
    QVBoxLayout *leftLayout = new QVBoxLayout;
    regBank0Label = new QLabel("GPU Register Bank 0:");
    regBank0Label->setStyleSheet("font-weight: bold");
    regBank0 = new QTreeWidget;
    regBank0->setHeaderHidden(true);
    regBank1Label = new QLabel("GPU Register Bank 1:");
    regBank1 = new QTreeWidget;
    regBank1->setHeaderHidden(true);
    leftLayout->addWidget(regBank0Label);
    leftLayout->addWidget(regBank0);
    leftLayout->addWidget(regBank1Label);
    leftLayout->addWidget(regBank1);

    // --- Center: Code view ---
    QVBoxLayout *centerLayout = new QVBoxLayout;
    codeLabel = new QLabel("GPU Code:");
    codeView = new QTreeWidget;
    codeView->setHeaderHidden(true);
    centerLayout->addWidget(codeLabel);
    centerLayout->addWidget(codeView);

    // --- Right: Controls and status ---
    QVBoxLayout *rightLayout = new QVBoxLayout;

    // Move GPU/DSP mode radio buttons to the top
    gpuMode = new QRadioButton("GPU Mode");
    dspMode = new QRadioButton("DSP Mode");
    gpuMode->setChecked(true);
    rightLayout->addWidget(gpuMode);
    rightLayout->addWidget(dspMode);

    loadBinBtn = new QPushButton("Load BIN");
    loadAddressEdit = new QLineEdit("$00F03000");
    label4 = new QLabel("at");
    pcEdit = new QLineEdit("$00F03000");
    label5 = new QLabel("PC:");
    runBtn = new QPushButton("Run GPU (F9)");
    stepBtn = new QPushButton("Step (F10)");
    skipBtn = new QPushButton("Skip (F11)");
    resetBtn = new QPushButton("Reset GPU (F12)");
    exitBtn = new QPushButton("Exit");
    memWarn = new QCheckBox("No memory warning");
    progress = new QProgressBar;
    flagStatusLabel = new QLabel("Flags: Z:0 N:0 C:0");
    g_hidataLabel = new QLabel("G_HIDATA: $00000000");
    g_remainLabel = new QLabel("G_REMAIN: $00000000");
    jumpLabel = new QLabel("Jump: $00000000");
    gpubpLabel = new QLabel("BP: $00000000");

    // Add widgets to the right layout (after GPU/DSP mode)
    rightLayout->addWidget(loadBinBtn);
    rightLayout->addWidget(new QLabel("Load Address:"));
    rightLayout->addWidget(loadAddressEdit);

    // Move the "No memory warning" checkbox here, right after Load Address
    rightLayout->addWidget(memWarn);

    rightLayout->addWidget(label5);
    rightLayout->addWidget(pcEdit);
    rightLayout->addWidget(runBtn);
    rightLayout->addWidget(stepBtn);
    rightLayout->addWidget(skipBtn);
    rightLayout->addWidget(resetBtn);
    rightLayout->addWidget(progress);
    rightLayout->addWidget(flagStatusLabel);
    rightLayout->addWidget(g_hidataLabel);
    rightLayout->addWidget(g_remainLabel);
    rightLayout->addWidget(jumpLabel);
    rightLayout->addWidget(gpubpLabel);

    // Add stretch to push the Exit button to the bottom
    rightLayout->addStretch();

    // Now add the Exit button at the bottom
    rightLayout->addWidget(exitBtn);

    // Assemble the main layout
    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(centerLayout, 2);
    mainLayout->addLayout(rightLayout, 1);

    // File dialog for loading BIN files
    openDialog = new QFileDialog(this);

    // Connect UI signals to slots
    connect(exitBtn, &QPushButton::clicked, this, &MainWindow::onExit);
    connect(loadBinBtn, &QPushButton::clicked, this, &MainWindow::onLoadBin);
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::onRun);
    connect(stepBtn, &QPushButton::clicked, this, &MainWindow::onStep);
    connect(skipBtn, &QPushButton::clicked, this, &MainWindow::onSkip);
    connect(resetBtn, &QPushButton::clicked, this, &MainWindow::onReset);
    connect(gpuMode, &QRadioButton::clicked, this, &MainWindow::onGPUMode);
    connect(dspMode, &QRadioButton::clicked, this, &MainWindow::onDSPMode);
    connect(pcEdit, &QLineEdit::returnPressed, this, &MainWindow::onPCEditReturnPressed);
    connect(regBank0, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onRegBank0ItemDoubleClicked);
    connect(regBank1, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onRegBank1ItemDoubleClicked);
    connect(codeView, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onCodeViewItemDoubleClicked);
}

// Updates all UI widgets to reflect the current state of the debugger
void MainWindow::updateUI() {
    // Update register banks
    regBank0->clear();
    for (const QString &s : debugger.getRegBank(0))
        regBank0->addTopLevelItem(new QTreeWidgetItem(QStringList() << s));
    regBank1->clear();
    for (const QString &s : debugger.getRegBank(1))
        regBank1->addTopLevelItem(new QTreeWidgetItem(QStringList() << s));
    // Update code view
    codeView->clear();
    for (const QString &s : debugger.getCodeView())
        codeView->addTopLevelItem(new QTreeWidgetItem(QStringList() << s));
    // Update status labels
    flagStatusLabel->setText(debugger.getFlags());
    g_hidataLabel->setText(debugger.getHiData());
    g_remainLabel->setText(debugger.getRemain());
    jumpLabel->setText(debugger.getJump());
    gpubpLabel->setText(debugger.getBP());
    pcEdit->setText(debugger.getPC());
    // Update progress bar
    progress->setValue(debugger.getProgress());
    // Enable/disable buttons based on debugger state
    runBtn->setEnabled(debugger.canRun());
    stepBtn->setEnabled(debugger.canStep());
    skipBtn->setEnabled(debugger.canSkip());
    resetBtn->setEnabled(debugger.canReset());
}

// Slot: Load a BIN file and initialize the debugger
void MainWindow::onLoadBin() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open file", "", "BIN Files (*.bin);;Obj files (*.o);;All Files (*)");
    if (!fileName.isEmpty()) {
        bool ok = false;
        int address = loadAddressEdit->text().remove('$').toInt(&ok, 16);
        if (!ok) address = 0;
        if (debugger.loadBin(fileName, address)) {
            debugger.reset();
            updateUI();
        } else {
            QMessageBox::warning(this, "Error", "Failed to load BIN file.");
        }
    }
}

// Slot: Run the GPU program
void MainWindow::onRun() { debugger.run(); updateUI(); }

// Slot: Step one instruction
void MainWindow::onStep() { debugger.step(); updateUI(); }

// Slot: Skip one instruction (without execution)
void MainWindow::onSkip() { debugger.skip(); updateUI(); }

// Slot: Reset the GPU state
void MainWindow::onReset() { debugger.reset(); updateUI(); }

// Slot: Exit the application
void MainWindow::onExit() { close(); }

// Slot: Switch to GPU mode
void MainWindow::onGPUMode() { debugger.setGPUMode(true); updateUI(); }

// Slot: Switch to DSP mode
void MainWindow::onDSPMode() { debugger.setGPUMode(false); updateUI(); }

// Slot: Update PC from the line edit
void MainWindow::onPCEditReturnPressed() {
    debugger.setPC(pcEdit->text());
    updateUI();
}

// Slot: Edit a register in bank 0
void MainWindow::onRegBank0ItemDoubleClicked(QTreeWidgetItem *item, int) {
    debugger.editRegister(0, item->text(0));
    updateUI();
}

// Slot: Edit a register in bank 1
void MainWindow::onRegBank1ItemDoubleClicked(QTreeWidgetItem *item, int) {
    debugger.editRegister(1, item->text(0));
    updateUI();
}

// Slot: Set a breakpoint in the code view
void MainWindow::onCodeViewItemDoubleClicked(QTreeWidgetItem *item, int) {
    debugger.setBreakpoint(item->text(0));
    updateUI();
}