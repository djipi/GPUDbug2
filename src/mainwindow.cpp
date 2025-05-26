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
#include <QHeaderView> // Add this include at the top of your file

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

    // --- Left: Register banks in two columns ---
    QHBoxLayout *regBanksLayout = new QHBoxLayout;

    // Register Bank 0 column
    QVBoxLayout *regBank0Layout = new QVBoxLayout;
    regBank0Label = new QLabel("Register Bank 0");
    regBank0 = new QTreeWidget;
    regBank0->setHeaderHidden(true);
    regBank0->setMinimumWidth(300); // Set minimum width for readability
    regBank0->setColumnCount(2);
    regBank0->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    regBank0->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    regBank0->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    regBank0Layout->addWidget(regBank0Label);
    regBank0Layout->addWidget(regBank0);

    // Register Bank 1 column
    QVBoxLayout *regBank1Layout = new QVBoxLayout;
    regBank1Label = new QLabel("Register Bank 1");
    regBank1 = new QTreeWidget;
    regBank1->setHeaderHidden(true);
    regBank1->setMinimumWidth(300); // Set minimum width for readability
    regBank1->setColumnCount(2);
    regBank1->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    regBank1->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    regBank1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    regBank1Layout->addWidget(regBank1Label);
    regBank1Layout->addWidget(regBank1);

    // Add both columns to the register banks layout
    regBanksLayout->addLayout(regBank0Layout);
    regBanksLayout->addLayout(regBank1Layout);

    // --- Center: Code view ---
    QVBoxLayout *centerLayout = new QVBoxLayout;
    codeLabel = new QLabel("Code");
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
    runBtn = new QPushButton("Run (F9)");
    stepBtn = new QPushButton("Step (F10)");
    skipBtn = new QPushButton("Skip (F11)");
    resetBtn = new QPushButton("Reset (F12)");
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

    QHBoxLayout *loadAddrLayout = new QHBoxLayout;
    QLabel *loadAddrLabel = new QLabel("Load Address:");
    loadAddrLayout->addWidget(loadAddrLabel);
    loadAddrLayout->addWidget(loadAddressEdit);
    rightLayout->addLayout(loadAddrLayout);

    // Move the "No memory warning" checkbox here, right after Load Address
    rightLayout->addWidget(memWarn);

    QHBoxLayout *pcLayout = new QHBoxLayout;
    pcLayout->addWidget(label5);
    pcLayout->addWidget(pcEdit);
    rightLayout->addLayout(pcLayout);

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
    mainLayout->addLayout(regBanksLayout, 1);
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

    regBank0->setHeaderHidden(true);
    regBank1->setHeaderHidden(true);
}

// Updates all UI widgets to reflect the current state of the debugger
void MainWindow::updateUI() {
    // Update register banks
    regBank0->clear();
    for (const QString &s : debugger.getRegBank(0)) {
        // Split s into two parts, e.g., "R0: $00000000"
        QStringList parts = s.split(": ");
        if (parts.size() == 2)
            regBank0->addTopLevelItem(new QTreeWidgetItem(parts));
        else
            regBank0->addTopLevelItem(new QTreeWidgetItem(QStringList() << s << ""));
    }
    regBank0->resizeColumnToContents(0);
    regBank0->resizeColumnToContents(1);

    regBank1->clear();
    for (const QString &s : debugger.getRegBank(1)) {
        QStringList parts = s.split(": ");
        if (parts.size() == 2)
            regBank1->addTopLevelItem(new QTreeWidgetItem(parts));
        else
            regBank1->addTopLevelItem(new QTreeWidgetItem(QStringList() << s << ""));
    }
    regBank1->resizeColumnToContents(0);
    regBank1->resizeColumnToContents(1);

    // Ensure all register lines are visible without vertical scroll bar
    int regCount = regBank0->topLevelItemCount();
    int rowHeight = regBank0->sizeHintForRow(0) > 0 ? regBank0->sizeHintForRow(0) : 22; // fallback if empty
    int headerHeight = regBank0->header()->isHidden() ? 0 : regBank0->header()->height();
    int totalHeight = regCount * rowHeight + headerHeight + 4; // 4 for frame

    regBank0->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    regBank0->setMinimumHeight(totalHeight);
    regBank0->setMaximumHeight(totalHeight);

    regCount = regBank1->topLevelItemCount();
    rowHeight = regBank1->sizeHintForRow(0) > 0 ? regBank1->sizeHintForRow(0) : 22;
    headerHeight = regBank1->header()->isHidden() ? 0 : regBank1->header()->height();
    totalHeight = regCount * rowHeight + headerHeight + 4;

    regBank1->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    regBank1->setMinimumHeight(totalHeight);
    regBank1->setMaximumHeight(totalHeight);

    // Update code view
    codeView->clear();
    for (const QString &s : debugger.getCodeView())
        codeView->addTopLevelItem(new QTreeWidgetItem(QStringList() << s));
    // Update status labels
    flagStatusLabel->setText(debugger.getFlags());
    g_hidataLabel->setText(QString("G_HIDATA: %1").arg(debugger.getHiData()));
    g_remainLabel->setText(QString("G_REMAIN: %1").arg(debugger.getRemain()));
    jumpLabel->setText(QString("Jump: %1").arg(debugger.getJump()));
    gpubpLabel->setText(QString("BP: %1").arg(debugger.getBP()));
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
void MainWindow::onGPUMode() {
    debugger.setGPUMode(true);
    loadAddressEdit->setText("$00F03000"); // Set default address for GPU mode
    updateUI();
}

// Slot: Switch to DSP mode
void MainWindow::onDSPMode() {
    debugger.setGPUMode(false);
    loadAddressEdit->setText("$00F1B000"); // Set default address for DSP mode
    updateUI();
}

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