#include <QString>
#include <QStringList>
#include <QDebug> // For debugging purposes
#include <QFile>
#include <QMessageBox>
#include <QByteArray>
#include <vector>
#include <sstream>
#include <iomanip>
#include <functional>
#include "gpudebugger.h"

// Add this near the top, after the includes:
template <typename T>
const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

const int MemorySize = 0xF1D000; // Address limit for the RISC processor
std::vector<uint8_t> MemoryBuffer(MemorySize);

// Constructor: Initialize the state
GPUDebugger::GPUDebugger(QObject* parent)
    : QObject(parent), // Initialize the QObject base class
      isReadyToReset(false),
      isReadyToRun(false),
      isReadyToStep(false),
      isReadyToSkip(false),
      progress(0),
      pc(0),
      programSize(0) {
    regBank[0].resize(32, 0);
    regBank[1].resize(32, 0);
}

// Destructor: Clean up resources if needed
GPUDebugger::~GPUDebugger() {
    // No dynamic memory to clean up, but can be used for future extensions
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

    loadAddress = LoadAddress; // <-- Store the final load address

    file.close();
    isReadyToRun = true;
    isReadyToStep = true;
    isReadyToSkip = true;
    codeViewLines = disassemble(address, programSize);
    return true;
}

void GPUDebugger::reset() {
    CurRegBank = 0; // Set current register bank to 0
    pc = loadAddress; // Set PC to the last loading address
    flagZ = 0;
    flagN = 0;
    flagC = 0;
    std::fill(regBank[0].begin(), regBank[0].end(), 0);
    std::fill(regBank[1].begin(), regBank[1].end(), 0);
    jumpbuffered = false;
    // Reset logic...
}

