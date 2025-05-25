// GPUDBUG v0.8 beta - by Orion_ [2007-2008]
// Translated from Pascal to C++
// License: http://creativecommons.org/licenses/by-nc-sa/3.0/

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <algorithm>

// Add this near the top, after the includes:
template <typename T>
const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

// --- GUI stubs and helpers (replace with your GUI framework) ---

#define mtError 0
#define mtWarning 1
#define mbOK 0

void MessageDlg(const std::string& title, const std::string& msg, int type, int buttons, int def) {
    std::cout << "[" << title << "] " << msg << std::endl;
}

bool InputQuery(const std::string& title, const std::string& prompt, std::string& value) {
    std::cout << title << " " << prompt << " [" << value << "]: ";
    std::getline(std::cin, value);
    return true;
}

void ApplicationTerminate() {
    exit(0);
}

void ApplicationProcessMessages() {
    // No-op for CLI
}

// --- Data structures ---

struct TreeViewItem {
    std::string Text;
    int ImageIndex = 0;
};

struct TreeView {
    std::vector<TreeViewItem> Items;
    int TopItem = 0;
    TreeViewItem* Selected = nullptr;
    void Refresh() {}
    void Clear() { Items.clear(); }
    int Count() const { return (int)Items.size(); }
};

struct Label { std::string Caption; int FontStyle = 0; };
struct ProgressBar { int Position = 0; };
struct Edit { std::string Text; };
struct CheckBox { bool Checked = false; };
struct RadioButton { bool Checked = false; };
struct Button { bool Enabled = true; std::string Caption; };
struct OpenDialog { std::string FileName; bool Execute() { return true; } };

// --- Constants ---

const int IMG_NODE_NOTHING = 0;
const int IMG_NODE_CURRENT = 1;
const int IMG_NODE_BREAKPOINT = 2;
const int IMG_NODE_REGCHANGE = 3;
const int IMG_NODE_REGSTORE = 4;
const int IMG_NODE_REGTEST = 5;
const int G_FLAGS  = 0xF02100;
const int G_CTRL   = 0xF02114;
const int G_HIDATA = 0xF02118;
const int G_REMAIN = 0xF0211C;
const int G_RAM    = 0xF03000;
const int D_FLAGS  = 0xF1A100;
const int D_CTRL   = 0xF1A114;
const int D_RAM    = 0xF1B000;

// --- Globals ---

uint8_t* MemoryBuffer = nullptr;
int MemorySize = 0;
int ProgramSize = 0;
int CodeViewCurPos = 0;
int LoadAddress = 0;
int CurRegBank = 0;
int RegBank[2][32] = {0};
int RegStatus = 0;
int JMPPC = 0, GPUPC = 0, GPUBP = 0, ZFlag = 0, NFlag = 0, CFlag = 0;
bool jumpbuffered = false, noPCrefresh = false, gpurun = false;

// --- Main form stub ---

struct TGDBUG {
    Button Button1, SkipButton, RunGPUButton, StepButton, Button4, ResetGPUButton;
    Label G_REMAINLabel, JUMPLabel, RegBank0Label, RegBank1Label, FlagStatusLabel, GPUBPLabel, G_HIDATALabel, Label3, Label4, Label5;
    TreeView CodeView, RegBank0, RegBank1;
    ProgressBar Loading0;
    Edit LoadAddressEdit, GPUPCEdit;
    CheckBox MemWarn;
    RadioButton GPUMode, DSPMode;
    OpenDialog OpenDialog;
} GDBUG;

// --- Utility functions ---

std::string IntToStr(int value) { return std::to_string(value); }
std::string IntToHex(int value, int width) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%0*X", width, value);
    return std::string(buf);
}
int StrToInt(const std::string& s) { return std::stoi(s, nullptr, 0); }

// --- Function declarations ---

void UpdateRegView();
void UpdateFlagView();
void UpdatePCView();
void UpdateGPUPCView();
void StopGPU();
void JumpBuffLabelUpdate();
void ResetGPU();
void CheckGPUPC();
bool LoadBin(const std::string& fl, int adrs);
int GPUReadLong(int adrs);
bool CheckInternalRam(int memadrs);
int GPUReadWord(int adrs, bool nochk);
int GPUReadByte(int adrs);
void MemWriteCheck();
void GPUWriteLong(int adrs, int data);
void GPUWriteWord(int adrs, int data);
void GPUWriteByte(int adrs, int data);
std::string GetJumpFlag(uint8_t flag);
void Update_ZN_Flag(int i);
void Update_C_Flag_Add(int a, int b);
void Update_C_Flag_Sub(int a, int b);
bool JumpConditionMatch(uint8_t condition);
void GPUStep(uint16_t w, bool exec);
void Disassemble();
void RunGPU();

// --- Function implementations ---

void UpdateRegView() {
    for (int i = 0; i < 32; ++i) {
        std::string str = (i < 10) ? " " : "";
        GDBUG.RegBank0.Items[i].Text = "r" + IntToStr(i) + ": " + str + "$" + IntToHex(RegBank[0][i], 8);
        GDBUG.RegBank1.Items[i].Text = "r" + IntToStr(i) + ": " + str + "$" + IntToHex(RegBank[1][i], 8);
    }
    GDBUG.RegBank0.Refresh();
    GDBUG.RegBank1.Refresh();
}

