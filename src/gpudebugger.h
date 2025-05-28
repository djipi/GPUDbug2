#pragma once
#include <QString>
#include <QStringList> // Include QStringList to resolve the error
#include <vector>
#include <cstdint>

extern const int MemorySize;
extern std::vector<uint8_t> MemoryBuffer;

class GPUDebugger {
public:
    GPUDebugger();
    bool loadBin(const QString& filename, int address);
    void reset();
    void step();
    void run();
    void skip();
    // ... other methods as needed

    // Data for UI
    QStringList getRegBank(int bank) const;
    QStringList getCodeView() const;
    QString getFlags() const;
    QString getPC() const;
    QString getJump() const;
    QString getRemain() const;
    QString getHiData() const;
    QString getBP() const;
    int getProgress() const;

    // New methods to check debugger state
    bool canRun() const;
    bool canStep() const;
    bool canSkip() const;
    bool canReset() const;

    void setPC(const QString& pcValue);
    void setGPUMode(bool isGPUMode);
    void setBreakpoint(const QString& address);

    void editRegister(int bank, const QString& value); // Add declaration for editRegister

    QStringList disassemble(int loadAddress, int programSize) const;
    int getProgramSize() const; // Add this if needed

private:
    int progress;
    bool isReadyToRun;
    bool isReadyToStep;
    bool isReadyToSkip;
    bool isReadyToReset;
    int pc;
    int programSize; // <-- Add this line
    std::vector<int> regBank0;
    std::vector<int> regBank1;
    QStringList codeViewLines; // Add this line
};

// Declare GetJumpFlag function
std::string GetJumpFlag(uint8_t flag);