void GPUDebugger::step(uint16_t w, bool exec) {
    // Implementation for stepping one instruction
    if (isReadyToStep) {
        uint8_t opcode = w >> 10;
        uint8_t reg1 = (w >> 5) & 31;
        uint8_t reg2 = w & 31;
        int RegTrace = 1;
/*
        TreeView* TVReg = nullptr;

        if (opcode == 36) // moveta
            TVReg = (CurRegBank == 0) ? &GDBUG.RegBank1 : &GDBUG.RegBank0;
        else
            TVReg = (CurRegBank == 0) ? &GDBUG.RegBank0 : &GDBUG.RegBank1;

        // Clear Reg Trace
        for (int i = 0; i < 32; ++i)
            TVReg->Items[i].ImageIndex = IMG_NODE_NOTHING;
*/
        pc += 2;

        if (exec) {
            switch (opcode) {
            case 22: // abs
                flagN = 0;
                flagC = (regBank[CurRegBank][reg2] < 0) ? 1 : 0;
                regBank[CurRegBank][reg2] = std::abs(regBank[CurRegBank][reg2]);
                flagZ = (regBank[CurRegBank][reg2] == 0) ? 1 : 0;
                break;
            case 0: // add
                Update_C_Flag_Add(regBank[CurRegBank][reg1], regBank[CurRegBank][reg2]);
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg1] + regBank[CurRegBank][reg2];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 1: // addc
                Update_C_Flag_Add(regBank[CurRegBank][reg1] + flagC, regBank[CurRegBank][reg2]);
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg1] + flagC + regBank[CurRegBank][reg2];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 2: // addq
                if (reg1 == 0) reg1 = 32;
                Update_C_Flag_Add(reg1, regBank[CurRegBank][reg2]);
                regBank[CurRegBank][reg2] = reg1 + regBank[CurRegBank][reg2];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 3: // addqt
                if (reg1 == 0) reg1 = 32;
                regBank[CurRegBank][reg2] = reg1 + regBank[CurRegBank][reg2];
                break;
            case 9: // and
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg1] & regBank[CurRegBank][reg2];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 15: // bclr
                regBank[CurRegBank][reg2] &= ~(1 << reg1);
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 14: // bset
                regBank[CurRegBank][reg2] |= (1 << reg1);
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 13: // btst
                RegTrace = 3;
                flagZ = ((regBank[CurRegBank][reg2] & (1 << reg1)) == 0) ? 1 : 0;
                break;
            case 30: // cmp
                RegTrace = 3;
                Update_C_Flag_Sub(regBank[CurRegBank][reg1], regBank[CurRegBank][reg2]);
                Update_ZN_Flag(regBank[CurRegBank][reg2] - regBank[CurRegBank][reg1]);
                break;
            case 31: // cmpq
                RegTrace = 3;
                Update_C_Flag_Sub(reg1, regBank[CurRegBank][reg2]);
                Update_ZN_Flag(regBank[CurRegBank][reg2] - reg1);
                break;
            case 21: { // div
                unsigned u32_1 = regBank[CurRegBank][reg1];
                unsigned u32_2 = regBank[CurRegBank][reg2];
                unsigned u32_3 = (u32_1 != 0) ? (u32_2 / u32_1) : 0;
                regBank[CurRegBank][reg2] = u32_3;
                int temp = (u32_1 != 0) ? (u32_2 % u32_1) : 0;
                if ((u32_3 & 1) == 0)
                    WriteLong(G_REMAIN, temp - u32_1);
                else
                    WriteLong(G_REMAIN, temp);
                break;
            }
            case 17: { // imult
                int temp = regBank[CurRegBank][reg1] & 0xFFFF;
                if (temp > 32767) temp -= 65536;
                regBank[CurRegBank][reg2] &= 0xFFFF;
                if (regBank[CurRegBank][reg2] > 32767) regBank[CurRegBank][reg2] -= 65536;
                regBank[CurRegBank][reg2] = temp * regBank[CurRegBank][reg2];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            }
            case 53: // jr
                RegTrace = 0;
                if (JumpConditionMatch(reg2)) {
                    if (reg1 > 15)
                        JMPPC = pc - ((32 - reg1) * 2);
                    else
                        JMPPC = pc + (reg1 * 2);
                    jumpbuffered = true;
                    JumpBuffLabelUpdate();
                }
                break;
            case 52: // jump
                RegTrace = 0;
                if (JumpConditionMatch(reg2)) {
                    JMPPC = regBank[CurRegBank][reg1];
                    jumpbuffered = true;
                    JumpBuffLabelUpdate();
                }
                break;
            case 41: // load
                regBank[CurRegBank][reg2] = ReadLong(regBank[CurRegBank][reg1]);
                break;
            case 43: // load r14+n
                regBank[CurRegBank][reg2] = ReadLong(regBank[CurRegBank][14] + reg1 * 4);
                break;
            case 44: // load r15+n
                regBank[CurRegBank][reg2] = ReadLong(regBank[CurRegBank][15] + reg1 * 4);
                break;
            case 58: // load r14+rn
                regBank[CurRegBank][reg2] = ReadLong(regBank[CurRegBank][14] + regBank[CurRegBank][reg1]);
                break;
            case 59: // load r15+rn
                regBank[CurRegBank][reg2] = ReadLong(regBank[CurRegBank][15] + regBank[CurRegBank][reg1]);
                break;
            case 39: // loadb
                regBank[CurRegBank][reg2] = ReadByte(regBank[CurRegBank][reg1]);
                break;
            case 40: // loadw
                regBank[CurRegBank][reg2] = ReadWord(regBank[CurRegBank][reg1], false);
                break;
            case 42: // loadp
                WriteLong(G_HIDATA, ReadLong(regBank[CurRegBank][reg1]));
                regBank[CurRegBank][reg2] = ReadLong(regBank[CurRegBank][reg1] + 4);
                break;
            case 34: // move
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg1];
                break;
            case 51: // move pc
                regBank[CurRegBank][reg2] = pc - 2;
                break;
            case 37: // movefa
                regBank[CurRegBank][reg2] = regBank[CurRegBank ^ 1][reg1];
                break;
            case 38: { // movei
                regBank[CurRegBank][reg2] = ReadWord(pc, true);
                regBank[CurRegBank][reg2] |= (ReadWord(pc + 2, true) << 16);
                pc += 4;
                break;
            }
            case 35: // moveq
                regBank[CurRegBank][reg2] = reg1;
                break;
            case 36: // moveta
                regBank[CurRegBank ^ 1][reg2] = regBank[CurRegBank][reg1];
                break;
            case 16: { // mult
                uint16_t u16_1 = regBank[CurRegBank][reg1];
                uint16_t u16_2 = regBank[CurRegBank][reg2];
                regBank[CurRegBank][reg2] = u16_1 * u16_2;
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            }
            case 8: // neg
                regBank[CurRegBank][reg2] = -regBank[CurRegBank][reg2];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 12: // not
                regBank[CurRegBank][reg2] = ~regBank[CurRegBank][reg2];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 10: // or
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg1] | regBank[CurRegBank][reg2];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 28: // ror
                flagC = (regBank[CurRegBank][reg2] >> 31) & 1;
                reg1 = regBank[CurRegBank][reg1] & 31;
                regBank[CurRegBank][reg2] = (regBank[CurRegBank][reg2] >> reg1) | (regBank[CurRegBank][reg2] << (32 - reg1));
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 29: // rorq
                flagC = (regBank[CurRegBank][reg2] >> 31) & 1;
                regBank[CurRegBank][reg2] = (regBank[CurRegBank][reg2] >> reg1) | (regBank[CurRegBank][reg2] << (32 - reg1));
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 32: // sat8
                regBank[CurRegBank][reg2] = clamp(regBank[CurRegBank][reg2], 0, 255);
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 33: // sat16
                regBank[CurRegBank][reg2] = clamp(regBank[CurRegBank][reg2], 0, 65535);
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 62: // sat24
                regBank[CurRegBank][reg2] = clamp(regBank[CurRegBank][reg2], 0, 16777215);
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 23: { // sh
                int temp = regBank[CurRegBank][reg1];
                if (temp > 32) temp = 0;
                if (temp < -32) temp = 0;
                if (temp >= 0) {
                    flagC = regBank[CurRegBank][reg2] & 1;
                    regBank[CurRegBank][reg2] >>= temp;
                }
                else {
                    flagC = (regBank[CurRegBank][reg2] >> 31) & 1;
                    regBank[CurRegBank][reg2] <<= -temp;
                }
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            }
            case 26: { // sha
                int temp = regBank[CurRegBank][reg1];
                if (temp > 32) temp = 0;
                if (temp < -32) temp = 0;
                if (temp >= 0) {
                    flagC = regBank[CurRegBank][reg2] & 1;
                    if (regBank[CurRegBank][reg2] < 0)
                        regBank[CurRegBank][reg2] = (0xFFFFFFFF << (32 - temp)) | (regBank[CurRegBank][reg2] >> temp);
                    else
                        regBank[CurRegBank][reg2] >>= temp;
                }
                else {
                    flagC = (regBank[CurRegBank][reg2] >> 31) & 1;
                    regBank[CurRegBank][reg2] <<= -temp;
                }
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            }
            case 27: // sharq
                flagC = regBank[CurRegBank][reg2] & 1;
                if (regBank[CurRegBank][reg2] < 0)
                    regBank[CurRegBank][reg2] = (0xFFFFFFFF << (32 - reg1)) | (regBank[CurRegBank][reg2] >> reg1);
                else
                    regBank[CurRegBank][reg2] >>= reg1;
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 24: // shlq
                reg1 = 32 - reg1;
                flagC = (regBank[CurRegBank][reg2] >> 31) & 1;
                regBank[CurRegBank][reg2] <<= reg1;
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 25: // shrq
                flagC = regBank[CurRegBank][reg2] & 1;
                regBank[CurRegBank][reg2] >>= reg1;
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 47: // store
                RegTrace = 2;
                WriteLong(regBank[CurRegBank][reg1], regBank[CurRegBank][reg2]);
                break;
            case 49: // store r14+n
                RegTrace = 2;
                WriteLong(regBank[CurRegBank][14] + reg1 * 4, regBank[CurRegBank][reg2]);
                break;
            case 50: // store r15+n
                RegTrace = 2;
                WriteLong(regBank[CurRegBank][15] + reg1 * 4, regBank[CurRegBank][reg2]);
                break;
            case 60: // store r14+rn
                RegTrace = 2;
                WriteLong(regBank[CurRegBank][14] + regBank[CurRegBank][reg1], regBank[CurRegBank][reg2]);
                break;
            case 61: // store r15+rn
                RegTrace = 2;
                WriteLong(regBank[CurRegBank][15] + regBank[CurRegBank][reg1], regBank[CurRegBank][reg2]);
                break;
            case 45: // storeb
                RegTrace = 2;
                WriteByte(regBank[CurRegBank][reg1], regBank[CurRegBank][reg2]);
                break;
            case 48: // storep
                RegTrace = 2;
                WriteLong(regBank[CurRegBank][reg1], ReadLong(G_HIDATA));
                WriteLong(regBank[CurRegBank][reg1] + 4, regBank[CurRegBank][reg2]);
                break;
            case 46: // storew
                RegTrace = 2;
                WriteWord(regBank[CurRegBank][reg1], regBank[CurRegBank][reg2]);
                break;
            case 4: // sub
                Update_C_Flag_Sub(regBank[CurRegBank][reg1], regBank[CurRegBank][reg2]);
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg2] - regBank[CurRegBank][reg1];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 5: // subc
                Update_C_Flag_Sub(regBank[CurRegBank][reg1], regBank[CurRegBank][reg2] + flagC);
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg2] - regBank[CurRegBank][reg1] - flagC;
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 6: // subq
                if (reg1 == 0) reg1 = 32;
                Update_C_Flag_Sub(reg1, regBank[CurRegBank][reg2]);
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg2] - reg1;
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            case 7: // subqt
                if (reg1 == 0) reg1 = 32;
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg2] - reg1;
                break;
            case 63: // pack/unpack
                if (reg1 == 0)
                    regBank[CurRegBank][reg2] =
                    ((regBank[CurRegBank][reg2] & 0x3C00000) >> 10) |
                    ((regBank[CurRegBank][reg2] & 0x001E000) >> 5) |
                    (regBank[CurRegBank][reg2] & 0x00000FF);
                else
                    regBank[CurRegBank][reg2] =
                    ((regBank[CurRegBank][reg2] << 10) & 0x3C00000) |
                    ((regBank[CurRegBank][reg2] << 5) & 0x001E000) |
                    (regBank[CurRegBank][reg2] & 0x00000FF);
                break;
            case 11: // xor
                regBank[CurRegBank][reg2] = regBank[CurRegBank][reg1] ^ regBank[CurRegBank][reg2];
                Update_ZN_Flag(regBank[CurRegBank][reg2]);
                break;
            default:
                break;
            }
