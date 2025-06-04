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
#include <QInputDialog> // Add this include at the top
#include <QEvent>
#include <QListView> // Include QListView
#include <QKeyEvent> // Include QKeyEvent

// MainWindow constructor: sets up the UI and initializes the display
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    updateUI();

	// Connection for disassembly progress updates
    connect(&debugger, &Debugger::disassemblyProgress, progress, &QProgressBar::setValue);
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
    regBank0Label->setCursor(Qt::PointingHandCursor);
    regBank0Label->installEventFilter(this);
    regBank0 = new QTreeWidget;
    regBank0->setHeaderHidden(true);
    regBank0->setMinimumWidth(400); // Set minimum width for readability
    regBank0->setColumnCount(3);
    regBank0->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    regBank0->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    regBank0->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    regBank0Layout->addWidget(regBank0Label);
    regBank0Layout->addWidget(regBank0);

    // Register Bank 1 column
    QVBoxLayout *regBank1Layout = new QVBoxLayout;
    regBank1Label = new QLabel("Register Bank 1");
    regBank1Label->setCursor(Qt::PointingHandCursor);
    regBank1Label->installEventFilter(this);
    regBank1 = new QTreeWidget;
    regBank1->setHeaderHidden(true);
    regBank1->setMinimumWidth(400); // Set minimum width for readability
    regBank1->setColumnCount(3);
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
    codeLabel = new QLabel("Disassembly Code");
    codeView = new QTreeWidget;
    codeView->setColumnCount(4); // Four sub-columns
    codeView->setHeaderHidden(true); // Hide header for no visual separation
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
    //label4 = new QLabel("at");
    pcEdit = new QLineEdit("$00F03000");
    label5 = new QLabel("PC:");
    runBtn = new QPushButton("Execute (F5)");
    stepBtn = new QPushButton("Step into (F11)");
    skipBtn = new QPushButton("Skip (F7)");
    resetBtn = new QPushButton("Restart (F3)");
    exitBtn = new QPushButton("Exit");
    memWarn = new QCheckBox("No memory warning");
    progress = new QProgressBar;
    flagStatusLabel = new QLabel("Flags: Z:0 N:0 C:0");
    g_hidataLabel = new QLabel("G_HIDATA: $00000000");
    g_remainLabel = new QLabel("G_REMAIN: $00000000");
    jumpLabel = new QLabel("Jump: $00000000");
    gpubpLabel = new QLabel("Breakpoint: $00000000");

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

    rightLayout->addWidget(gpubpLabel);

    rightLayout->addWidget(runBtn);
    rightLayout->addWidget(stepBtn);
    rightLayout->addWidget(skipBtn);
    rightLayout->addWidget(resetBtn);
    rightLayout->addWidget(progress);
    rightLayout->addWidget(flagStatusLabel);
    rightLayout->addWidget(g_hidataLabel);
    rightLayout->addWidget(g_remainLabel);
    rightLayout->addWidget(jumpLabel);

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
    connect(memWarn, &QCheckBox::toggled, &debugger, &Debugger::setMemoryWarningEnabled);

    regBank0->setHeaderHidden(true);
    regBank1->setHeaderHidden(true);

    // Optionally, set the initial state
    debugger.setMemoryWarningEnabled(memWarn->isChecked());

    prevRegBank0.resize(32);
    prevRegBank1.resize(32);
}