void UpdateFlagView() {
    GDBUG.FlagStatusLabel.Caption = "Flags: Z:" + IntToStr(ZFlag) + " N:" + IntToStr(NFlag) + " C:" + IntToStr(CFlag);
}

void UpdatePCView() {
    GDBUG.GPUPCEdit.Text = "$" + IntToHex(GPUPC, 8);
}

void UpdateGPUPCView() {
    int adrs = LoadAddress, size = ProgramSize, i = 0;
    uint8_t* walk = MemoryBuffer + LoadAddress;
    CodeViewCurPos = 0;
    for (i = 0; i < (int)GDBUG.CodeView.Items.size(); ++i)
        GDBUG.CodeView.Items[i].ImageIndex = IMG_NODE_NOTHING;
    while (GPUPC != adrs && size > 1) {
        uint8_t opcode = (*walk) >> 2;
        walk += 2; adrs += 2; size -= 2;
        if (opcode == 38) { walk += 4; adrs += 4; size -= 4; }
        ++CodeViewCurPos;
    }
    if (CodeViewCurPos < (int)GDBUG.CodeView.Items.size()) {
        GDBUG.CodeView.Items[CodeViewCurPos].ImageIndex = IMG_NODE_CURRENT;
        GDBUG.CodeView.TopItem = CodeViewCurPos;
    }
    GDBUG.CodeView.Refresh();
    noPCrefresh = true;
}

void StopGPU() {
    gpurun = false;
    UpdateGPUPCView();
    UpdatePCView();
    UpdateRegView();
    UpdateFlagView();
    GDBUG.RunGPUButton.Caption = "Run GPU (F9)";
    GDBUG.StepButton.Enabled = true;
    GDBUG.SkipButton.Enabled = true;
    GDBUG.ResetGPUButton.Enabled = true;
    GDBUG.Button1.Enabled = true;
}

void JumpBuffLabelUpdate() {
    if (jumpbuffered) {
        GDBUG.JUMPLabel.Caption = "Jump: $" + IntToHex(JMPPC, 8);
        GDBUG.JUMPLabel.FontStyle = 1;
    } else {
        GDBUG.JUMPLabel.Caption = "Jump: $00000000";
        GDBUG.JUMPLabel.FontStyle = 0;
    }
}

void ResetGPU() {
    StopGPU();
    CurRegBank = 0;
    ZFlag = 0; NFlag = 0; CFlag = 0;
    GPUPC = LoadAddress;
    GDBUG.GPUPCEdit.Text = "$" + IntToHex(LoadAddress, 8);
    CodeViewCurPos = 0;
    noPCrefresh = true;
    jumpbuffered = false;
    JumpBuffLabelUpdate();
    for (int i = 0; i < (int)GDBUG.CodeView.Items.size(); ++i)
        GDBUG.CodeView.Items[i].ImageIndex = IMG_NODE_NOTHING;
    if (!GDBUG.CodeView.Items.empty())
        GDBUG.CodeView.Items[CodeViewCurPos].ImageIndex = IMG_NODE_CURRENT;
    GDBUG.CodeView.Refresh();
    for (int i = 0; i < 32; ++i) {
        RegBank[0][i] = 0;
        RegBank[1][i] = 0;
        GDBUG.RegBank0.Items[i].ImageIndex = IMG_NODE_NOTHING;
        GDBUG.RegBank1.Items[i].ImageIndex = IMG_NODE_NOTHING;
    }
    UpdatePCView();
    UpdateRegView();
    UpdateFlagView();
    UpdateGPUPCView();
}

void CheckGPUPC() {
    if (GPUPC < 0 || GPUPC > MemorySize) {
        std::string str = "GPU PC outside allocated buffer !\nAddress = $" + IntToHex(GPUPC, 8) + "\nResetting GPU !";
        MessageDlg("Error", str, mtError, mbOK, 0);
        ResetGPU();
    }
}

bool LoadBin(const std::string& fl, int adrs) {
    LoadAddress = adrs;
    FILE* f = fopen(fl.c_str(), "rb");
    if (!f) {
        MessageDlg("Error", "Error while loading file.", mtError, mbOK, 0);
        return false;
    }
    fseek(f, 0, SEEK_END);
    int fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize > (MemorySize - LoadAddress)) {
        MessageDlg("Error", "File too large !", mtError, mbOK, 0);
        fclose(f);
        return false;
    }
    ProgramSize = fsize;
    uint8_t* walk = MemoryBuffer + adrs;
    if (ProgramSize > 12) {
        uint8_t* walk2 = walk;
        fread(walk, 1, 12, f);
        int value = (walk2[0] << 24) | (walk2[1] << 16) | (walk2[2] << 8) | walk2[3];
        walk += 12;
        if (value == 0x42533934) {
            value = (walk2[4] << 24) | (walk2[5] << 16) | (walk2[6] << 8) | walk2[7];
            LoadAddress = value;
            walk = MemoryBuffer + LoadAddress;
        }
        ProgramSize -= 12;
    }
    fread(walk, 1, ProgramSize, f);
    fclose(f);
    return true;
}