/*
            switch (RegTrace) {
            case 0:
                TVReg->Items[reg2].ImageIndex = IMG_NODE_NOTHING;
                break;
            case 1:
                TVReg->Items[reg2].ImageIndex = IMG_NODE_REGCHANGE;
                break;
            case 2:
                TVReg->Items[reg1].ImageIndex = IMG_NODE_REGSTORE;
                break;
            case 3:
                TVReg->Items[reg2].ImageIndex = IMG_NODE_REGTEST;
                break;
            }
*/
        }
        else {
            if (opcode == 38) // movei
                pc += 4;
        }

        // Jump Buffered
        if (opcode != 52 && opcode != 53 && jumpbuffered) {
            pc = JMPPC;
            jumpbuffered = false;
            JumpBuffLabelUpdate();
            CheckGPUPC();
            //UpdateGPUPCView();
        }
    }
}


// Check if the GPU Program Counter is within valid bounds
void GPUDebugger::CheckGPUPC() {
    if ((pc < 0) || (pc > MemorySize)) {
        std::string str = "GPU PC outside allocated buffer !\nAddress = $" + IntToHex(pc, 8) + "\nResetting GPU !";
        QMessageBox::critical(nullptr, "Error", QString::fromStdString(str));
        reset();
    }
}


void GPUDebugger::RunGPU() {
    gpurun = true;
    WriteLong(G_CTRL, ReadLong(G_CTRL) | 1);
    while (gpurun) {
        if (pc == breakpointAddress) {
            StopGPU();
        }
        else if (pc >= (loadAddress + programSize)) {
            std::string str = "Reached program end !\nAddress = $" + IntToHex(pc, 8);
            // If MessageDlg is not defined, you can use QMessageBox instead:
            QMessageBox::warning(nullptr, "Warning", QString::fromStdString(str));
            StopGPU();
        }
        else {
            int w = ReadWord(pc, true);
            if (w != -1)
                step((uint16_t)w, true);
        }
        //ApplicationProcessMessages();
    }
    StopGPU();
}


