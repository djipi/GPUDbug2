#pragma once
#include <QString>
#include <QStringList> // Include QStringList to resolve the error
#include <vector>
#include <cstdint>
#include <QSet> // Include QSet for breakpoints
#include <QObject> // Include QObject for signals and slots
#include <functional> // Include functional for std::function

extern const int MemorySize;
extern std::vector<uint8_t> MemoryBuffer;

class Debugger : public QObject { // Ensure QObject is a base class
    Q_OBJECT // Required for Qt's meta-object system

public:
    explicit Debugger(QObject* parent = nullptr);
    ~Debugger();
    bool loadBin(const QString& filename, int address);
    void reset();
    void step(uint16_t w, bool exec);
    void run();
    void skip();
    // ... other methods as needed

    // Data for UI
    QStringList getRegBank(int bank) const;
	int getRegBankRegisterValue(int bank, int reg) const;
    QStringList getCodeView() const;
    QString getFlags() const;
    QString getPCString() const;
    int getPCValue() const;
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

    void setStringPC(const QString& pcValue);
    void setGPUMode(bool isGPUMode);
    void setBreakpoint(const QString& address);
    bool hasBreakpoint(int address) const;

    void editRegister(int bank, const QString& value);

    QStringList disassemble(int loadAddress, int programSize) const;
    int getProgramSize() const;

    void setMemoryWarningEnabled(bool enabled) { memoryWarningEnabled = enabled; }

signals:
    void disassemblyProgress(int percent);

private:
    int progress;
    bool isReadyToRun;
    bool isReadyToStep;
    bool isReadyToSkip;
    bool isReadyToReset;
    int pc;
    int programSize;
    std::vector<int> regBank[2];
    QStringList codeViewLines;
    int breakpointAddress = 0;
    QSet<int> breakpoints; // Stores all breakpoints
    int loadAddress = 0; // Stores the last loading address
    int flagZ = 0;
    int flagN = 0;
    int flagC = 0;
    int CurRegBank = 0; // New member added
    bool memoryWarningEnabled = true; // New variable for UI control
    int JMPPC = 0;
	bool GPUMode = true; // GPU mode is default
    const int G_FLAGS = 0xF02100;
    const int G_CTRL = 0xF02114;
    const int G_HIDATA = 0xF02118;
    const int G_REMAIN = 0xF0211C;
    const int G_RAM = 0xF03000;
    const int D_FLAGS = 0xF1A100;
    const int D_CTRL = 0xF1A114;
    const int D_RAM = 0xF1B000;
    bool gpurun = false;
    bool jumpbuffered = false;

public:
    int ReadWord(int adrs, bool nochk);

private:
    void Update_C_Flag_Add(int a, int b);
    void Update_ZN_Flag(int i);
    void Update_C_Flag_Sub(int a, int b);
    int ReadLong(int adrs);
    void WriteLong(int adrs, int data);
    int ReadByte(int adrs);
    void WriteByte(int adrs, int data);
    void WriteWord(int adrs, int data);
    bool JumpConditionMatch(uint8_t condition) const;
    void JumpBuffLabelUpdate();
    void MemWriteCheck();
    void StopGPU();
    bool CheckInternalRam(int memadrs);
    void CheckGPUPC();
    void RunGPU();
    std::string GetJumpFlag(uint8_t flag) const;
    std::string IntToHex(int value, int width) const;
};
