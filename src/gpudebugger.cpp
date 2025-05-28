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
    : isReadyToReset(false), isReadyToRun(false), isReadyToStep(false), isReadyToSkip(false), progress(0), pc(0), programSize(0) {
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

    programSize = static_cast<int>(fileSize); // <-- Use member variable
    int readOffset = LoadAddress;

    QByteArray header = file.read(12);
    if (header.size() == 12) {
        int value = (static_cast<uint8_t>(header[0]) << 24) |
                    (static_cast<uint8_t>(header[1]) << 16) |
                    (static_cast<uint8_t>(header[2]) << 8)  |
                    (static_cast<uint8_t>(header[3]));
        if (value == 0x42533934) {
            int newAddr = (static_cast<uint8_t>(header[4]) << 24) |
                          (static_cast<uint8_t>(header[5]) << 16) |
                          (static_cast<uint8_t>(header[6]) << 8)  |
                          (static_cast<uint8_t>(header[7]));
            LoadAddress = newAddr;
            readOffset = LoadAddress;
            programSize -= 12; // <-- Update member variable
            QByteArray rest = file.read(programSize);
            if (rest.size() != programSize) {
                QMessageBox::critical(nullptr, "Error", "Error while loading file.");
                file.close();
                return false;
            }
            if (readOffset + programSize > MemorySize) {
                QMessageBox::critical(nullptr, "Error", "File too large for memory.");
                file.close();
                return false;
            }
            std::copy(rest.begin(), rest.end(), MemoryBuffer.begin() + readOffset);
        } else {
            file.seek(0);
            QByteArray all = file.read(programSize);
            if (all.size() != programSize) {
                QMessageBox::critical(nullptr, "Error", "Error while loading file.");
                file.close();
                return false;
            }
            if (readOffset + programSize > MemorySize) {
                QMessageBox::critical(nullptr, "Error", "File too large for memory.");
                file.close();
                return false;
            }
            std::copy(all.begin(), all.end(), MemoryBuffer.begin() + readOffset);
        }
    } else {
        file.seek(0);
        QByteArray all = file.read(programSize);
        if (all.size() != programSize) {
            QMessageBox::critical(nullptr, "Error", "Error while loading file.");
            file.close();
            return false;
        }
        if (readOffset + programSize > MemorySize) {
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
    codeViewLines = disassemble(address, programSize);
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
    return codeViewLines;
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

int GPUDebugger::getProgramSize() const {
    return programSize;
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

QStringList GPUDebugger::disassemble(int loadAddress, int programSize) const {
    QStringList result;
    const uint8_t* walk = MemoryBuffer.data() + loadAddress;
    int size = programSize;
    int adrs = loadAddress;
    while (size > 1) {
        int ecart = 0;
        uint8_t w1 = *walk++;
        uint8_t w2 = *walk++;
        ecart += 2;
        uint8_t opcode = w1 >> 2;
        uint8_t reg1 = ((w1 << 3) & 31) | (w2 >> 5);
        uint8_t reg2 = w2 & 31;
        QString instr, js;
        switch (opcode) {
        case 22: instr = QString("abs    r%1").arg(reg2); break;
        case 0: instr = QString("add    r%1,r%2").arg(reg1).arg(reg2); break;
        case 1: instr = QString("addc   r%1,r%2").arg(reg1).arg(reg2); break;
        case 2: instr = QString("addq   #%1,r%2").arg(reg1 == 0 ? 32 : reg1).arg(reg2); break;
        case 3: instr = QString("addqt  #%1,r%2").arg(reg1 == 0 ? 32 : reg1).arg(reg2); break;
        case 9: instr = QString("and    r%1,r%2").arg(reg1).arg(reg2); break;
        case 15: instr = QString("bclr   #%1,r%2").arg(reg1).arg(reg2); break;
        case 14: instr = QString("bset   #%1,r%2").arg(reg1).arg(reg2); break;
        case 13: instr = QString("btst   #%1,r%2").arg(reg1).arg(reg2); break;
        case 30: instr = QString("cmp    r%1,r%2").arg(reg1).arg(reg2); break;
        case 31: instr = QString("cmpq   #%1,r%2").arg(reg1).arg(reg2); break;
        case 21: instr = QString("div    r%1,r%2").arg(reg1).arg(reg2); break;
        case 17: instr = QString("imult  r%1,r%2").arg(reg1).arg(reg2); break;
        case 53:
            instr = "jr     ";
            js = QString::fromStdString(GetJumpFlag(reg2));
            if (!js.isEmpty()) instr += js + ",$";
            instr += (reg1 > 15)
                ? QString("%1").arg(adrs - ((32 - reg1) * 2), 8, 16, QChar('0'))
                : QString("%1").arg(adrs + (reg1 * 2), 8, 16, QChar('0'));
            break;
        case 52:
            instr = "jump   ";
            js = QString::fromStdString(GetJumpFlag(reg2));
            if (!js.isEmpty()) instr += js + ",";
            instr += QString("(r%1)").arg(reg1);
            break;
        case 41: instr = QString("load   (r%1),r%2").arg(reg1).arg(reg2); break;
        case 43: instr = QString("load   (r14+%1),r%2").arg(reg1).arg(reg2); break;
        case 44: instr = QString("load   (r15+%1),r%2").arg(reg1).arg(reg2); break;
        case 58: instr = QString("load   (r14+r%1),r%2").arg(reg1).arg(reg2); break;
        case 59: instr = QString("load   (r15+r%1),r%2").arg(reg1).arg(reg2); break;
        case 39: instr = QString("loadb  (r%1),r%2").arg(reg1).arg(reg2); break;
        case 40: instr = QString("loadw  (r%1),r%2").arg(reg1).arg(reg2); break;
        case 42: instr = QString("loadp  (r%1),r%2").arg(reg1).arg(reg2); break;
        case 34: instr = QString("move   r%1,r%2").arg(reg1).arg(reg2); break;
        case 51: instr = QString("move   PC,r%1").arg(reg2); break;
        case 37: instr = QString("movefa r%1,r%2").arg(reg1).arg(reg2); break;
        case 38: {
            int value = (walk[0] << 8) | walk[1] | (walk[2] << 24) | (walk[3] << 16);
            walk += 4; ecart += 4;
            instr = QString("movei  #$%1,r%2").arg(value, 8, 16, QChar('0')).arg(reg2);
            break;
        }
        case 35: instr = QString("moveq  #%1,r%2").arg(reg1).arg(reg2); break;
        case 36: instr = QString("moveta r%1,r%2").arg(reg1).arg(reg2); break;
        case 16: instr = QString("mult   r%1,r%2").arg(reg1).arg(reg2); break;
        case 8: instr = QString("neg    r%1").arg(reg2); break;
        case 12: instr = QString("not    r%1").arg(reg2); break;
        case 10: instr = QString("or     r%1,r%2").arg(reg1).arg(reg2); break;
        case 28: instr = QString("ror    r%1,r%2").arg(reg1).arg(reg2); break;
        case 29: instr = QString("rorq   #%1,r%2").arg(reg1).arg(reg2); break;
        case 32: instr = QString("sat8   r%1").arg(reg2); break;
        case 33: instr = QString("sat16  r%1").arg(reg2); break;
        case 62: instr = QString("sat24  r%1").arg(reg2); break;
        case 23: instr = QString("sh     r%1,r%2").arg(reg1).arg(reg2); break;
        case 26: instr = QString("sha    r%1,r%2").arg(reg1).arg(reg2); break;
        case 27: instr = QString("sharq  #%1,r%2").arg(reg1).arg(reg2); break;
        case 24: instr = QString("shlq   #%1,r%2").arg(32 - reg1).arg(reg2); break;
        case 25: instr = QString("shrq   #%1,r%2").arg(reg1).arg(reg2); break;
        case 47: instr = QString("store  r%1,(r%2)").arg(reg2).arg(reg1); break;
        case 49: instr = QString("store  r%1,(r14+%2)").arg(reg2).arg(reg1); break;
        case 50: instr = QString("store  r%1,(r15+%2)").arg(reg2).arg(reg1); break;
        case 60: instr = QString("store  r%1,(r14+r%2)").arg(reg2).arg(reg1); break;
        case 61: instr = QString("store  r%1,(r15+r%2)").arg(reg2).arg(reg1); break;
        case 45: instr = QString("storeb r%1,(r%2)").arg(reg2).arg(reg1); break;
        case 48: instr = QString("storep r%1,(r%2)").arg(reg2).arg(reg1); break;
        case 46: instr = QString("storew r%1,(r%2)").arg(reg2).arg(reg1); break;
        case 4: instr = QString("sub    r%1,r%2").arg(reg1).arg(reg2); break;
        case 5: instr = QString("subc   r%1,r%2").arg(reg1).arg(reg2); break;
        case 6: instr = QString("subq   #%1,r%2").arg(reg1 == 0 ? 32 : reg1).arg(reg2); break;
        case 7: instr = QString("subqt  #%1,r%2").arg(reg1 == 0 ? 32 : reg1).arg(reg2); break;
        case 63: instr = (reg1 == 0)
            ? QString("pack   r%1").arg(reg2)
            : QString("unpack r%1").arg(reg2); break;
        case 11: instr = QString("xor    r%1,r%2").arg(reg1).arg(reg2); break;
        default: instr = "unknown"; break;
        }
        result << QString("$%1: %2").arg(adrs, 8, 16, QChar('0')).arg(instr);
        size -= ecart;
        adrs += ecart;
    }
    return result;
}

std::string GetJumpFlag(uint8_t flag) {
    switch (flag) {
    case 0x0: return "";
    case 0x1: return "NE";
    case 0x2: return "EQ";
    case 0x4: return "CC";
    case 0x5: return "HI";
    case 0x6: return "NC Z";
    case 0x8: return "CS";
    case 0x9: return "C NZ";
    case 0xA: return "C Z";
    case 0x14: return "GE";
    case 0x15: return "GT";
    case 0x16: return "NN Z";
    case 0x18: return "LE";
    case 0x19: return "LT";
    case 0x1A: return "N Z";
    case 0x1F: return "NOT";
    default: return "ERR";
    }
}