// Run the program until a breakpoint is hit or the end of the program is reached
void GPUDebugger::run() {
    // Implementation for running the program
    if (isReadyToRun) {
        if (!gpurun) {
            /*
            GDBUG.RunGPUButton.Caption = "Stop GPU (F9)";
            GDBUG.StepButton.Enabled = false;
            GDBUG.SkipButton.Enabled = false;
            GDBUG.ResetGPUButton.Enabled = false;
            GDBUG.Button1.Enabled = false;
            */
            RunGPU();
        }
        else {
            StopGPU();
        }
        //gpurun = true;
    }
}


// Step through one instruction
void GPUDebugger::skip() {
    // Implementation for skipping one instruction
    if (isReadyToSkip) {
        pc += 4; // Increment PC by 4 without execution
    }
}


// check if the program can be run
bool GPUDebugger::canRun() const {
    return isReadyToRun;
}


// Check if the debugger can step through instructions
bool GPUDebugger::canStep() const {
    return isReadyToStep;
}


// Check if the debugger can skip the current instruction
bool GPUDebugger::canSkip() const {
    return isReadyToSkip; // Return whether skipping is allowed
}


// Get the register bank as a list of strings for the specified bank (0 or 1)
QStringList GPUDebugger::getRegBank(int bank) const {
    QStringList list;
    for (int i = 0; i < 32; ++i) {
        int value = 0;
        if (bank == 0 && regBank[0].size() == 32) value = regBank[0][i];
        else if (bank == 1 && regBank[1].size() == 32) value = regBank[1][i];
        // Register label in lowercase, value in uppercase
        list << QString("r%1: $%2")
            .arg(i, 0, 10)
            .arg(QString::number(value, 16).toUpper().rightJustified(8, '0'));
    }
    return list;
}