int GPUReadLong(int adrs) {
    int memadrs = adrs;
    adrs = adrs & 0xFFFFFFFC;
    if (memadrs != adrs && !GDBUG.MemWarn.Checked) {
        std::string str = "ReadLong not on a Long aligned address !\nAddress = $" + IntToHex(memadrs, 8) + "\nShould be = $" + IntToHex(adrs, 8);
        MessageDlg("Warning", str, mtWarning, mbOK, 0);
    }
    memadrs = adrs;
    if (memadrs >= 0 && (memadrs + 4) <= MemorySize) {
        uint8_t* walk = MemoryBuffer + memadrs;
        int value = (walk[0] << 24) | (walk[1] << 16) | (walk[2] << 8) | walk[3];
        return value;
    } else if (!GDBUG.MemWarn.Checked) {
        std::string str = "ReadLong outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        MessageDlg("Error", str, mtError, mbOK, 0);
        return -1;
    }
    return 0;
}

bool CheckInternalRam(int memadrs) {
    int mas, mae;
    if (GDBUG.GPUMode.Checked) {
        mas = G_RAM;
        mae = G_RAM + 4 * 1024;
    } else {
        mas = D_RAM;
        mae = D_RAM + 8 * 1024;
    }
    return (memadrs >= mas) && (memadrs < mae);
}

int GPUReadWord(int adrs, bool nochk) {
    int memadrs = adrs;
    adrs = adrs & 0xFFFFFFFE;
    if (memadrs != adrs && !GDBUG.MemWarn.Checked) {
        std::string str = "ReadWord not on a Word aligned address !\nAddress = $" + IntToHex(memadrs, 8) + "\nShould be = $" + IntToHex(adrs, 8);
        MessageDlg("Warning", str, mtWarning, mbOK, 0);
    }
    if (CheckInternalRam(memadrs) && !nochk)
        MessageDlg("Warning", "ReadWord not allowed in internal ram !", mtWarning, mbOK, 0);
    memadrs = adrs;
    if (memadrs >= 0 && (memadrs + 2) <= MemorySize) {
        uint8_t* walk = MemoryBuffer + memadrs;
        int value = (walk[0] << 8) | walk[1];
        return value;
    } else if (!GDBUG.MemWarn.Checked) {
        std::string str = "ReadWord outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        MessageDlg("Error", str, mtError, mbOK, 0);
        return -1;
    }
    return 0;
}

int GPUReadByte(int adrs) {
    int memadrs = adrs;
    if (CheckInternalRam(memadrs))
        MessageDlg("Warning", "ReadByte not allowed in internal ram !", mtWarning, mbOK, 0);
    if (memadrs >= 0 && memadrs < MemorySize) {
        uint8_t* walk = MemoryBuffer + memadrs;
        return walk[0];
    } else if (!GDBUG.MemWarn.Checked) {
        std::string str = "ReadByte outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        MessageDlg("Error", str, mtError, mbOK, 0);
        return -1;
    }
    return 0;
}

void MemWriteCheck() {
    if (GDBUG.GPUMode.Checked) {
        if ((GPUReadLong(G_CTRL) & 1) == 0 && gpurun) {
            StopGPU();
            MessageDlg("Stop", "GPU Self Stopped !", mtWarning, mbOK, 0);
        }
        CurRegBank = (GPUReadLong(G_FLAGS) >> 14) & 1;
        GDBUG.G_HIDATALabel.Caption = "G_HIDATA: $" + IntToHex(GPUReadLong(G_HIDATA), 8);
        GDBUG.G_REMAINLabel.Caption = "G_REMAIN: $" + IntToHex(GPUReadLong(G_REMAIN), 8);
    } else {
        if ((GPUReadLong(D_CTRL) & 1) == 0 && gpurun) {
            StopGPU();
            MessageDlg("Stop", "DSP Self Stopped !", mtWarning, mbOK, 0);
        }
        CurRegBank = (GPUReadLong(D_FLAGS) >> 14) & 1;
    }
    GDBUG.RegBank0Label.FontStyle = 0;
    GDBUG.RegBank1Label.FontStyle = 0;
    if (CurRegBank == 0)
        GDBUG.RegBank0Label.FontStyle = 1;
    else
        GDBUG.RegBank1Label.FontStyle = 1;
}

void GPUWriteLong(int adrs, int data) {
    int memadrs = adrs;
    adrs = adrs & 0xFFFFFFFC;
    if (memadrs != adrs && !GDBUG.MemWarn.Checked) {
        std::string str = "WriteLong not on a Long aligned address !\nAddress = $" + IntToHex(memadrs, 8) + "\nShould be = $" + IntToHex(adrs, 8);
        MessageDlg("Warning", str, mtWarning, mbOK, 0);
    }
    memadrs = adrs;
    if (memadrs >= 0 && (memadrs + 4) <= MemorySize) {
        uint8_t* walk = MemoryBuffer + memadrs;
        walk[0] = (data >> 24) & 0xFF;
        walk[1] = (data >> 16) & 0xFF;
        walk[2] = (data >> 8) & 0xFF;
        walk[3] = data & 0xFF;
    } else if (!GDBUG.MemWarn.Checked) {
        std::string str = "WriteLong outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        MessageDlg("Error", str, mtError, mbOK, 0);
    }
    MemWriteCheck();
}

