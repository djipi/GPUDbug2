#include <QString>
#include <QStringList>
#include <QDebug> // For debugging purposes
#include <QFile>
#include <QMessageBox>
#include <QByteArray>
#include <vector>
#include "gpudebugger.h"

const int MemorySize = 0xF1D000; // Address limit for the RISC processor
std::vector<uint8_t> MemoryBuffer(MemorySize);

// Constructor: Initialize the state
GPUDebugger::GPUDebugger() 
    : isReadyToReset(false), isReadyToRun(false), isReadyToStep(false), isReadyToSkip(false), progress(0), pc(0) {
    regBank0.resize(32, 0); // Initialize regBank0 with 32 elements, all set to 0
    regBank1.resize(32, 0); // Initialize regBank1 with 32 elements, all set to 0
}

// Implementation of canReset
bool GPUDebugger::canReset() const {
    // Logic to determine if reset is allowed
    // For example, allow reset if the debugger is not running
    return isReadyToReset;
}

bool GPUDebugger::loadBin(const QString& filename, int address) {
    int LoadAddress = address;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(nullptr, "Error", "Error while loading file.");
        return false;
    }

    qint64 fileSize = file.size();
    if (fileSize > (MemorySize - LoadAddress)) {
        QMessageBox::critical(nullptr, "Error", "File too large!");
        file.close();
        return false;
    }

    int ProgramSize = static_cast<int>(fileSize);
    int readOffset = LoadAddress;

    QByteArray header = file.read(12);
    if (header.size() == 12) {
        // Check for magic number "BS94" ($42533934)
        int value = (static_cast<uint8_t>(header[0]) << 24) |
                    (static_cast<uint8_t>(header[1]) << 16) |
                    (static_cast<uint8_t>(header[2]) << 8)  |
                    (static_cast<uint8_t>(header[3]));
        if (value == 0x42533934) {
            // Next 4 bytes: new LoadAddress
            int newAddr = (static_cast<uint8_t>(header[4]) << 24) |
                          (static_cast<uint8_t>(header[5]) << 16) |
                          (static_cast<uint8_t>(header[6]) << 8)  |
                          (static_cast<uint8_t>(header[7]));
            LoadAddress = newAddr;
            readOffset = LoadAddress;
            // ProgramSize is reduced by 12 bytes (header)
            ProgramSize -= 12;
            // Read the rest of the file into memory
            QByteArray rest = file.read(ProgramSize);
            if (rest.size() != ProgramSize) {
                QMessageBox::critical(nullptr, "Error", "Error while loading file.");
                file.close();
                return false;
            }
            if (readOffset + ProgramSize > MemorySize) {
                QMessageBox::critical(nullptr, "Error", "File too large for memory.");
                file.close();
                return false;
            }
            std::copy(rest.begin(), rest.end(), MemoryBuffer.begin() + readOffset);
        } else {
            // Not a "BS94" file, copy header + rest
            file.seek(0);
            QByteArray all = file.read(ProgramSize);
            if (all.size() != ProgramSize) {
                QMessageBox::critical(nullptr, "Error", "Error while loading file.");
                file.close();
                return false;
            }
            if (readOffset + ProgramSize > MemorySize) {
                QMessageBox::critical(nullptr, "Error", "File too large for memory.");
                file.close();
                return false;
            }
            std::copy(all.begin(), all.end(), MemoryBuffer.begin() + readOffset);
        }
    } else {
        // File smaller than 12 bytes, just copy all
        file.seek(0);
        QByteArray all = file.read(ProgramSize);
        if (all.size() != ProgramSize) {
            QMessageBox::critical(nullptr, "Error", "Error while loading file.");
            file.close();
            return false;
        }
        if (readOffset + ProgramSize > MemorySize) {
            QMessageBox::critical(nullptr, "Error", "File too large for memory.");
            file.close();
            return false;
        }
        std::copy(all.begin(), all.end(), MemoryBuffer.begin() + readOffset);
    }

    file.close();
    isReadyToRun = true;
    isReadyToStep = true;
    isReadyToSkip = true;
    return true;
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
    QStringList list;
    for (int i = 0; i < 32; ++i) {
        int value = 0;
        if (bank == 0 && regBank0.size() == 32) value = regBank0[i];
        else if (bank == 1 && regBank1.size() == 32) value = regBank1[i];
        // Register name: no space, always R0..R31; value: aligned to 8 hex digits
        list << QString("R%1: $%2")
            .arg(i, 0, 10) // width 0: no padding, so no space in name
            .arg(value, 8, 16, QChar('0'))
            .toUpper();
    }
    return list;
}

QStringList GPUDebugger::getCodeView() const {
    // Return an empty list if no file has been loaded
    if (!isReadyToRun && !isReadyToStep && !isReadyToSkip) {
        return QStringList();
    }
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
    // Parse "R<num>: $<hexvalue>"
    QRegExp rx("^R(\\d+): \\$([0-9A-Fa-f]+)$");
    if (!rx.exactMatch(value)) {
        qDebug() << "Invalid register format:" << value;
        return;
    }
    int regIndex = rx.cap(1).toInt();
    bool ok = false;
    int regValue = rx.cap(2).toInt(&ok, 16);
    if (!ok || regIndex < 0 || regIndex >= 32) {
        qDebug() << "Invalid register value or index:" << value;
        return;
    }
    if (bank == 0 && regBank0.size() == 32) {
        regBank0[regIndex] = regValue;
    }
    else if (bank == 1 && regBank1.size() == 32) {
        regBank1[regIndex] = regValue;
    }
    else {
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