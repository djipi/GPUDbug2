#pragma once
#include <QString>
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
    bool canSkip() const; // Added declaration for canSkip
    bool canReset() const; // Add this declaration

    void setPC(const QString& pcValue);
    void editRegister(int bank, const QString& value);

    void setGPUMode(bool isGPUMode); // Added declaration for setGPUMode
    void setBreakpoint(const QString& address); // Added declaration for setBreakpoint

private:
    // Internal state and logic (from your previous C++ translation)
    int progress; // Add a member variable to store progress
    bool isReadyToRun; // New member variable to track if the debugger can run
    bool isReadyToStep; // Added member variable to track if stepping is allowed
    bool isReadyToSkip; // Added member variable to track if skipping is allowed
    bool isReadyToReset; // Add a member variable to track if reset is allowed
    int pc; // Program counter
    std::vector<int> regBank0; // Define regBank0 as a vector of integers
    std::vector<int> regBank1; // Define regBank1 for consistency with the code
};