// Get the code view as a list of strings (disassembled instructions)
QStringList GPUDebugger::getCodeView() const {
    return codeViewLines;
}


// Get the current flags as a formatted string
QString GPUDebugger::getFlags() const {
    return QString("Flags: Z:%1 N:%2 C:%3").arg(flagZ).arg(flagN).arg(flagC);
}


// Get the current program counter (PC) as a formatted string
QString GPUDebugger::getPCString() const {
    return QString("$%1").arg(pc, 8, 16, QChar('0')).toUpper();
}


// Get the current value program counter (PC)
int GPUDebugger::getPCValue() const {
    return pc;
}


// Get the jump address (JMPPC) as a formatted string
QString GPUDebugger::getJump() const {
    return QString("$%1").arg(JMPPC, 8, 16, QChar('0')).toUpper();
}


// Get the remaining data register value (G_REMAIN) as a formatted string
QString GPUDebugger::getRemain() const {
    return "$00000000";
}


// Get the high data register value (G_HIDATA) as a formatted string
QString GPUDebugger::getHiData() const {
    return "$00000000";
}


// Get the current breakpoint address in a formatted string
QString GPUDebugger::getBP() const {
    return QString("$%1").arg(breakpointAddress, 8, 16, QChar('0')).toUpper();
}


// Get the current progress of the debugger (e.g., for disassembly or execution)
int GPUDebugger::getProgress() const {
    return progress;
}


// Get the size of the program loaded into memory
int GPUDebugger::getProgramSize() const {
    return programSize;
}


// Set the PC (Program Counter) to the specified value
void GPUDebugger::setStringPC(const QString& pcValue) {
    bool ok = false;
    QString modifiablePCValue = pcValue; // Create a modifiable copy
    pc = modifiablePCValue.remove('$').toInt(&ok, 16);
    if (!ok) pc = 0;
}


// Edit a register value in the specified bank
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
    if (!ok || (regIndex < 0) || (regIndex >= 32)) {
        qDebug() << "Invalid register value or index:" << value;
        return;
    }
    if (bank == 0 && regBank[0].size() == 32) {
        regBank[0][regIndex] = regValue;
    }
    else if (bank == 1 && regBank[1].size() == 32) {
        regBank[1][regIndex] = regValue;
    }
    else {
        qDebug() << "Invalid bank specified.";
    }
}