void GPUWriteWord(int adrs, int data) {
    int memadrs = adrs;
    adrs = adrs & 0xFFFFFFFE;
    if (memadrs != adrs && !GDBUG.MemWarn.Checked) {
        std::string str = "WriteWord not on a Word aligned address !\nAddress = $" + IntToHex(memadrs, 8) + "\nShould be = $" + IntToHex(adrs, 8);
        MessageDlg("Warning", str, mtWarning, mbOK, 0);
    }
    if (CheckInternalRam(memadrs))
        MessageDlg("Warning", "WriteWord not allowed in internal ram !", mtWarning, mbOK, 0);
    memadrs = adrs;
    if (memadrs >= 0 && (memadrs + 2) <= MemorySize) {
        uint8_t* walk = MemoryBuffer + memadrs;
        walk[0] = (data >> 8) & 0xFF;
        walk[1] = data & 0xFF;
    } else if (!GDBUG.MemWarn.Checked) {
        std::string str = "WriteWord outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        MessageDlg("Error", str, mtError, mbOK, 0);
    }
    MemWriteCheck();
}

void GPUWriteByte(int adrs, int data) {
    int memadrs = adrs;
    if (CheckInternalRam(memadrs))
        MessageDlg("Warning", "WriteByte not allowed in internal ram !", mtWarning, mbOK, 0);
    if (memadrs >= 0 && memadrs < MemorySize) {
        uint8_t* walk = MemoryBuffer + memadrs;
        walk[0] = data & 0xFF;
    } else if (!GDBUG.MemWarn.Checked) {
        std::string str = "WriteByte outside allocated buffer !\nAddress = $" + IntToHex(adrs, 8);
        MessageDlg("Error", str, mtError, mbOK, 0);
    }
    MemWriteCheck();
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

void Update_ZN_Flag(int i) {
    NFlag = (i < 0) ? 1 : 0;
    ZFlag = (i == 0) ? 1 : 0;
}

void Update_C_Flag_Add(int a, int b) {
    unsigned int uint1 = ~a;
    unsigned int uint2 = b;
    CFlag = (uint2 > uint1) ? 1 : 0;
}

void Update_C_Flag_Sub(int a, int b) {
    unsigned int uint1 = a;
    unsigned int uint2 = b;
    CFlag = (uint1 > uint2) ? 1 : 0;
}

bool JumpConditionMatch(uint8_t condition) {
    return
        (condition == 0) ||
        ((condition == 1) && (ZFlag == 0)) ||
        ((condition == 2) && (ZFlag == 1)) ||
        ((condition == 4) && (CFlag == 0)) ||
        ((condition == 5) && (CFlag == 0) && (ZFlag == 0)) ||
        ((condition == 6) && (CFlag == 0) && (ZFlag == 1)) ||
        ((condition == 8) && (CFlag == 1)) ||
        ((condition == 9) && (CFlag == 1) && (ZFlag == 0)) ||
        ((condition == 0xA) && (CFlag == 1) && (ZFlag == 1)) ||
        ((condition == 0x14) && (NFlag == 0)) ||
        ((condition == 0x15) && (NFlag == 0) && (ZFlag == 0)) ||
        ((condition == 0x16) && (NFlag == 0) && (ZFlag == 1)) ||
        ((condition == 0x18) && (NFlag == 1)) ||
        ((condition == 0x19) && (NFlag == 1) && (ZFlag == 0)) ||
        ((condition == 0x1A) && (NFlag == 1) && (ZFlag == 1));
}

// --- The rest of the code (GPUStep, Disassemble, RunGPU, and event handlers) ---
// Due to space, please request the next part for the remaining implementation.
// --- Remaining implementation: GPUStep, Disassemble, RunGPU, and event handlers ---

void GPUStep(uint16_t w, bool exec) {
    uint8_t opcode = w >> 10;
    uint8_t reg1 = (w >> 5) & 31;
    uint8_t reg2 = w & 31;
    int RegTrace = 1;
    TreeView* TVReg = nullptr;

    if (opcode == 36) // moveta
        TVReg = (CurRegBank == 0) ? &GDBUG.RegBank1 : &GDBUG.RegBank0;
    else
        TVReg = (CurRegBank == 0) ? &GDBUG.RegBank0 : &GDBUG.RegBank1;

    // Clear Reg Trace
    for (int i = 0; i < 32; ++i)
        TVReg->Items[i].ImageIndex = IMG_NODE_NOTHING;

    GPUPC += 2;

    if (exec) {
        switch (opcode) {
        case 22: // abs
            NFlag = 0;
            CFlag = (RegBank[CurRegBank][reg2] < 0) ? 1 : 0;
            RegBank[CurRegBank][reg2] = std::abs(RegBank[CurRegBank][reg2]);
            ZFlag = (RegBank[CurRegBank][reg2] == 0) ? 1 : 0;
            break;
        case 0: // add
            Update_C_Flag_Add(RegBank[CurRegBank][reg1], RegBank[CurRegBank][reg2]);
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg1] + RegBank[CurRegBank][reg2];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 1: // addc
            Update_C_Flag_Add(RegBank[CurRegBank][reg1] + CFlag, RegBank[CurRegBank][reg2]);
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg1] + CFlag + RegBank[CurRegBank][reg2];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 2: // addq
            if (reg1 == 0) reg1 = 32;
            Update_C_Flag_Add(reg1, RegBank[CurRegBank][reg2]);
            RegBank[CurRegBank][reg2] = reg1 + RegBank[CurRegBank][reg2];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 3: // addqt
            if (reg1 == 0) reg1 = 32;
            RegBank[CurRegBank][reg2] = reg1 + RegBank[CurRegBank][reg2];
            break;
        case 9: // and
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg1] & RegBank[CurRegBank][reg2];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 15: // bclr
            RegBank[CurRegBank][reg2] &= ~(1 << reg1);
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 14: // bset
            RegBank[CurRegBank][reg2] |= (1 << reg1);
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 13: // btst
            RegTrace = 3;
            ZFlag = ((RegBank[CurRegBank][reg2] & (1 << reg1)) == 0) ? 1 : 0;
            break;
        case 30: // cmp
            RegTrace = 3;
            Update_C_Flag_Sub(RegBank[CurRegBank][reg1], RegBank[CurRegBank][reg2]);
            Update_ZN_Flag(RegBank[CurRegBank][reg2] - RegBank[CurRegBank][reg1]);
            break;
        case 31: // cmpq
            RegTrace = 3;
            Update_C_Flag_Sub(reg1, RegBank[CurRegBank][reg2]);
            Update_ZN_Flag(RegBank[CurRegBank][reg2] - reg1);
            break;
        case 21: { // div
            unsigned u32_1 = RegBank[CurRegBank][reg1];
            unsigned u32_2 = RegBank[CurRegBank][reg2];
            unsigned u32_3 = (u32_1 != 0) ? (u32_2 / u32_1) : 0;
            RegBank[CurRegBank][reg2] = u32_3;
            int temp = (u32_1 != 0) ? (u32_2 % u32_1) : 0;
            if ((u32_3 & 1) == 0)
                GPUWriteLong(G_REMAIN, temp - u32_1);
            else
                GPUWriteLong(G_REMAIN, temp);
            break;
        }
        case 17: { // imult
            int temp = RegBank[CurRegBank][reg1] & 0xFFFF;
            if (temp > 32767) temp -= 65536;
            RegBank[CurRegBank][reg2] &= 0xFFFF;
            if (RegBank[CurRegBank][reg2] > 32767) RegBank[CurRegBank][reg2] -= 65536;
            RegBank[CurRegBank][reg2] = temp * RegBank[CurRegBank][reg2];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        }
        case 53: // jr
            RegTrace = 0;
            if (JumpConditionMatch(reg2)) {
                if (reg1 > 15)
                    JMPPC = GPUPC - ((32 - reg1) * 2);
                else
                    JMPPC = GPUPC + (reg1 * 2);
                jumpbuffered = true;
                JumpBuffLabelUpdate();
            }
            break;
        case 52: // jump
            RegTrace = 0;
            if (JumpConditionMatch(reg2)) {
                JMPPC = RegBank[CurRegBank][reg1];
                jumpbuffered = true;
                JumpBuffLabelUpdate();
            }
            break;
        case 41: // load
            RegBank[CurRegBank][reg2] = GPUReadLong(RegBank[CurRegBank][reg1]);
            break;
        case 43: // load r14+n
            RegBank[CurRegBank][reg2] = GPUReadLong(RegBank[CurRegBank][14] + reg1 * 4);
            break;
        case 44: // load r15+n
            RegBank[CurRegBank][reg2] = GPUReadLong(RegBank[CurRegBank][15] + reg1 * 4);
            break;
        case 58: // load r14+rn
            RegBank[CurRegBank][reg2] = GPUReadLong(RegBank[CurRegBank][14] + RegBank[CurRegBank][reg1]);
            break;
        case 59: // load r15+rn
            RegBank[CurRegBank][reg2] = GPUReadLong(RegBank[CurRegBank][15] + RegBank[CurRegBank][reg1]);
            break;
        case 39: // loadb
            RegBank[CurRegBank][reg2] = GPUReadByte(RegBank[CurRegBank][reg1]);
            break;
        case 40: // loadw
            RegBank[CurRegBank][reg2] = GPUReadWord(RegBank[CurRegBank][reg1], false);
            break;
        case 42: // loadp
            GPUWriteLong(G_HIDATA, GPUReadLong(RegBank[CurRegBank][reg1]));
            RegBank[CurRegBank][reg2] = GPUReadLong(RegBank[CurRegBank][reg1] + 4);
            break;
        case 34: // move
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg1];
            break;
        case 51: // move pc
            RegBank[CurRegBank][reg2] = GPUPC - 2;
            break;
        case 37: // movefa
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank ^ 1][reg1];
            break;
        case 38: { // movei
            RegBank[CurRegBank][reg2] = GPUReadWord(GPUPC, true);
            RegBank[CurRegBank][reg2] |= (GPUReadWord(GPUPC + 2, true) << 16);
            GPUPC += 4;
            break;
        }
        case 35: // moveq
            RegBank[CurRegBank][reg2] = reg1;
            break;
        case 36: // moveta
            RegBank[CurRegBank ^ 1][reg2] = RegBank[CurRegBank][reg1];
            break;
        case 16: { // mult
            uint16_t u16_1 = RegBank[CurRegBank][reg1];
            uint16_t u16_2 = RegBank[CurRegBank][reg2];
            RegBank[CurRegBank][reg2] = u16_1 * u16_2;
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        }
        case 8: // neg
            RegBank[CurRegBank][reg2] = -RegBank[CurRegBank][reg2];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 12: // not
            RegBank[CurRegBank][reg2] = ~RegBank[CurRegBank][reg2];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 10: // or
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg1] | RegBank[CurRegBank][reg2];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 28: // ror
            CFlag = (RegBank[CurRegBank][reg2] >> 31) & 1;
            reg1 = RegBank[CurRegBank][reg1] & 31;
            RegBank[CurRegBank][reg2] = (RegBank[CurRegBank][reg2] >> reg1) | (RegBank[CurRegBank][reg2] << (32 - reg1));
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 29: // rorq
            CFlag = (RegBank[CurRegBank][reg2] >> 31) & 1;
            RegBank[CurRegBank][reg2] = (RegBank[CurRegBank][reg2] >> reg1) | (RegBank[CurRegBank][reg2] << (32 - reg1));
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 32: // sat8
            RegBank[CurRegBank][reg2] = clamp(RegBank[CurRegBank][reg2], 0, 255);
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 33: // sat16
            RegBank[CurRegBank][reg2] = clamp(RegBank[CurRegBank][reg2], 0, 65535);
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 62: // sat24
            RegBank[CurRegBank][reg2] = clamp(RegBank[CurRegBank][reg2], 0, 16777215);
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 23: { // sh
            int temp = RegBank[CurRegBank][reg1];
            if (temp > 32) temp = 0;
            if (temp < -32) temp = 0;
            if (temp >= 0) {
                CFlag = RegBank[CurRegBank][reg2] & 1;
                RegBank[CurRegBank][reg2] >>= temp;
            }
            else {
                CFlag = (RegBank[CurRegBank][reg2] >> 31) & 1;
                RegBank[CurRegBank][reg2] <<= -temp;
            }
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        }
        case 26: { // sha
            int temp = RegBank[CurRegBank][reg1];
            if (temp > 32) temp = 0;
            if (temp < -32) temp = 0;
            if (temp >= 0) {
                CFlag = RegBank[CurRegBank][reg2] & 1;
                if (RegBank[CurRegBank][reg2] < 0)
                    RegBank[CurRegBank][reg2] = (0xFFFFFFFF << (32 - temp)) | (RegBank[CurRegBank][reg2] >> temp);
                else
                    RegBank[CurRegBank][reg2] >>= temp;
            }
            else {
                CFlag = (RegBank[CurRegBank][reg2] >> 31) & 1;
                RegBank[CurRegBank][reg2] <<= -temp;
            }
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        }
        case 27: // sharq
            CFlag = RegBank[CurRegBank][reg2] & 1;
            if (RegBank[CurRegBank][reg2] < 0)
                RegBank[CurRegBank][reg2] = (0xFFFFFFFF << (32 - reg1)) | (RegBank[CurRegBank][reg2] >> reg1);
            else
                RegBank[CurRegBank][reg2] >>= reg1;
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 24: // shlq
            reg1 = 32 - reg1;
            CFlag = (RegBank[CurRegBank][reg2] >> 31) & 1;
            RegBank[CurRegBank][reg2] <<= reg1;
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 25: // shrq
            CFlag = RegBank[CurRegBank][reg2] & 1;
            RegBank[CurRegBank][reg2] >>= reg1;
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 47: // store
            RegTrace = 2;
            GPUWriteLong(RegBank[CurRegBank][reg1], RegBank[CurRegBank][reg2]);
            break;
        case 49: // store r14+n
            RegTrace = 2;
            GPUWriteLong(RegBank[CurRegBank][14] + reg1 * 4, RegBank[CurRegBank][reg2]);
            break;
        case 50: // store r15+n
            RegTrace = 2;
            GPUWriteLong(RegBank[CurRegBank][15] + reg1 * 4, RegBank[CurRegBank][reg2]);
            break;
        case 60: // store r14+rn
            RegTrace = 2;
            GPUWriteLong(RegBank[CurRegBank][14] + RegBank[CurRegBank][reg1], RegBank[CurRegBank][reg2]);
            break;
        case 61: // store r15+rn
            RegTrace = 2;
            GPUWriteLong(RegBank[CurRegBank][15] + RegBank[CurRegBank][reg1], RegBank[CurRegBank][reg2]);
            break;
        case 45: // storeb
            RegTrace = 2;
            GPUWriteByte(RegBank[CurRegBank][reg1], RegBank[CurRegBank][reg2]);
            break;
        case 48: // storep
            RegTrace = 2;
            GPUWriteLong(RegBank[CurRegBank][reg1], GPUReadLong(G_HIDATA));
            GPUWriteLong(RegBank[CurRegBank][reg1] + 4, RegBank[CurRegBank][reg2]);
            break;
        case 46: // storew
            RegTrace = 2;
            GPUWriteWord(RegBank[CurRegBank][reg1], RegBank[CurRegBank][reg2]);
            break;
        case 4: // sub
            Update_C_Flag_Sub(RegBank[CurRegBank][reg1], RegBank[CurRegBank][reg2]);
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg2] - RegBank[CurRegBank][reg1];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 5: // subc
            Update_C_Flag_Sub(RegBank[CurRegBank][reg1], RegBank[CurRegBank][reg2] + CFlag);
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg2] - RegBank[CurRegBank][reg1] - CFlag;
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 6: // subq
            if (reg1 == 0) reg1 = 32;
            Update_C_Flag_Sub(reg1, RegBank[CurRegBank][reg2]);
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg2] - reg1;
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        case 7: // subqt
            if (reg1 == 0) reg1 = 32;
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg2] - reg1;
            break;
        case 63: // pack/unpack
            if (reg1 == 0)
                RegBank[CurRegBank][reg2] =
                ((RegBank[CurRegBank][reg2] & 0x3C00000) >> 10) |
                ((RegBank[CurRegBank][reg2] & 0x001E000) >> 5) |
                (RegBank[CurRegBank][reg2] & 0x00000FF);
            else
                RegBank[CurRegBank][reg2] =
                ((RegBank[CurRegBank][reg2] << 10) & 0x3C00000) |
                ((RegBank[CurRegBank][reg2] << 5) & 0x001E000) |
                (RegBank[CurRegBank][reg2] & 0x00000FF);
            break;
        case 11: // xor
            RegBank[CurRegBank][reg2] = RegBank[CurRegBank][reg1] ^ RegBank[CurRegBank][reg2];
            Update_ZN_Flag(RegBank[CurRegBank][reg2]);
            break;
        default:
            break;
        }

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
    }
    else {
        if (opcode == 38) // movei
            GPUPC += 4;
    }

    // Jump Buffered
    if (opcode != 52 && opcode != 53 && jumpbuffered) {
        GPUPC = JMPPC;
        jumpbuffered = false;
        JumpBuffLabelUpdate();
        CheckGPUPC();
        UpdateGPUPCView();
    }
}