// Updates all UI widgets to reflect the current state of the debugger
void MainWindow::updateUI() {
    // Update register banks with change highlighting
    regBank0->clear();
    QStringList currentBank0 = debugger.getRegBank(0);
    for (int i = 0; i < currentBank0.size(); ++i) {
        QStringList parts = currentBank0[i].split(": ", QString::KeepEmptyParts);
        QString mark = "";
        if ((i < prevRegBank0.size()) && (prevRegBank0[i] != debugger.getRegBankRegisterValue(0, i))) {
            mark = "*";
            prevRegBank0[i] = debugger.getRegBankRegisterValue(0, i);
        }
        QTreeWidgetItem* item;
        if (parts.size() == 2)
            item = new QTreeWidgetItem(QStringList() << mark << parts[0] << parts[1]);
        else
            item = new QTreeWidgetItem(QStringList() << mark << currentBank0[i] << "");
        if (mark == "*") {
            QFont font = item->font(0);
            font.setBold(true);
            item->setFont(0, font);
            item->setForeground(0, QBrush(Qt::green));
        }
        regBank0->addTopLevelItem(item);
    }
    //prevRegBank0 = std::vector<int>(currentBank0.begin(), currentBank0.end());
    regBank0->resizeColumnToContents(0);
    regBank0->resizeColumnToContents(1);
    regBank0->resizeColumnToContents(2);

    regBank1->clear();
    QStringList currentBank1 = debugger.getRegBank(1);
    for (int i = 0; i < currentBank1.size(); ++i) {
        QStringList parts = currentBank1[i].split(": ", QString::KeepEmptyParts);
        QString mark = "";
        if ((i < prevRegBank1.size()) && (prevRegBank1[i] != debugger.getRegBankRegisterValue(1, i))) {
            mark = "*";
            prevRegBank1[i] = debugger.getRegBankRegisterValue(1, i);
        }
        QTreeWidgetItem* item;
        if (parts.size() == 2)
            item = new QTreeWidgetItem(QStringList() << mark << parts[0] << parts[1]);
        else
            item = new QTreeWidgetItem(QStringList() << mark << currentBank1[i] << "");
        if (mark == "*") {
            QFont font = item->font(0);
            font.setBold(true);
            item->setFont(0, font);
            item->setForeground(0, QBrush(Qt::green));
        }
        regBank1->addTopLevelItem(item);
    }
    //prevRegBank1 = std::vector<int>(currentBank1.begin(), currentBank1.end());
    regBank1->resizeColumnToContents(0);
    regBank1->resizeColumnToContents(1);
    regBank1->resizeColumnToContents(2);

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
    int currentPC = 0;
    {
        QString pcStr = debugger.getPCString();
        bool ok = false;
        currentPC = QString(pcStr).remove('$').toInt(&ok, 16);
        if (!ok) currentPC = 0;
    }
    for (const QString &s : debugger.getCodeView()) {
        QStringList parts = s.split(": ", QString::KeepEmptyParts);
        QString bpMark, pcMark;
        if (parts.size() == 2) {
            QString addrStr = parts[0].trimmed();
            QString addrForConv = addrStr;
            addrForConv.remove('$');
            bool ok = false;
            int addr = addrForConv.toInt(&ok, 16);
            if (ok && debugger.hasBreakpoint(addr))
                bpMark = "*";
            if (ok && addr == currentPC)
                pcMark = ">";
            QString displayAddr = QString("$%1").arg(addr, 8, 16, QChar('0')).toUpper();
            QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << bpMark << pcMark << displayAddr << parts[1]);
            if (!bpMark.isEmpty()) {
                item->setForeground(0, QBrush(Qt::red));
                QFont markerFont = codeView->font();
                markerFont.setPointSizeF(markerFont.pointSizeF() * 1.3);
                markerFont.setBold(true);
                item->setFont(0, markerFont);
            }
            if (!pcMark.isEmpty()) {
                QFont pcFont = codeView->font();
                pcFont.setBold(true);
                item->setFont(1, pcFont);
                item->setForeground(1, QBrush(QColor(0, 70, 200)));
            }
            codeView->addTopLevelItem(item);
        } else {
            codeView->addTopLevelItem(new QTreeWidgetItem(QStringList() << "" << "" << s << ""));
        }
    }
    codeView->resizeColumnToContents(0);
    codeView->resizeColumnToContents(1);
    //codeView->resizeColumnToContents(2);
    //codeView->resizeColumnToContents(3);

    // Update status labels
    flagStatusLabel->setText(debugger.getFlags());
    g_hidataLabel->setText(QString("G_HIDATA: %1").arg(debugger.getHiData()));
    g_remainLabel->setText(QString("G_REMAIN: %1").arg(debugger.getRemain()));
    jumpLabel->setText(QString("Jump: %1").arg(debugger.getJump()));
    gpubpLabel->setText(QString("Breakpoint: %1").arg(debugger.getBP()));
    pcEdit->setText(debugger.getPCString());
    // Update progress bar
    progress->setValue(debugger.getProgress());
    // Enable/disable buttons based on debugger state
    bool fileLoaded = debugger.canRun() || debugger.canStep() || debugger.canSkip();
    runBtn->setEnabled(fileLoaded);
    stepBtn->setEnabled(fileLoaded);
    skipBtn->setEnabled(fileLoaded);
    resetBtn->setEnabled(fileLoaded);
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
            debugger.setStringPC(QString("$%1").arg(address, 8, 16, QChar('0')).toUpper()); // Set PC to loading address
            updateUI();
        } else {
            QMessageBox::warning(this, "Error", "Failed to load BIN file.");
        }
    }
}

// Slot: Run the GPU program
void MainWindow::onRun() {
    // Disable buttons immediately when Run is clicked
    runBtn->setEnabled(false);
    stepBtn->setEnabled(false);
    skipBtn->setEnabled(false);
    resetBtn->setEnabled(false);

    debugger.run();
    updateUI();
}

// Slot: Step one instruction
void MainWindow::onStep() {
    int w = debugger.ReadWord(debugger.getPCValue(), true);
    if (w != -1) {
        //noPCrefresh = false;
        //GPUStep((uint16_t)w, true);
        debugger.step((uint16_t)w, true);
        updateUI();
    }
}

// Slot: Skip one instruction (without execution)
void MainWindow::onSkip() { debugger.skip(); updateUI(); }