// Set the Mode (true for GPU, false for DSP)
void GPUDebugger::setGPUMode(bool isGPUMode) {
    GPUMode = isGPUMode;
/*
    // Logic to switch between GPU and DSP modes
    if (isGPUMode) {
        // Set GPU-specific state
        // Example: Update internal flags or configurations
    } else {
        // Set DSP-specific state
        // Example: Update internal flags or configurations
    }
*/
}


// Set a breakpoint at a specified address
void GPUDebugger::setBreakpoint(const QString& address) {
    bool ok = false;
    QString modifiableAddress = address;
    int bp = modifiableAddress.remove('$').toInt(&ok, 16);
    if (ok) {
        if (breakpoints.contains(bp)) {
            // Remove breakpoint if already set at this address
            breakpoints.remove(bp);
            if (breakpointAddress == bp)
                breakpointAddress = 0;
        } else {
            // Set new breakpoint
            breakpoints.insert(bp);
            breakpointAddress = bp;
        }
    } else {
        qDebug() << "Invalid address format for breakpoint:" << address;
    }
}


// Check if a breakpoint is set at a given address
bool GPUDebugger::hasBreakpoint(int address) const {
    return breakpoints.contains(address);
}


// Disassemble the program starting from the given load address
QStringList GPUDebugger::disassemble(int loadAddress, int programSize) const {
    QStringList result;
    const uint8_t* walk = MemoryBuffer.data() + loadAddress;
    int size = programSize;
    int adrs = loadAddress;

    GPUDebugger* self = const_cast<GPUDebugger*>(this);

    int total = (programSize > 0) ? programSize : 1;
    int processed = 0;

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
        // Format address as $XXXXXXXX in uppercase
        QString addrStr = QString("$%1").arg(adrs, 8, 16, QChar('0')).toUpper();
        result << QString("%1: %2").arg(addrStr).arg(instr);
        size -= ecart;
        adrs += ecart;
        processed += ecart;

        // Update progress (0-99%)
        int percent = (processed * 100) / total;
        self->progress = percent;
        emit self->disassemblyProgress(percent); // Notify UI
    }
    // Ensure progress is 100% at the end
    self->progress = 100;
    emit self->disassemblyProgress(100); // Ensure 100% at end
    return result;
}


// Update the Jump Buffer label in the debugger UI
void GPUDebugger::JumpBuffLabelUpdate() {
    /*
    if (jumpbuffered) {
        GDBUG.JUMPLabel.Caption = "Jump: $" + IntToHex(JMPPC, 8);
        GDBUG.JUMPLabel.FontStyle = 1;
    }
    else {
        GDBUG.JUMPLabel.Caption = "Jump: $00000000";
        GDBUG.JUMPLabel.FontStyle = 0;
    }
    */
}


// Update the N and Z flags based on the value of i
void GPUDebugger::Update_ZN_Flag(int i) {
    flagN = (i < 0) ? 1 : 0;
    flagZ = (i == 0) ? 1 : 0;
}


//  Update the C flag for addition
void GPUDebugger::Update_C_Flag_Add(int a, int b) {
    unsigned int uint1 = ~a;
    unsigned int uint2 = b;
    flagC = (uint2 > uint1) ? 1 : 0;
}


// Update the C flag for subtraction
void GPUDebugger::Update_C_Flag_Sub(int a, int b) {
    unsigned int uint1 = a;
    unsigned int uint2 = b;
    flagC = (uint1 > uint2) ? 1 : 0;
}


// Write a byte value to the specified address
void GPUDebugger::WriteLong(int adrs, int data) {
    int memadrs = adrs;
    adrs = adrs & 0xFFFFFFFC;
    if (memadrs != adrs) {
        if (!memoryWarningEnabled) {
            std::string str = "WriteLong not on a Long aligned address !\nAddress = $" + IntToHex(memadrs, 8) + "\nShould be = $" + IntToHex(adrs, 8);
            QMessageBox::critical(nullptr, "Error", QString::fromStdString(str));
        }
    }
    memadrs = adrs;
    if ((memadrs >= 0) && ((memadrs + 4) <= MemorySize)) {
        uint8_t* walk = MemoryBuffer.data() + memadrs;
        walk[0] = (data >> 24) & 0xFF;
        walk[1] = (data >> 16) & 0xFF;
        walk[2] = (data >> 8) & 0xFF;
        walk[3] = data & 0xFF;
    }
    else {
        if (!memoryWarningEnabled) {
            std::string str = "WriteLong outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
            QMessageBox::critical(nullptr, "Error", QString::fromStdString(str));
        }
    }
    MemWriteCheck();
}


