#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QTreeWidget>
#include <QProgressBar>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include "gpudebugger.h"

// MainWindow: The main Qt5 window for the Jaguar GPU Simulator/Debugger.
// This class sets up the UI and connects user actions to the GPUDebugger logic.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Slot for loading a BIN file
    void onLoadBin();
    // Slot for running the GPU program
    void onRun();
    // Slot for stepping one instruction
    void onStep();
    // Slot for skipping one instruction (without execution)
    void onSkip();
    // Slot for resetting the GPU state
    void onReset();
    // Slot for exiting the application
    void onExit();
    // Slot for switching to GPU mode
    void onGPUMode();
    // Slot for switching to DSP mode
    void onDSPMode();
    // Slot for updating PC from the line edit
    void onPCEditReturnPressed();
    // Slot for editing a register in bank 0
    void onRegBank0ItemDoubleClicked(QTreeWidgetItem*, int);
    // Slot for editing a register in bank 1
    void onRegBank1ItemDoubleClicked(QTreeWidgetItem*, int);
    // Slot for setting a breakpoint in the code view
    void onCodeViewItemDoubleClicked(QTreeWidgetItem*, int);

private:
    // UI widgets
    QLabel *regBank0Label, *regBank1Label, *codeLabel, *flagStatusLabel, *gpubpLabel, *g_hidataLabel, *g_remainLabel, *jumpLabel, *label4, *label5;
    QTreeWidget *regBank0, *regBank1, *codeView;
    QPushButton *loadBinBtn, *runBtn, *stepBtn, *skipBtn, *resetBtn, *exitBtn;
    QLineEdit *loadAddressEdit, *pcEdit;
    QCheckBox *memWarn;
    QRadioButton *gpuMode, *dspMode;
    QProgressBar *progress;
    QFileDialog *openDialog;

    GPUDebugger debugger; // The core logic handler

    // Sets up the UI layout and widgets
    void setupUI();
    // Updates the UI to reflect the current debugger state
    void updateUI();
};