// Slot: Reset the GPU state
void MainWindow::onReset() {
    debugger.reset();
    std::fill(prevRegBank0.begin(), prevRegBank0.end(), 0);
    std::fill(prevRegBank1.begin(), prevRegBank1.end(), 0);
    updateUI();
}

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
    debugger.setStringPC(pcEdit->text());
    updateUI();
}

// Slot: Edit a register in bank 0
void MainWindow::onRegBank0ItemDoubleClicked(QTreeWidgetItem* item, int column) {
    if (column != 1) return; // Only allow editing the value column
    int regIndex = regBank0->indexOfTopLevelItem(item);
    QString currentValue = item->text(2);
    bool ok = false;
    QInputDialog dlg(this);
    dlg.setWindowTitle("Edit Register (Bank 0)");
    dlg.setLabelText(QString("Current value: %1\nEnter new value (hex, without $):").arg(currentValue));
    dlg.setTextValue(currentValue.remove('$'));
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.resize(600, dlg.sizeHint().height()); // Make the dialog wider
    if (dlg.exec() != QDialog::Accepted) return;
    QString newValue = dlg.textValue();
    if (newValue.isEmpty()) return;
    debugger.editRegister(0, QString("R%1: $%2").arg(regIndex).arg(newValue.toInt(nullptr, 16), 8, 16, QChar('0')).toUpper());
    updateUI();
}

// Slot: Edit a register in bank 1
void MainWindow::onRegBank1ItemDoubleClicked(QTreeWidgetItem* item, int column) {
    if (column != 1) return; // Only allow editing the value column
    int regIndex = regBank1->indexOfTopLevelItem(item);
    QString currentValue = item->text(2);
    bool ok = false;
    QInputDialog dlg(this);
    dlg.setWindowTitle("Edit Register (Bank 1)");
    dlg.setLabelText(QString("Current value: %1\nEnter new value (hex, without $):").arg(currentValue));
    dlg.setTextValue(currentValue.remove('$'));
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.resize(600, dlg.sizeHint().height()); // Make the dialog wider
    if (dlg.exec() != QDialog::Accepted) return;
    QString newValue = dlg.textValue();
    if (newValue.isEmpty()) return;
    debugger.editRegister(1, QString("R%1: $%2").arg(regIndex).arg(newValue.toInt(nullptr, 16), 8, 16, QChar('0')).toUpper());
    updateUI();
}

// Slot: Set a breakpoint in the code view
void MainWindow::onCodeViewItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item) return;
    debugger.setBreakpoint(item->text(2)); // Use column 2 for address
    updateUI();
}

// Slot: Edit a register in bank 0 via label click
void MainWindow::onRegBank0LabelClicked() {
    bool ok = false;
    int regIndex = QInputDialog::getInt(this, "Edit Register (Bank 0)", "Register index (0-31):", 0, 0, 31, 1, &ok);
    if (!ok) return;
    QString currentValue = regBank0->topLevelItem(regIndex)->text(1);
    bool ok2 = false;
    QString newValue = QInputDialog::getText(this, "Edit Register (Bank 0)",
        QString("Current value: %1\nEnter new value (hex, without $):").arg(currentValue),
        QLineEdit::Normal, currentValue.remove('$'), &ok2);
    if (!ok2 || newValue.isEmpty()) return;
    debugger.editRegister(0, QString("R%1: $%2").arg(regIndex).arg(newValue.toInt(nullptr, 16), 8, 16, QChar('0')).toUpper());
    updateUI();
}

// Slot: Edit a register in bank 1 via label click
void MainWindow::onRegBank1LabelClicked() {
    bool ok = false;
    int regIndex = QInputDialog::getInt(this, "Edit Register (Bank 1)", "Register index (0-31):", 0, 0, 31, 1, &ok);
    if (!ok) return;
    QString currentValue = regBank1->topLevelItem(regIndex)->text(1);
    bool ok2 = false;
    QString newValue = QInputDialog::getText(this, "Edit Register (Bank 1)",
        QString("Current value: %1\nEnter new value (hex, without $):").arg(currentValue),
        QLineEdit::Normal, currentValue.remove('$'), &ok2);
    if (!ok2 || newValue.isEmpty()) return;
    debugger.editRegister(1, QString("R%1: $%2").arg(regIndex).arg(newValue.toInt(nullptr, 16), 8, 16, QChar('0')).toUpper());
    updateUI();
}

// Event filter for handling mouse button release events on labels
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        if (obj == regBank0Label) {
            onRegBank0LabelClicked();
            return true;
        }
        if (obj == regBank1Label) {
            onRegBank1LabelClicked();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// Key press event handler for shortcut keys
void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->modifiers() == Qt::NoModifier) {
        switch (event->key()) {
        case Qt::Key_F5:
            onRun();
            break;
        case Qt::Key_F11:
            onStep();
            break;
        case Qt::Key_F7:
            onSkip();
            break;
        case Qt::Key_F3:
            onReset();
            break;
        default:
            QMainWindow::keyPressEvent(event);
            return;
        }
        event->accept();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}