// Read a long value from the specified address
int GPUDebugger::ReadLong(int adrs) {
    int memadrs = adrs;
    adrs = adrs & 0xFFFFFFFC;
    if (memadrs != adrs) {
        if (!memoryWarningEnabled) {
            std::string str = "ReadLong not on a Long aligned address !\nAddress = $" + IntToHex(memadrs, 8) + "\nShould be = $" + IntToHex(adrs, 8);
            QMessageBox::critical(nullptr, "Error", QString::fromStdString(str));
        }
    }
    memadrs = adrs;
    if ((memadrs >= 0) && ((memadrs + 4) <= MemorySize)) {
        uint8_t* walk = MemoryBuffer.data() + memadrs;
        int value = (walk[0] << 24) | (walk[1] << 16) | (walk[2] << 8) | walk[3];
        return value;
    }
    else {
        if (!memoryWarningEnabled) {
            std::string str = "ReadLong outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
            QMessageBox::critical(nullptr, "Error", QString::fromStdString(str));
        }
        return -1;
    }
    return 0;
}


// get the jump flag as a string based on the flag value
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


// Convert an integer to a hexadecimal string with specified width
std::string IntToHex(int value, int width) {
    std::ostringstream stream;
    stream << std::uppercase << std::setfill('0') << std::setw(width) << std::hex << value;
    return stream.str();
}


// Check if the jump condition matches the current flags
bool GPUDebugger::JumpConditionMatch(uint8_t condition) const {
    return
        (condition == 0) ||
        ((condition == 1) && (flagZ == 0)) ||
        ((condition == 2) && (flagZ == 1)) ||
        ((condition == 4) && (flagC == 0)) ||
        ((condition == 5) && (flagC == 0) && (flagZ == 0)) ||
        ((condition == 6) && (flagC == 0) && (flagZ == 1)) ||
        ((condition == 8) && (flagC == 1)) ||
        ((condition == 9) && (flagC == 1) && (flagZ == 0)) ||
        ((condition == 0xA) && (flagC == 1) && (flagZ == 1)) ||
        ((condition == 0x14) && (flagN == 0)) ||
        ((condition == 0x15) && (flagN == 0) && (flagZ == 0)) ||
        ((condition == 0x16) && (flagN == 0) && (flagZ == 1)) ||
        ((condition == 0x18) && (flagN == 1)) ||
        ((condition == 0x19) && (flagN == 1) && (flagZ == 0)) ||
        ((condition == 0x1A) && (flagN == 1) && (flagZ == 1));
}


/*
// Set whether memory warnings are enabled or not
void GPUDebugger::setMemoryWarningEnabled(bool enabled) {
    memoryWarningEnabled = enabled;
}
*/


// Read a byte from the specified address
int GPUDebugger::ReadByte(int adrs) {
    int memadrs = adrs;
    if (CheckInternalRam(memadrs))
        QMessageBox::warning(nullptr, "Warning", "ReadByte not allowed in internal RAM!");
    if ((memadrs >= 0) && memadrs < MemorySize) {
        uint8_t* walk = MemoryBuffer.data() + memadrs;
        return walk[0];
    }
    else if (!memoryWarningEnabled) {
        std::string str = "ReadByte outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        QMessageBox::critical(nullptr, "Error", QString::fromStdString(str));
        return -1;
    }
    return 0;
}