void Disassemble() {
    GDBUG.CodeView.Clear();
    uint8_t* walk = MemoryBuffer + LoadAddress;
    int size = ProgramSize;
    int adrs = LoadAddress;
    while (size > 1) {
        int ecart = 0;
        uint8_t w1 = *walk++;
        uint8_t w2 = *walk++;
        ecart += 2;
        uint8_t opcode = w1 >> 2;
        uint8_t reg1 = ((w1 << 3) & 31) | (w2 >> 5);
        uint8_t reg2 = w2 & 31;
        std::string instr, js;
        switch (opcode) {
        case 22: instr = "abs    r" + IntToStr(reg2); break;
        case 0: instr = "add    r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 1: instr = "addc   r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 2: instr = "addq   #" + IntToStr(reg1 == 0 ? 32 : reg1) + ",r" + IntToStr(reg2); break;
        case 3: instr = "addqt  #" + IntToStr(reg1 == 0 ? 32 : reg1) + ",r" + IntToStr(reg2); break;
        case 9: instr = "and    r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 15: instr = "bclr   #" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 14: instr = "bset   #" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 13: instr = "btst   #" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 30: instr = "cmp    r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 31: instr = "cmpq   #" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 21: instr = "div    r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 17: instr = "imult  r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 53:
            instr = "jr     ";
            js = GetJumpFlag(reg2);
            if (!js.empty()) instr += js + ",$";
            instr += (reg1 > 15)
                ? IntToHex(adrs - ((31 - reg1) * 2), 8)
                : IntToHex(adrs + (reg1 * 2), 8);
            break;
        case 52:
            instr = "jump   ";
            js = GetJumpFlag(reg2);
            if (!js.empty()) instr += js + ",";
            instr += "(r" + IntToStr(reg1) + ")";
            break;
        case 41: instr = "load   (r" + IntToStr(reg1) + "),r" + IntToStr(reg2); break;
        case 43: instr = "load   (r14+" + IntToStr(reg1) + "),r" + IntToStr(reg2); break;
        case 44: instr = "load   (r15+" + IntToStr(reg1) + "),r" + IntToStr(reg2); break;
        case 58: instr = "load   (r14+r" + IntToStr(reg1) + "),r" + IntToStr(reg2); break;
        case 59: instr = "load   (r15+r" + IntToStr(reg1) + "),r" + IntToStr(reg2); break;
        case 39: instr = "loadb  (r" + IntToStr(reg1) + "),r" + IntToStr(reg2); break;
        case 40: instr = "loadw  (r" + IntToStr(reg1) + "),r" + IntToStr(reg2); break;
        case 42: instr = "loadp  (r" + IntToStr(reg1) + "),r" + IntToStr(reg2); break;
        case 34: instr = "move   r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 51: instr = "move   PC,r" + IntToStr(reg2); break;
        case 37: instr = "movefa r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 38: {
            int value = (walk[0] << 8) | walk[1] | (walk[2] << 24) | (walk[3] << 16);
            walk += 4; ecart += 4;
            instr = "movei  #$" + IntToHex(value, 8) + ",r" + IntToStr(reg2);
            break;
        }
        case 35: instr = "moveq  #" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 36: instr = "moveta r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 16: instr = "mult   r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 8: instr = "neg    r" + IntToStr(reg2); break;
        case 12: instr = "not    r" + IntToStr(reg2); break;
        case 10: instr = "or     r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 28: instr = "ror    r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 29: instr = "rorq   #" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 32: instr = "sat8   r" + IntToStr(reg2); break;
        case 33: instr = "sat16  r" + IntToStr(reg2); break;
        case 62: instr = "sat24  r" + IntToStr(reg2); break;
        case 23: instr = "sh     r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 26: instr = "sha    r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 27: instr = "sharq  #" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 24: instr = "shlq   #" + IntToStr(32 - reg1) + ",r" + IntToStr(reg2); break;
        case 25: instr = "shrq   #" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 47: instr = "store  r" + IntToStr(reg2) + ",(r" + IntToStr(reg1) + ")"; break;
        case 49: instr = "store  r" + IntToStr(reg2) + ",(r14+" + IntToStr(reg1) + ")"; break;
        case 50: instr = "store  r" + IntToStr(reg2) + ",(r15+" + IntToStr(reg1) + ")"; break;
        case 60: instr = "store  r" + IntToStr(reg2) + ",(r14+r" + IntToStr(reg1) + ")"; break;
        case 61: instr = "store  r" + IntToStr(reg2) + ",(r15+r" + IntToStr(reg1) + ")"; break;
        case 45: instr = "storeb r" + IntToStr(reg2) + ",(r" + IntToStr(reg1) + ")"; break;
        case 48: instr = "storep r" + IntToStr(reg2) + ",(r" + IntToStr(reg1) + ")"; break;
        case 46: instr = "storew r" + IntToStr(reg2) + ",(r" + IntToStr(reg1) + ")"; break;
        case 4: instr = "sub    r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 5: instr = "subc   r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        case 6: instr = "subq   #" + IntToStr(reg1 == 0 ? 32 : reg1) + ",r" + IntToStr(reg2); break;
        case 7: instr = "subqt  #" + IntToStr(reg1 == 0 ? 32 : reg1) + ",r" + IntToStr(reg2); break;
        case 63: instr = (reg1 == 0 ? "pack   r" : "unpack r") + IntToStr(reg2); break;
        case 11: instr = "xor    r" + IntToStr(reg1) + ",r" + IntToStr(reg2); break;
        default: instr = "unknown"; break;
        }
        GDBUG.CodeView.Items.push_back({ "$" + IntToHex(adrs, 8) + ": " + instr, IMG_NODE_NOTHING });
        size -= ecart;
        adrs += ecart;
        GDBUG.Loading0.Position = 100 - ((size * 100) / ProgramSize);
    }
    if (!GDBUG.CodeView.Items.empty())
        GDBUG.CodeView.Items[0].ImageIndex = IMG_NODE_CURRENT;
}

