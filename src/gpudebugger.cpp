#include <QString>
#include <QStringList>
#include <QDebug> // For debugging purposes
#include "gpudebugger.h"

// Constructor: Initialize the state
GPUDebugger::GPUDebugger() : isReadyToReset(false), isReadyToRun(false), isReadyToStep(false), isReadyToSkip(false), progress(0), pc(0) {}

// Implementation of canReset
bool GPUDebugger::canReset() const {
    // Logic to determine if reset is allowed
    // For example, allow reset if the debugger is not running
    return isReadyToReset;
}

bool GPUDebugger::loadBin(const QString& filename, int address) {
    // Implementation for loading a BIN file
    isReadyToRun = true;
    isReadyToStep = true;
    isReadyToSkip = true; // Enable skipping after loading a BIN file
    return true; // Assume success for simplicity
}

void GPUDebugger::reset() {
    // Reset the debugger state
    isReadyToRun = false;
    isReadyToStep = false;
    isReadyToSkip = false; // Disable skipping after reset
    pc = 0;
    progress = 0;
    // Reset logic...
    isReadyToReset = false; // Update the state after reset
}

void GPUDebugger::step() {
    // Implementation for stepping one instruction
    if (isReadyToStep) {
        pc += 4; // Example: Increment PC by 4
    }
}

void GPUDebugger::run() {
    // Implementation for running the program
    if (isReadyToRun) {
        progress = 100; // Example: Set progress to 100%
    }
}

void GPUDebugger::skip() {
    // Implementation for skipping one instruction
    if (isReadyToSkip) {
        pc += 4; // Example: Increment PC by 4 without execution
    }
}

bool GPUDebugger::canRun() const {
    return isReadyToRun;
}

bool GPUDebugger::canStep() const {
    return isReadyToStep;
}

bool GPUDebugger::canSkip() const {
    return isReadyToSkip; // Return whether skipping is allowed
}

QStringList GPUDebugger::getRegBank(int bank) const {
    // Example implementation
    return QStringList() << "R0: $00000000" << "R1: $00000001";
}

QStringList GPUDebugger::getCodeView() const {
    // Example implementation
    return QStringList() << "MOV R0, R1" << "ADD R2, R3";
}

QString GPUDebugger::getFlags() const {
    return "Flags: Z:0 N:0 C:0";
}

QString GPUDebugger::getPC() const {
    return QString("$%1").arg(pc, 8, 16, QChar('0')).toUpper();
}

QString GPUDebugger::getJump() const {
    return "$00000000";
}

QString GPUDebugger::getRemain() const {
    return "$00000000";
}

QString GPUDebugger::getHiData() const {
    return "$00000000";
}

QString GPUDebugger::getBP() const {
    return "$00000000";
}

int GPUDebugger::getProgress() const {
    return progress;
}

void GPUDebugger::setPC(const QString& pcValue) {
    bool ok = false;
    QString modifiablePCValue = pcValue; // Create a modifiable copy
    pc = modifiablePCValue.remove('$').toInt(&ok, 16);
    if (!ok) pc = 0;
}

void GPUDebugger::editRegister(int bank, const QString& value) {
    // Example implementation: Log the bank and value for debugging
    qDebug() << "Editing register in bank" << bank << "with value" << value;

    // Add logic to modify the register value in the specified bank
    // For example:
    if (bank == 0) {
        // Modify register in bank 0
    } else if (bank == 1) {
        // Modify register in bank 1
    } else {
        qDebug() << "Invalid bank specified.";
    }
}

void GPUDebugger::setGPUMode(bool isGPUMode) {
    // Logic to switch between GPU and DSP modes
    if (isGPUMode) {
        // Set GPU-specific state
        // Example: Update internal flags or configurations
    } else {
        // Set DSP-specific state
        // Example: Update internal flags or configurations
    }
}

void GPUDebugger::setBreakpoint(const QString& address) {
    // Convert the address from QString to an integer
    bool ok = false;
    QString modifiableAddress = address; // Create a modifiable copy
    int breakpointAddress = modifiableAddress.remove('$').toInt(&ok, 16);
    if (ok) {
        // Logic to set the breakpoint (e.g., add to a list of breakpoints)
        qDebug() << "Breakpoint set at address:" << QString::number(breakpointAddress, 16).toUpper();
        // Add your breakpoint handling logic here
    } else {
        qDebug() << "Invalid address format for breakpoint:" << address;
    }
}