// Read a word from the specified address
int GPUDebugger::ReadWord(int adrs, bool nochk) {
    int memadrs = adrs;
    adrs = adrs & 0xFFFFFFFE;
    if ((memadrs != adrs) && !memoryWarningEnabled) {
        std::string str = "ReadWord not on a Word aligned address !\nAddress = $" + IntToHex(memadrs, 8) + "\nShould be = $" + IntToHex(adrs, 8);
        QMessageBox::warning(nullptr, "Warning", QString::fromStdString(str));
    }
    if (CheckInternalRam(memadrs) && !nochk)
        QMessageBox::warning(nullptr, "Warning", "ReadWord not allowed in internal ram !");
    memadrs = adrs;
    if ((memadrs >= 0) && (memadrs + 2) <= MemorySize) {
        uint8_t* walk = MemoryBuffer.data() + memadrs;
        int value = (walk[0] << 8) | walk[1];
        return value;
    }
    else if (!memoryWarningEnabled) {
        std::string str = "ReadWord outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        QMessageBox::warning(nullptr, "Warning", QString::fromStdString(str));
        return -1;
    }
    return 0;
}


// Write a word to the specified address
void GPUDebugger::WriteWord(int adrs, int data) {
    int memadrs = adrs;
    adrs = adrs & 0xFFFFFFFE;
    if ((memadrs != adrs) && !memoryWarningEnabled) {
        std::string str = "WriteWord not on a Word aligned address !\nAddress = $" + IntToHex(memadrs, 8) + "\nShould be = $" + IntToHex(adrs, 8);
        QMessageBox::critical(nullptr, "Error", QString::fromStdString(str));
    }
    if (CheckInternalRam(memadrs))
        QMessageBox::warning(nullptr, "Warning", "WriteWord not allowed in internal ram !");
    memadrs = adrs;
    if (memadrs >= 0 && (memadrs + 2) <= MemorySize) {
        uint8_t* walk = MemoryBuffer.data() + memadrs;
        walk[0] = (data >> 8) & 0xFF;
        walk[1] = data & 0xFF;
    }
    else if (!memoryWarningEnabled) {
        std::string str = "WriteWord outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        QMessageBox::critical(nullptr, "Error", QString::fromStdString(str));
    }
    MemWriteCheck();
}


// Write a byte to the specified address
void GPUDebugger::WriteByte(int adrs, int data) {
    int memadrs = adrs;
    if (CheckInternalRam(memadrs))
        QMessageBox::warning(nullptr, "Warning", "WriteByte not allowed in internal ram !");
    if ((memadrs >= 0) && (memadrs < MemorySize)) {
        uint8_t* walk = MemoryBuffer.data() + memadrs;
        walk[0] = data & 0xFF;
    }
    else if (!memoryWarningEnabled) {
        std::string str = "WriteByte outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        QMessageBox::critical(nullptr, "Error", QString::fromStdString(str));
    }
    MemWriteCheck();
}


// Check memory write conditions and update registers accordingly
void GPUDebugger::MemWriteCheck() {
    if (GPUMode) {
        if ((ReadLong(G_CTRL) & 1) == 0 && gpurun) {
            StopGPU();
            QMessageBox::warning(nullptr, "Stop", "GPU Self Stopped!");
        }
        CurRegBank = (ReadLong(G_FLAGS) >> 14) & 1;
/*
        GDBUG.G_HIDATALabel.Caption = "G_HIDATA: $" + IntToHex(ReadLong(G_HIDATA), 8);
        GDBUG.G_REMAINLabel.Caption = "G_REMAIN: $" + IntToHex(ReadLong(G_REMAIN), 8);
*/
    }
    else {
        if ((ReadLong(D_CTRL) & 1) == 0 && gpurun) {
            StopGPU();
            QMessageBox::warning(nullptr, "Stop", "DSP Self Stopped!");
        }
        CurRegBank = (ReadLong(D_FLAGS) >> 14) & 1;
    }
/*
    GDBUG.RegBank0Label.FontStyle = 0;
    GDBUG.RegBank1Label.FontStyle = 0;
    if (CurRegBank == 0)
        GDBUG.RegBank0Label.FontStyle = 1;
    else
        GDBUG.RegBank1Label.FontStyle = 1;
*/
}


// Stop the program execution
void GPUDebugger::StopGPU() {
    gpurun = false;
}



// Check if the address is in the GPU, or DSP, RAM range
bool GPUDebugger::CheckInternalRam(int memadrs) {
    int mas, mae;
    if (GPUMode) {
        mas = G_RAM;
        mae = G_RAM + 4 * 1024;
    }
    else {
        mas = D_RAM;
        mae = D_RAM + 8 * 1024;
    }
    return (memadrs >= mas) && (memadrs < mae);
}