void RunGPU() {
    gpurun = true;
    GPUWriteLong(G_CTRL, GPUReadLong(G_CTRL) | 1);
    while (gpurun) {
        if (GPUPC == GPUBP) {
            StopGPU();
        }
        else if (GPUPC >= LoadAddress + ProgramSize) {
            std::string str = "Reached program end !\nAddress = $" + IntToHex(GPUPC, 8);
            MessageDlg("Warning", str, mtWarning, mbOK, 0);
            StopGPU();
        }
        else {
            int w = GPUReadWord(GPUPC, true);
            if (w != -1)
                GPUStep((uint16_t)w, true);
        }
        ApplicationProcessMessages();
    }
    StopGPU();
}

// --- Event handler stubs (adapt to your GUI framework) ---

void Button1Click() {
    if (GDBUG.OpenDialog.Execute()) {
        if (LoadBin(GDBUG.OpenDialog.FileName, StrToInt(GDBUG.LoadAddressEdit.Text))) {
            Disassemble();
            ResetGPU();
            GDBUG.RunGPUButton.Enabled = true;
            GDBUG.StepButton.Enabled = true;
            GDBUG.SkipButton.Enabled = true;
            GDBUG.ResetGPUButton.Enabled = true;
        }
    }
}

void Button4Click() {
    if (gpurun) StopGPU();
    ApplicationTerminate();
}

void SkipButtonClick() {
    if (CodeViewCurPos == (int)GDBUG.CodeView.Items.size())
        MessageDlg("Warning", "Reached program end !", mtWarning, mbOK, 0);
    else {
        int w = GPUReadWord(GPUPC, true);
        if (w != -1) {
            noPCrefresh = false;
            GPUStep((uint16_t)w, false);
            UpdatePCView();
            UpdateRegView();
            UpdateFlagView();
            if (!noPCrefresh) {
                GDBUG.CodeView.Items[CodeViewCurPos].ImageIndex = IMG_NODE_NOTHING;
                ++CodeViewCurPos;
                if (CodeViewCurPos < (int)GDBUG.CodeView.Items.size()) {
                    GDBUG.CodeView.Items[CodeViewCurPos].ImageIndex = IMG_NODE_CURRENT;
                    GDBUG.CodeView.TopItem = CodeViewCurPos;
                }
                GDBUG.CodeView.Refresh();
            }
        }
    }
}

void StepButtonClick() {
    if (CodeViewCurPos == (int)GDBUG.CodeView.Items.size())
        MessageDlg("Warning", "Reached program end !", mtWarning, mbOK, 0);
    else {
        int w = GPUReadWord(GPUPC, true);
        if (w != -1) {
            noPCrefresh = false;
            GPUStep((uint16_t)w, true);
            UpdatePCView();
            UpdateRegView();
            UpdateFlagView();
            if (!noPCrefresh) {
                GDBUG.CodeView.Items[CodeViewCurPos].ImageIndex = IMG_NODE_NOTHING;
                ++CodeViewCurPos;
                if (CodeViewCurPos < (int)GDBUG.CodeView.Items.size()) {
                    GDBUG.CodeView.Items[CodeViewCurPos].ImageIndex = IMG_NODE_CURRENT;
                    GDBUG.CodeView.TopItem = CodeViewCurPos;
                }
                GDBUG.CodeView.Refresh();
            }
        }
    }
}

void RunGPUButtonClick() {
    if (!gpurun) {
        GDBUG.RunGPUButton.Caption = "Stop GPU (F9)";
        GDBUG.StepButton.Enabled = false;
        GDBUG.SkipButton.Enabled = false;
        GDBUG.ResetGPUButton.Enabled = false;
        GDBUG.Button1.Enabled = false;
        RunGPU();
    }
    else {
        StopGPU();
    }
}

void ResetGPUButtonClick() {
    ResetGPU();
}

// Add other event handlers as needed, following the same pattern.
