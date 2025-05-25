{
GPUDBUG v0.8 beta - by Orion_ [2007-2008]

This Source Code is Licensed under the following Creative Commons License:
http://creativecommons.org/licenses/by-nc-sa/3.0/

You are free:
    * to Share - to copy, distribute and transmit the work
    * to Remix - to adapt the work

Under the following conditions:
    * Attribution. You must attribute the work in the manner specified by the author or licensor (but not in any way that suggests that they endorse you or your use of the work).
    * Noncommercial. You may not use this work for commercial purposes.
    * Share Alike. If you alter, transform, or build upon this work, you may distribute the resulting work only under the same or similar license to this one.
}

unit Unit1;

{$mode objfpc}{$H+}

interface

{Windows}
uses
  Classes, SysUtils, LResources, Forms, Controls, Graphics, Dialogs, Buttons, FPImage,
  StdCtrls, ComCtrls, LCLType;

type

  { TGDBUG }

  TGDBUG = class(TForm)
    Button1: TButton;
    G_REMAINLabel: TLabel;
    SkipButton: TButton;
    CodeView: TTreeView;
    ImageList: TImageList;
    JUMPLabel: TLabel;
    Loading0: TProgressBar;
    ToggleBox1: TToggleBox;
    RegBank0: TTreeView;
    RegBank1: TTreeView;
    RegBank0Label: TLabel;
    RegBank1Label: TLabel;
    Label3: TLabel;
    RunGPUButton: TButton;
    StepButton: TButton;
    Button4: TButton;
    OpenDialog: TOpenDialog;
    LoadAddressEdit: TEdit;
    Label4: TLabel;
    ResetGPUButton: TButton;
    FlagStatusLabel: TLabel;
    GPUPCEdit: TEdit;
    GPUBPLabel: TLabel;
    G_HIDATALabel: TLabel;
    MemWarn: TCheckBox;
    GPUMode: TRadioButton;
    DSPMode: TRadioButton;
    Label5: TLabel;
    procedure Button1Click(Sender: TObject);
    procedure Button4Click(Sender: TObject);
    procedure GPUModeChange(Sender: TObject);
    procedure SkipButtonClick(Sender: TObject);
    procedure CodeViewMouseUp(Sender: TOBject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure ResetGPUButtonClick(Sender: TObject);
    procedure StepButtonClick(Sender: TObject);
    procedure FormKeyUp(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure RunGPUButtonClick(Sender: TObject);
    procedure RegBank0MouseUp(Sender: TOBject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure GPUModeClick(Sender: TObject);
    procedure DSPModeClick(Sender: TObject);
    procedure GPUPCEditKeyDown(Sender: TObject; var Key: Word;
      Shift: TShiftState);
    procedure RegBank1MouseUp(Sender: TOBject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
  private
    { Déclarations privées }
  public
    { Déclarations publiques }
  end;

var
  GDBUG: TGDBUG;
  MemoryBuffer : ^byte;
  MemorySize : integer;
  ProgramSize : integer;
  CodeViewCurPos : integer;
  LoadAddress : integer;
  CurRegBank : integer;
  RegBank : Array [0..1,0..31] of integer;
  RegStatus : integer;
  JMPPC, GPUPC, GPUBP, ZFlag, NFlag, CFlag : integer;
  jumpbuffered, noPCrefresh, gpurun : boolean;
  

const
     IMG_NODE_NOTHING = 0;
     IMG_NODE_CURRENT = 1;
     IMG_NODE_BREAKPOINT = 2;
     IMG_NODE_REGCHANGE = 3;
     IMG_NODE_REGSTORE = 4;
     IMG_NODE_REGTEST = 5;
     G_FLAGS  = $F02100;
     G_CTRL   = $F02114;
     G_HIDATA = $F02118;
     G_REMAIN = $F0211C;
     G_RAM    = $F03000;
     D_FLAGS  = $F1A100;
     D_CTRL   = $F1A114;
     D_RAM    = $F1B000;

implementation


Procedure UpdateRegView;
Var
  i : integer;
  str : string;
Begin
  For i := 0 to 31 do
  Begin
    if i < 10 then str := ' ' else str := '';
    GDBUG.RegBank0.Items.Item[i].Text := 'r' + IntToStr(i) + ': ' + str + '$' + IntToHex(RegBank[0, i], 8);
    GDBUG.RegBank1.Items.Item[i].Text := 'r' + IntToStr(i) + ': ' + str + '$' + IntToHex(RegBank[1, i], 8);
  End;
  GDBUG.RegBank0.Refresh;
  GDBUG.RegBank1.Refresh;
End;


Procedure UpdateFlagView;
Begin
  GDBUG.FlagStatusLabel.Caption := 'Flags: Z:' + IntToStr(ZFlag) + ' N:' + IntToStr(NFlag) + ' C:' + IntToStr(CFlag);
End;


Procedure UpdatePCView;
Begin
  GDBUG.GPUPCEdit.Text := '$' + IntToHex(GPUPC, 8);
End;


Procedure UpdateGPUPCView;
Var
  adrs, size, i : integer;
  opcode : byte;
  walk : ^byte;
Begin
  walk := @MemoryBuffer^;
  Inc(walk, LoadAddress);
  size := ProgramSize;
  adrs := LoadAddress;
  CodeViewCurPos := 0;

  For i := 0 to GDBUG.CodeView.Items.Count - 1 do
    GDBUG.CodeView.Items.Item[i].ImageIndex := IMG_NODE_NOTHING;

  While (GPUPC <> adrs) and (size > 1) do
  Begin
    opcode := walk^ shr 2;
    Inc(walk, 2); Inc(adrs, 2); Dec(size, 2);
    if opcode = 38 then Begin Inc(walk, 4); Inc(adrs, 4); Dec(size, 4); End; // movei
    Inc(CodeViewCurPos);
  End;

  if CodeViewCurPos < GDBUG.CodeView.Items.Count then
  Begin
    GDBUG.CodeView.Items.Item[CodeViewCurPos].ImageIndex := IMG_NODE_CURRENT;
    GDBUG.CodeView.TopItem := GDBUG.CodeView.Items.Item[CodeViewCurPos];
  End;
  GDBUG.CodeView.Refresh;
  noPCrefresh := true;
End;


Procedure StopGPU;
Begin
  gpurun := false;
  UpdateGPUPCView;
  UpdatePCView;
  UpdateRegView;
  UpdateFlagView;
  GDBUG.RunGPUButton.Caption := 'Run GPU (F9)';
  GDBUG.StepButton.Enabled := true;
  GDBUG.SkipButton.Enabled := true;
  GDBUG.ResetGPUButton.Enabled := true;
  GDBUG.Button1.Enabled := true;
End;


Procedure JumpBuffLabelUpdate;
Begin
  if jumpbuffered = true then
  Begin
    GDBUG.JUMPLabel.Caption := 'Jump: $' + IntToHex(JMPPC, 8);
    GDBUG.JUMPLabel.Font.Style := [fsBold];
  End
  Else
  Begin
    GDBUG.JUMPLabel.Caption := 'Jump: $00000000';
    GDBUG.JUMPLabel.Font.Style := [];
  End
End;


Procedure ResetGPU;
Var
  i : integer;
Begin
  StopGPU;
  CurRegBank := 0;
  ZFlag := 0;
  NFlag := 0;
  CFlag := 0;
  GPUPC := LoadAddress;
  GDBUG.GPUPCEdit.Text := '$' + IntToHex(LoadAddress, 8);
  CodeViewCurPos := 0;
  noPCrefresh := true;
  jumpbuffered := false;
  JumpBuffLabelUpdate;

  For i := 0 to GDBUG.CodeView.Items.Count - 1 do
    GDBUG.CodeView.Items.Item[i].ImageIndex := IMG_NODE_NOTHING;
  GDBUG.CodeView.Items.Item[CodeViewCurPos].ImageIndex := IMG_NODE_CURRENT;
  GDBUG.CodeView.Refresh;

  For i := 0 to 31 do
  Begin
    RegBank[0,i] := 0;
    RegBank[1,i] := 0;
    GDBUG.RegBank0.Items.Item[i].ImageIndex := IMG_NODE_NOTHING;
    GDBUG.RegBank1.Items.Item[i].ImageIndex := IMG_NODE_NOTHING;
  End;

  UpdatePCView;
  UpdateRegView;
  UpdateFlagView;
  UpdateGPUPCView;
End;


Procedure CheckGPUPC;
Var
  str : string;
Begin
  if (GPUPC < 0) or (GPUPC > MemorySize) then
  Begin
    str := 'GPU PC outside allocated buffer !' + #13 + #10 + 'Address = $' + IntToHex(GPUPC, 8) + #13 + #10 + 'Resetting GPU !';
    MessageDlg('Error', str, mtError, [mbOK], 0);
    ResetGPU;
  End;
End;


Function LoadBin(fl : string; adrs : integer) : boolean;
var
  f : File of byte;
  str : string;
  walk : ^byte;
  walk2 : ^byte;
  value : integer;
Begin
  LoadAddress := adrs;
  Try
    AssignFile(f, fl);
    Reset(f);
    if FileSize(f) > (MemorySize - LoadAddress) then
    Begin
      str := 'File too large !';
      MessageDlg('Error', str, mtError, [mbOK], 0);
      LoadBin := false;
    End
    Else
    Begin
      ProgramSize := FileSize(f);
      walk := @MemoryBuffer^;
      Inc(walk, adrs);
      if ProgramSize > 12  then
	  Begin

	    walk2 := walk;
	    BlockRead(f, walk^, 12);
	    value :=          (walk2^ shl 24); Inc(walk2);
	    value := value or (walk2^ shl 16); Inc(walk2);
	    value :=value or (walk2^ shl 8) ; Inc(walk2);
	    value :=value or  walk2^        ; Inc(walk2);
	    Inc(walk,12);
	    if value = $42533934 then
	    Begin
		  value :=          (walk2^ shl 24); Inc(walk2);
		  value := value or (walk2^ shl 16); Inc(walk2);
		  value :=value or (walk2^ shl 8) ; Inc(walk2);
		  value :=value or  walk2^        ; Inc(walk2);
		  LoadAddress := value;
		  walk := @MemoryBuffer^;
		  Inc(walk,LoadAddress);
{		  TGDBUG.LoadAddressEdit.Text := '$' + IntToHex(LoadAddress, 8);}
		end;
	    ProgramSize := ProgramSize - 12;
      end;
      BlockRead(f, walk^, ProgramSize);
    End;
    CloseFile(f);
  Except
    MessageDlg('Error', 'Error while loading file.', mtError, [mbOK], 0);
    LoadBin := false;
  End;
  LoadBin := true;
End;


Function GPUReadLong(adrs : integer) : integer;
Var
  walk : ^byte;
  value, memadrs : integer;
  str : string;
begin
  memadrs := adrs;
  adrs := adrs and $FFFFFFFC;
  if (memadrs <> adrs) and (GDBUG.MemWarn.Checked = false) then
  Begin
    str := 'ReadLong not on a Long aligned address !' + #13 + #10 + 'Address = $' + IntToHex(memadrs, 8) + #13 + #10 + 'Should be = $' + IntToHex(adrs, 8);
    MessageDlg('Warning', str, mtWarning, [mbOK], 0);
  End;

  memadrs := adrs;
  if (memadrs >= 0) and ((memadrs + 4) <= MemorySize) then
  Begin
    walk := @MemoryBuffer^;
    Inc(walk, memadrs);

    value :=          (walk^ shl 24); Inc(walk);
    value := value or (walk^ shl 16); Inc(walk);
    value := value or (walk^ shl 8) ; Inc(walk);
    value := value or  walk^        ; Inc(walk);
    GPUReadLong := value;
  End
  Else If GDBUG.MemWarn.Checked = false then
  Begin
    str := 'ReadLong outside allocated buffer !' + #13 + #10 + 'Address = $' + IntToHex(adrs, 8);
    MessageDlg('Error', str, mtError, [mbOK], 0);
    GPUReadLong := -1;
  End;
End;


Function CheckInternalRam(memadrs : integer) : bool;
Var
  mas, mae : integer;
Begin
  If GDBUG.GPUMode.Checked = true then
  Begin
    mas := G_RAM;
    mae := G_RAM + 4*1024;
  End
  Else
  Begin
    mas := D_RAM;
    mae := D_RAM + 8*1024;
  End;

  if (memadrs >= mas) and (memadrs < mae) then
    CheckInternalRam := true
  else
    CheckInternalRam := false;
End;



Function GPUReadWord(adrs : integer; nochk : bool) : integer;
Var
  walk : ^byte;
  value, memadrs : integer;
  str : string;
begin
  memadrs := adrs;
  adrs := adrs and $FFFFFFFE;
  if (memadrs <> adrs) and (GDBUG.MemWarn.Checked = false) then
  Begin
    str := 'ReadWord not on a Word aligned address !' + #13 + #10 + 'Address = $' + IntToHex(memadrs, 8) + #13 + #10 + 'Should be = $' + IntToHex(adrs, 8);
    MessageDlg('Warning', str, mtWarning, [mbOK], 0);
  End;

  if (CheckInternalRam(memadrs) = true) and (nochk = false) then
    MessageDlg('Warning', 'ReadWord not allowed in internal ram !', mtWarning, [mbOK], 0);

  memadrs := adrs;
  if (memadrs >= 0) and ((memadrs + 2) <= MemorySize) then
  Begin
    walk := @MemoryBuffer^;
    Inc(walk, memadrs);

    value :=          (walk^ shl 8) ; Inc(walk);
    value := value or  walk^        ; Inc(walk);
    GPUReadWord := value;
  End
  Else If GDBUG.MemWarn.Checked = false then
  Begin
    str := 'ReadWord outside allocated buffer !' + #13 + #10 + 'Address = $' + IntToHex(adrs, 8);
    MessageDlg('Error', str, mtError, [mbOK], 0);
    GPUReadWord := -1;
  End;
End;


Function GPUReadByte(adrs : integer) : integer;
Var
  walk : ^byte;
  value, memadrs : integer;
  str : string;
begin
  memadrs := adrs;

  if CheckInternalRam(memadrs) = true then
    MessageDlg('Warning', 'ReadByte not allowed in internal ram !', mtWarning, [mbOK], 0);

  if (memadrs >= 0) and (memadrs < MemorySize) then
  Begin
    walk := @MemoryBuffer^;
    Inc(walk, memadrs);
    value := walk^;
    GPUReadByte := value;
  End
  Else If GDBUG.MemWarn.Checked = false then
  Begin
    str := 'ReadByte outside allocated buffer !' + #13 + #10 + 'Address = $' + IntToHex(adrs, 8);
    MessageDlg('Error', str, mtError, [mbOK], 0);
    GPUReadByte := -1;
  End;
End;


Procedure MemWriteCheck;
Begin
  If GDBUG.GPUMode.Checked = true then
  Begin
    If ((GPUReadLong(G_CTRL) and 1) = 0) and (gpurun = true) then
    Begin
      StopGPU;
      MessageDlg('Stop', 'GPU Self Stopped !', mtWarning, [mbOK], 0);
    End;
    CurRegBank := (GPUReadLong(G_FLAGS) shr 14) and 1;
    GDBUG.G_HIDATALabel.Caption := 'G_HIDATA: $' + IntToHex(GPUReadLong(G_HIDATA), 8);
    GDBUG.G_REMAINLabel.Caption := 'G_REMAIN: $' + IntToHex(GPUReadLong(G_REMAIN), 8);
  End
  Else
  Begin
    If ((GPUReadLong(D_CTRL) and 1) = 0) and (gpurun = true) then
    Begin
      StopGPU;
      MessageDlg('Stop', 'DSP Self Stopped !', mtWarning, [mbOK], 0);
    End;
    CurRegBank := (GPUReadLong(D_FLAGS) shr 14) and 1;
  End;

  GDBUG.RegBank0Label.Font.Style := [];
  GDBUG.RegBank1Label.Font.Style := [];
  If CurRegBank = 0 then
    GDBUG.RegBank0Label.Font.Style := [fsBold]
  Else
    GDBUG.RegBank1Label.Font.Style := [fsBold];
End;


Procedure GPUWriteLong(adrs, data : integer);
Var
  walk : ^byte;
  memadrs : integer;
  str : string;
begin
  memadrs := adrs;
  adrs := adrs and $FFFFFFFC;
  if (memadrs <> adrs) and (GDBUG.MemWarn.Checked = false) then
  Begin
    str := 'WriteLong not on a Long aligned address !' + #13 + #10 + 'Address = $' + IntToHex(memadrs, 8) + #13 + #10 + 'Should be = $' + IntToHex(adrs, 8);
    MessageDlg('Warning', str, mtWarning, [mbOK], 0);
  End;

  memadrs := adrs;
  if (memadrs >= 0) and ((memadrs + 4) <= MemorySize) then
  Begin
    walk := @MemoryBuffer^;
    Inc(walk, memadrs);

    walk^ := (data shr 24) and $FF; Inc(walk);
    walk^ := (data shr 16) and $FF; Inc(walk);
    walk^ := (data shr  8) and $FF; Inc(walk);
    walk^ :=  data         and $FF; Inc(walk);
  End
  Else If GDBUG.MemWarn.Checked = false then
  Begin
    str := 'WriteLong outside allocated buffer !' + #13 + #10 + 'Address = $' + IntToHex(adrs, 8);
    MessageDlg('Error', str, mtError, [mbOK], 0);
  End;
  MemWriteCheck;
End;


Procedure GPUWriteWord(adrs, data : integer);
Var
  walk : ^byte;
  memadrs : integer;
  str : string;
begin
  memadrs := adrs;
  adrs := adrs and $FFFFFFFE;
  if (memadrs <> adrs) and (GDBUG.MemWarn.Checked = false) then
  Begin
    str := 'WriteWord not on a Word aligned address !' + #13 + #10 + 'Address = $' + IntToHex(memadrs, 8) + #13 + #10 + 'Should be = $' + IntToHex(adrs, 8);
    MessageDlg('Warning', str, mtWarning, [mbOK], 0);
  End;

  if CheckInternalRam(memadrs) = true then
    MessageDlg('Warning', 'WriteWord not allowed in internal ram !', mtWarning, [mbOK], 0);

  memadrs := adrs;
  if (memadrs >= 0) and ((memadrs + 2) <= MemorySize) then
  Begin
    walk := @MemoryBuffer^;
    Inc(walk, memadrs);

    walk^ := (data shr  8) and $FF; Inc(walk);
    walk^ :=  data         and $FF; Inc(walk);
  End
  Else If GDBUG.MemWarn.Checked = false then
  Begin
    str := 'WriteWord outside allocated buffer !' + #13 + #10 + 'Address = $' + IntToHex(adrs, 8);
    MessageDlg('Error', str, mtError, [mbOK], 0);
  End;
  MemWriteCheck;
End;


Procedure GPUWriteByte(adrs, data : integer);
Var
  walk : ^byte;
  value, memadrs : integer;
  str : string;
begin
  memadrs := adrs;

  if CheckInternalRam(memadrs) = true then
    MessageDlg('Warning', 'WriteByte not allowed in internal ram !', mtWarning, [mbOK], 0);

  if (memadrs >= 0) and (memadrs < MemorySize) then
  Begin
    walk := @MemoryBuffer^;
    Inc(walk, memadrs);
    walk^ := data and $FF;
  End
  Else If GDBUG.MemWarn.Checked = false then
  Begin
    str := 'WriteByte outside allocated buffer !' + #13 + #10 + 'Address = $' + IntToHex(adrs, 8);
    MessageDlg('Error', str, mtError, [mbOK], 0);
  End;
  MemWriteCheck;
End;


Function GetJumpFlag(flag : byte) : string;
Var
  js : string;
Begin
  Case flag of
   $0 : js := '';
   $1 : js := 'NE';
   $2 : js := 'EQ';
   $4 : js := 'CC';
   $5 : js := 'HI';
   $6 : js := 'NC Z';
   $8 : js := 'CS';
   $9 : js := 'C NZ';
   $A : js := 'C Z';
  $14 : js := 'GE';
  $15 : js := 'GT';
  $16 : js := 'NN Z';
  $18 : js := 'LE';
  $19 : js := 'LT';
  $1A : js := 'N Z';
  $1F : js := 'NOT';
    else
        js := 'ERR';
  End;

  GetJumpFlag := js;
End;


Procedure Update_ZN_Flag(i : integer);
Begin
  if i < 0 then NFlag := 1 else NFlag := 0;
  if i = 0 then ZFlag := 1 else ZFlag := 0;
End;


Procedure Update_C_Flag_Add(a, b : integer);
Var
  uint1, uint2 : cardinal;
Begin
  uint1 := not a;
  uint2 := b;
  if uint2 > uint1 then CFlag := 1 else CFlag := 0;
End;


Procedure Update_C_Flag_Sub(a, b : integer);
Var
  uint1, uint2 : cardinal;
Begin
  uint1 := a;
  uint2 := b;
  if uint1 > uint2 then CFlag := 1 else CFlag := 0;
End;


Function JumpConditionMatch(condition : byte) : boolean;
Begin
  if
  (condition = 0) or
  ((condition = 1) and (ZFlag = 0)) or
  ((condition = 2) and (ZFlag = 1)) or
  ((condition = 4) and (CFlag = 0)) or
  ((condition = 5) and (CFlag = 0) and (ZFlag = 0)) or
  ((condition = 6) and (CFlag = 0) and (ZFlag = 1)) or
  ((condition = 8) and (CFlag = 1)) or
  ((condition = 9) and (CFlag = 1) and (ZFlag = 0)) or
  ((condition = $A) and (CFlag = 1) and (ZFlag = 1)) or
  ((condition = $14) and (NFlag = 0)) or
  ((condition = $15) and (NFlag = 0) and (ZFlag = 0)) or
  ((condition = $16) and (NFlag = 0) and (ZFlag = 1)) or
  ((condition = $18) and (NFlag = 1)) or
  ((condition = $19) and (NFlag = 1) and (ZFlag = 0)) or
  ((condition = $1A) and (NFlag = 1) and (ZFlag = 1))
  then JumpConditionMatch := true
  else JumpConditionMatch := false;
End;


Procedure GPUStep(w : word; exec : boolean);
Var
  opcode, reg1, reg2, RegTrace : byte;
  s16_1, s16_2 : shortint;
  u16_1, u16_2 : word;
  u32_1, u32_2, u32_3 : cardinal;
  i, temp : integer;
  TVReg : TTreeView;
Begin
  opcode := w shr 10;
  reg1 := (w shr 5) and 31;
  reg2 := w and 31;
  RegTrace := 1;

  If opcode = 36 then // moveta
  Begin
    If CurRegBank = 0 then TVReg := GDBUG.RegBank1
    Else TVReg := GDBUG.RegBank0;
  End
  Else
  Begin
    If CurRegBank = 0 then TVReg := GDBUG.RegBank0
    Else TVReg := GDBUG.RegBank1;
  End;

  // Clear Reg Trace
  For i := 0 to 31 do
    TVReg.Items.Item[i].ImageIndex := IMG_NODE_NOTHING;

  Inc(GPUPC, 2);

 if exec = true then
 Begin
  Case opcode of
  22 : Begin //abs
         NFlag := 0;
         if RegBank[CurRegBank, reg2] < 0 then CFlag := 1 else CFlag := 0;
         RegBank[CurRegBank, reg2] := abs(RegBank[CurRegBank, reg2]);
         if RegBank[CurRegBank, reg2] = 0 then ZFlag := 1 else ZFlag := 0;
       End;

   0 : Begin //add
         Update_C_Flag_Add(RegBank[CurRegBank, reg1], RegBank[CurRegBank, reg2]);
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg1] + RegBank[CurRegBank, reg2];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

   1 : Begin //addc
         Update_C_Flag_Add(RegBank[CurRegBank, reg1] + CFlag, RegBank[CurRegBank, reg2]);
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg1] + CFlag + RegBank[CurRegBank, reg2];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

   2 : Begin //addq
         if reg1 = 0 then reg1 := 32;
         Update_C_Flag_Add(reg1, RegBank[CurRegBank, reg2]);
         RegBank[CurRegBank, reg2] := reg1 + RegBank[CurRegBank, reg2];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

   3 : Begin //addqt
         if reg1 = 0 then reg1 := 32;
         RegBank[CurRegBank, reg2] := reg1 + RegBank[CurRegBank, reg2];
       End;

   9 : Begin //and
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg1] and RegBank[CurRegBank, reg2];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  15 : Begin //bclr
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] and (not (1 shl reg1));
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  14 : Begin //bset
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] or (1 shl reg1);
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  13 : Begin //btst
         RegTrace := 3;

         if (RegBank[CurRegBank, reg2] and (1 shl reg1)) = 0 then ZFlag := 1 else ZFlag := 0;
       End;

  30 : Begin //cmp
         RegTrace := 3;

         Update_C_Flag_Sub(RegBank[CurRegBank, reg1], RegBank[CurRegBank, reg2]);
         Update_ZN_Flag(RegBank[CurRegBank, reg2] - RegBank[CurRegBank, reg1]);
       End;

  31 : Begin //cmpq
         RegTrace := 3;

         Update_C_Flag_Sub(reg1, RegBank[CurRegBank, reg2]);
         Update_ZN_Flag(RegBank[CurRegBank, reg2] - reg1);
       End;

  21 : Begin //div
         u32_1 := RegBank[CurRegBank, reg1];
         u32_2 := RegBank[CurRegBank, reg2];
         u32_3 := u32_2 div u32_1;
         RegBank[CurRegBank, reg2] := u32_3;

         // Compute remainder (thanks SebRmv)
         temp := u32_2 mod u32_1;
         if (u32_3 and 1) = 0 then
            GPUWriteLong(G_REMAIN, temp - u32_1)
         else
            GPUWriteLong(G_REMAIN, temp);
       End;

  20 : Begin //imacn
       End;

  17 : Begin //imult
         temp := RegBank[CurRegBank, reg1] and $FFFF;
         if (temp > 32767) then
           temp := temp - 65536;
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] and $FFFF;
         if (RegBank[CurRegBank, reg2] > 32767) then
           RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] - 65536;
         RegBank[CurRegBank, reg2] := temp * RegBank[CurRegBank, reg2];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  18 : Begin //imultn
       End;

  53 : Begin //jr
         RegTrace := 0;

         if JumpConditionMatch(reg2) = true then
         Begin
           if reg1 > 15 then
             JMPPC := GPUPC - ((32 - reg1) * 2)
           else
             JMPPC := GPUPC + (reg1 * 2);
           jumpbuffered := true;
           JumpBuffLabelUpdate;
         End;
       End;

  52 : Begin //jump
         RegTrace := 0;

         if JumpConditionMatch(reg2) = true then
         Begin
           JMPPC := RegBank[CurRegBank, reg1];
           jumpbuffered := true;
           JumpBuffLabelUpdate;
         End;
       End;

  41 : Begin //load
         RegBank[CurRegBank, reg2] := GPUReadLong(RegBank[CurRegBank, reg1]);
       End;

  43 : Begin //load r14+n
         RegBank[CurRegBank, reg2] := GPUReadLong(RegBank[CurRegBank, 14] + reg1 * 4);
       End;

  44 : Begin //load r15+n
         RegBank[CurRegBank, reg2] := GPUReadLong(RegBank[CurRegBank, 15] + reg1 * 4);
       End;

  58 : Begin //load r14+rn
         RegBank[CurRegBank, reg2] := GPUReadLong(RegBank[CurRegBank, 14] + RegBank[CurRegBank, reg1]);
       End;

  59 : Begin //load r15+rn
         RegBank[CurRegBank, reg2] := GPUReadLong(RegBank[CurRegBank, 15] + RegBank[CurRegBank, reg1]);
       End;

  39 : Begin //loadb
         RegBank[CurRegBank, reg2] := GPUReadByte(RegBank[CurRegBank, reg1]);
       End;

  40 : Begin //loadw
         RegBank[CurRegBank, reg2] := GPUReadWord(RegBank[CurRegBank, reg1], false);
       End;

  42 : Begin //loadp
         GPUWriteLong(G_HIDATA, GPUReadLong(RegBank[CurRegBank, reg1]));
         RegBank[CurRegBank, reg2] := GPUReadLong(RegBank[CurRegBank, reg1] + 4);
       End;

  54 : Begin //mmult
       End;

  34 : Begin //move
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg1];
       End;

  51 : Begin //move pc
         RegBank[CurRegBank, reg2] := GPUPC - 2;
       End;

  37 : Begin //movefa
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank xor 1, reg1];
       End;

  38 : Begin //movei
         RegBank[CurRegBank, reg2] := GPUReadWord(GPUPC, true);
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] or (GPUReadWord(GPUPC + 2, true) shl 16);
         Inc(GPUPC, 4);
       End;

  35 : Begin //moveq
         RegBank[CurRegBank, reg2] := reg1;
       End;

  36 : Begin //moveta
         RegBank[CurRegBank xor 1, reg2] := RegBank[CurRegBank, reg1];
       End;

  55 : Begin //mtoi
       End;

  16 : Begin //mult
         u16_1 := RegBank[CurRegBank, reg1];
         u16_2 := RegBank[CurRegBank, reg2];
         RegBank[CurRegBank, reg2] := u16_1 * u16_2;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

   8 : Begin //neg
         RegBank[CurRegBank, reg2] := -RegBank[CurRegBank, reg2];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  57 : Begin //nop
         RegTrace := 0;
       End;

  56 : Begin //normi
       End;

  12 : Begin //not
         RegBank[CurRegBank, reg2] := not RegBank[CurRegBank, reg2];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  10 : Begin //or
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg1] or RegBank[CurRegBank, reg2];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  19 : Begin //resmac
       End;

  28 : Begin //ror
         CFlag := RegBank[CurRegBank, reg2] shr 31;
         reg1 := RegBank[CurRegBank, reg1] and 31;
         RegBank[CurRegBank, reg2] := (RegBank[CurRegBank, reg2] shr reg1) or (RegBank[CurRegBank, reg2] shl (32 - reg1));
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  29 : Begin //rorq
         CFlag := RegBank[CurRegBank, reg2] shr 31;
         RegBank[CurRegBank, reg2] := (RegBank[CurRegBank, reg2] shr reg1) or (RegBank[CurRegBank, reg2] shl (32 - reg1));
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  32 : Begin //sat8
         if RegBank[CurRegBank, reg2] < 0 then RegBank[CurRegBank, reg2] := 0;
         if RegBank[CurRegBank, reg2] > 255 then RegBank[CurRegBank, reg2] := 255;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  33 : Begin //sat16
         if RegBank[CurRegBank, reg2] < 0 then RegBank[CurRegBank, reg2] := 0;
         if RegBank[CurRegBank, reg2] > 65535 then RegBank[CurRegBank, reg2] := 65535;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  62 : Begin //sat24
         if RegBank[CurRegBank, reg2] < 0 then RegBank[CurRegBank, reg2] := 0;
         if RegBank[CurRegBank, reg2] > 16777215 then RegBank[CurRegBank, reg2] := 16777215;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  23 : Begin //sh
         temp := RegBank[CurRegBank, reg1];
         if temp > 32 then temp := 0;
         if temp < -32 then temp := 0;
         if temp >= 0 then
         Begin
           CFlag := RegBank[CurRegBank, reg2] and 1;
           RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] shr temp;
         End
         else
         Begin
           CFlag := RegBank[CurRegBank, reg2] shr 31;
           RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] shl (-temp);
         End;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  26 : Begin //sha
         temp := RegBank[CurRegBank, reg1];
         if temp > 32 then temp := 0;
         if temp < -32 then temp := 0;
         if temp >= 0 then
         Begin
           CFlag := RegBank[CurRegBank, reg2] and 1;
           if RegBank[CurRegBank, reg2] < 0 then
             RegBank[CurRegBank, reg2] := ($FFFFFFFF shl (32 - temp)) or (RegBank[CurRegBank, reg2] shr temp)
           else
             RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] shr temp;
         End
         else
         Begin
           CFlag := RegBank[CurRegBank, reg2] shr 31;
           RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] shl (-temp);
         End;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  27 : Begin //sharq
         CFlag := RegBank[CurRegBank, reg2] and 1;
         if RegBank[CurRegBank, reg2] < 0 then
           RegBank[CurRegBank, reg2] := ($FFFFFFFF shl (32 - reg1)) or (RegBank[CurRegBank, reg2] shr reg1)
         else
           RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] shr reg1;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  24 : Begin //shlq
         reg1 := 32 - reg1;
         CFlag := RegBank[CurRegBank, reg2] shr 31;
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] shl reg1;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  25 : Begin //shrq
         CFlag := RegBank[CurRegBank, reg2] and 1;
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] shr reg1;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

  47 : Begin //store
         RegTrace := 2;
         GPUWriteLong(RegBank[CurRegBank, reg1], RegBank[CurRegBank, reg2]);
       End;

  49 : Begin //store r14+n
         RegTrace := 2;
         GPUWriteLong(RegBank[CurRegBank, 14] + reg1 * 4, RegBank[CurRegBank, reg2]);
       End;

  50 : Begin //store r15+n
         RegTrace := 2;
         GPUWriteLong(RegBank[CurRegBank, 15] + reg1 * 4, RegBank[CurRegBank, reg2]);
       End;

  60 : Begin //store r14+rn
         RegTrace := 2;
         GPUWriteLong(RegBank[CurRegBank, 14] + RegBank[CurRegBank, reg1], RegBank[CurRegBank, reg2]);
       End;

  61 : Begin //store r15+rn
         RegTrace := 2;
         GPUWriteLong(RegBank[CurRegBank, 15] + RegBank[CurRegBank, reg1], RegBank[CurRegBank, reg2]);
       End;

  45 : Begin //storeb
         RegTrace := 2;
         GPUWriteByte(RegBank[CurRegBank, reg1], RegBank[CurRegBank, reg2]);
       End;

  48 : Begin //storep
         RegTrace := 2;
         GPUWriteLong(RegBank[CurRegBank, reg1], GPUReadLong(G_HIDATA));
         GPUWriteLong(RegBank[CurRegBank, reg1] + 4, RegBank[CurRegBank, reg2]);
       End;

  46 : Begin //storew
         RegTrace := 2;
         GPUWriteWord(RegBank[CurRegBank, reg1], RegBank[CurRegBank, reg2]);
       End;

   4 : Begin //sub
         Update_C_Flag_Sub(RegBank[CurRegBank, reg1], RegBank[CurRegBank, reg2]);
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] - RegBank[CurRegBank, reg1];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

   5 : Begin //subc
         Update_C_Flag_Sub(RegBank[CurRegBank, reg1], RegBank[CurRegBank, reg2] + CFlag);
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] - RegBank[CurRegBank, reg1] - CFlag;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

   6 : Begin //subq
         if reg1 = 0 then reg1 := 32;
         Update_C_Flag_Sub(reg1, RegBank[CurRegBank, reg2]);
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] - reg1;
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;

   7 : Begin //subqt
         if reg1 = 0 then reg1 := 32;
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg2] - reg1;
       End;

  63 : Begin
         if reg1 = 0 then
         Begin //pack
           RegBank[CurRegBank, reg2] :=
           ((RegBank[CurRegBank, reg2] and $3C00000) shr 10) or
           ((RegBank[CurRegBank, reg2] and $001E000) shr 5) or
            (RegBank[CurRegBank, reg2] and $00000FF);
         End
         else
         Begin //unpack
           RegBank[CurRegBank, reg2] :=
           ((RegBank[CurRegBank, reg2] shl 10) and $3C00000) or
           ((RegBank[CurRegBank, reg2] shl 5)  and $001E000) or
            (RegBank[CurRegBank, reg2]         and $00000FF);
         End;
       End;

  11 : Begin //xor
         RegBank[CurRegBank, reg2] := RegBank[CurRegBank, reg1] xor RegBank[CurRegBank, reg2];
         Update_ZN_Flag(RegBank[CurRegBank, reg2]);
       End;
  End;

  Case RegTrace of
    0 : TVReg.Items.Item[reg2].ImageIndex := IMG_NODE_NOTHING;
    1 : TVReg.Items.Item[reg2].ImageIndex := IMG_NODE_REGCHANGE;
    2 : Begin
          if (opcode = 49) or (opcode = 50) then
            TVReg.Items.Item[14 + (opcode - 49)].ImageIndex := IMG_NODE_REGSTORE
          else if (opcode = 60) or (opcode = 61) then
          Begin
            TVReg.Items.Item[14 + (opcode - 60)].ImageIndex := IMG_NODE_REGSTORE;
            TVReg.Items.Item[reg1].ImageIndex := IMG_NODE_REGSTORE;
          End
          else
            TVReg.Items.Item[reg1].ImageIndex := IMG_NODE_REGSTORE;
        End;
    3 : TVReg.Items.Item[reg2].ImageIndex := IMG_NODE_REGTEST;
  End;

 End
 Else
 Begin
   if opcode = 38 {movei} then
     Inc(GPUPC, 4);
 End;

  // Jump Buffered
  if (opcode <> 52) {jump} and (opcode <> 53) {jr} and (jumpbuffered = true) then
  Begin
    GPUPC := JMPPC;
    jumpbuffered := false;
    JumpBuffLabelUpdate;
    CheckGPUPC;
    UpdateGPUPCView;
  End;
End;


Procedure Disassemble;
Var
  w1, w2, opcode, reg1, reg2 : byte;
  walk : ^byte;
  node, curnode : TTreeNode;
  ecart, size, value, adrs : integer;
  instr, js : string;
Begin
  GDBUG.CodeView.Items.Clear;
  node := GDBUG.CodeView.Items.GetFirstNode;

  walk := @MemoryBuffer^;
  Inc(walk, LoadAddress);
  size := ProgramSize;
  adrs := LoadAddress;

  While size > 1 do
  Begin
    ecart := 0;

    w1 := walk^; Inc(walk);
    w2 := walk^; Inc(walk);
    Inc(ecart, 2);

    opcode := w1 shr 2;
    reg1 := ((w1 shl 3) and 31) or (w2 shr 5);
    reg2 := w2 and 31;

    Case opcode of
         22 : instr := 'abs    r' + IntToStr(reg2);
          0 : instr := 'add    r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
          1 : instr := 'addc   r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
          2 : Begin
                if reg1 = 0 then reg1 := 32;
                instr := 'addq   #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
              End;
          3 : Begin
                if reg1 = 0 then reg1 := 32;
                instr := 'addqt  #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
              End;
          9 : instr := 'and    r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         15 : instr := 'bclr   #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         14 : instr := 'bset   #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         13 : instr := 'btst   #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         30 : instr := 'cmp    r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         31 : instr := 'cmpq   #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         21 : instr := 'div    r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         20 : instr := 'imacn  r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         17 : instr := 'imult  r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         18 : instr := 'imultn r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         53 : Begin
                instr := 'jr     ';
                js := GetJumpFlag(reg2);
                if js <> '' then
                  instr := instr + js + ',$';
                if reg1 > 15 then
                  instr := instr + IntToHex(adrs - ((31 - reg1) * 2), 8)
                else
                  instr := instr + IntToHex(adrs + (reg1 * 2), 8);
              End;
         52 : Begin
                instr := 'jump   ';
                js := GetJumpFlag(reg2);
                if js <> '' then
                  instr := instr + js + ',';
                instr := instr + '(r' + IntToStr(reg1) + ')';
              End;
         41 : instr := 'load   (r' + IntToStr(reg1) + '),r' + IntToStr(reg2);
         43 : instr := 'load   (r14+' + IntToStr(reg1) + '),r' + IntToStr(reg2);
         44 : instr := 'load   (r15+' + IntToStr(reg1) + '),r' + IntToStr(reg2);
         58 : instr := 'load   (r14+r' + IntToStr(reg1) + '),r' + IntToStr(reg2);
         59 : instr := 'load   (r15+r' + IntToStr(reg1) + '),r' + IntToStr(reg2);
         39 : instr := 'loadb  (r' + IntToStr(reg1) + '),r' + IntToStr(reg2);
         40 : instr := 'loadw  (r' + IntToStr(reg1) + '),r' + IntToStr(reg2);
         42 : instr := 'loadp  (r' + IntToStr(reg1) + '),r' + IntToStr(reg2);
         54 : instr := 'mmult  r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         34 : instr := 'move   r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         51 : instr := 'move   PC,r' + IntToStr(reg2);
         37 : instr := 'movefa r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         38 : Begin
              value :=          (walk^ shl 8) ; Inc(walk);
              value := value or  walk^        ; Inc(walk);
              value := value or (walk^ shl 24); Inc(walk);
              value := value or (walk^ shl 16); Inc(walk);
              Inc(ecart, 4);
              instr := 'movei  #$' + IntToHex(value, 8) + ',r' + IntToStr(reg2);
              End;
         35 : instr := 'moveq  #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         36 : instr := 'moveta r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         55 : instr := 'mtoi   r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         16 : instr := 'mult   r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
          8 : instr := 'neg    r' + IntToStr(reg2);
         57 : instr := 'nop';
         56 : instr := 'normi  r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         12 : instr := 'not    r' + IntToStr(reg2);
         10 : instr := 'or     r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         19 : instr := 'resmac r' + IntToStr(reg2);
         28 : instr := 'ror    r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         29 : instr := 'rorq   #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         32 : instr := 'sat8   r' + IntToStr(reg2);
         33 : instr := 'sat16  r' + IntToStr(reg2);
         62 : instr := 'sat24  r' + IntToStr(reg2);
         23 : instr := 'sh     r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         26 : instr := 'sha    r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         27 : instr := 'sharq  #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         24 : instr := 'shlq   #' + IntToStr(32-reg1) + ',r' + IntToStr(reg2);
         25 : instr := 'shrq   #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         47 : instr := 'store  r' + IntToStr(reg2) + ',(r' + IntToStr(reg1) + ')';
         49 : instr := 'store  r' + IntToStr(reg2) + ',(r14+' + IntToStr(reg1) + ')';
         50 : instr := 'store  r' + IntToStr(reg2) + ',(r15+' + IntToStr(reg1) + ')';
         60 : instr := 'store  r' + IntToStr(reg2) + ',(r14+r' + IntToStr(reg1) + ')';
         61 : instr := 'store  r' + IntToStr(reg2) + ',(r15+r' + IntToStr(reg1) + ')';
         45 : instr := 'storeb r' + IntToStr(reg2) + ',(r' + IntToStr(reg1) + ')';
         48 : instr := 'storep r' + IntToStr(reg2) + ',(r' + IntToStr(reg1) + ')';
         46 : instr := 'storew r' + IntToStr(reg2) + ',(r' + IntToStr(reg1) + ')';
          4 : instr := 'sub    r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
          5 : instr := 'subc   r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
          6 : Begin
                if reg1 = 0 then reg1 := 32;
                instr := 'subq   #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
              End;
          7 : Begin
                if reg1 = 0 then reg1 := 32;
                instr := 'subqt  #' + IntToStr(reg1) + ',r' + IntToStr(reg2);
              End;
         63 : Begin
                if reg1 = 0 then
                  instr := 'pack   r' + IntToStr(reg2)
                else
                  instr := 'unpack r' + IntToStr(reg2);
              End;
         11 : instr := 'xor    r' + IntToStr(reg1) + ',r' + IntToStr(reg2);
         else
              instr := 'unknown';
    End;

    GDBUG.CodeView.Items.Add(node, '$' + IntToHex(adrs, 8) + ': ' + instr);

    Dec(size, ecart);
    Inc(adrs, ecart);
    GDBUG.Loading0.Position := 100 - ((size * 100) div ProgramSize);
  End;

  curnode := GDBUG.CodeView.Items.GetFirstNode;
  curnode.ImageIndex := IMG_NODE_CURRENT;
End;


Procedure RunGPU;
Var
   w : word;
   str : string;
Begin
  gpurun := true;
  GPUWriteLong(G_CTRL, GPUReadLong(G_CTRL) or 1);
  While gpurun = true do
  Begin
    if GPUPC = GPUBP then
    Begin
      //str := 'Breakpoint at $' + IntToHex(GPUPC, 8) + ' !';
      //MessageDlg('Warning', str, mtWarning, [mbOK], 0);
      StopGPU;
    End
    else
    if GPUPC >= LoadAddress + ProgramSize then
    Begin
      str := 'Reached program end !' + #13 + #10 + 'Address = $' + IntToHex(GPUPC, 8);
      MessageDlg('Warning', str, mtWarning, [mbOK], 0);
      StopGPU;
    End
    Else
    Begin
      w := GPUReadWord(GPUPC, true);
      if w <> -1 then
        GPUStep(w, true);
    End;
    Application.ProcessMessages;
  End;
  StopGPU;
End;


procedure TGDBUG.Button1Click(Sender: TObject);
begin
  If OpenDialog.Execute then
  Begin
    if LoadBin(OpenDialog.FileName, strtoint(LoadAddressEdit.Text)) = true then
    Begin
      Disassemble;
      ResetGPU;
      RunGPUButton.Enabled := true;
      StepButton.Enabled := true;
      SkipButton.Enabled := true;
      ResetGPUButton.Enabled := true;
    End;
  End;
end;

procedure TGDBUG.Button4Click(Sender: TObject);
begin
  if gpurun = true then StopGPU;
  Application.Terminate;
end;

procedure TGDBUG.GPUModeChange(Sender: TObject);
begin

end;

procedure TGDBUG.SkipButtonClick(Sender: TObject);
var
  w : word;
begin
  if CodeViewCurPos = CodeView.Items.Count then
    MessageDlg('Warning', 'Reached program end !', mtWarning, [mbOK], 0)
  Else
  Begin
    w := GPUReadWord(GPUPC, true);
    if w <> -1 then
    Begin
      noPCrefresh := false;

      GPUStep(w, false);
      UpdatePCView;
      UpdateRegView;
      UpdateFlagView;

      if noPCrefresh = false then
      Begin
        CodeView.Items.Item[CodeViewCurPos].ImageIndex := IMG_NODE_NOTHING;
        Inc(CodeViewCurPos);
        if CodeViewCurPos < CodeView.Items.Count then
        Begin
          CodeView.Items.Item[CodeViewCurPos].ImageIndex := IMG_NODE_CURRENT;
          CodeView.TopItem := GDBUG.CodeView.Items.Item[CodeViewCurPos];
        End;
        CodeView.Refresh;
      End;
    End;
  End;
end;

procedure TGDBUG.CodeViewMouseUp(Sender: TOBject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
Var
  str : string;
begin
  If CodeView.Selected <> nil then
  Begin
    CodeView.Selected.SelectedIndex := IMG_NODE_BREAKPOINT;
    str := copy(CodeView.Selected.Text, 0, 9);
    GPUBP := StrToInt(str);
    GPUBPLabel.Caption := 'BP: $' + IntToHex(GPUBP, 8);
    CodeView.Refresh;
  End;
end;

procedure TGDBUG.FormCreate(Sender: TObject);
var
  node1, node2 : TTreeNode;
  i : integer;
  str : string;
begin
  MemorySize := $F1D000;
  GetMem(MemoryBuffer, MemorySize);
  if MemoryBuffer = nil then
  Begin
    MessageDlg('Error', 'Not enough memory ! (need ~15Meg)', mtError, [mbOK], 0);
    Application.Terminate;
  End;

  RegBank0.Items.Clear;
  RegBank1.Items.Clear;
  node1 := RegBank0.Items.GetFirstNode;
  node2 := RegBank1.Items.GetFirstNode;
  gpurun := false;

  For i := 0 to 31 do
  Begin
    RegBank[0,i] := 0;
    RegBank[1,i] := 0;
    if i < 10 then str := ' ' else str := '';
    RegBank0.Items.Add(node1, 'r' + IntToStr(i) + ': ' + str + '$00000000');
    RegBank1.Items.Add(node2, 'r' + IntToStr(i) + ': ' + str + '$00000000');
  End;
end;

procedure TGDBUG.FormDestroy(Sender: TObject);
begin
  FreeMemory(MemoryBuffer);
end;

procedure TGDBUG.ResetGPUButtonClick(Sender: TObject);
begin
  ResetGPU;
end;

procedure TGDBUG.StepButtonClick(Sender: TObject);
var
  w : word;
begin
  if CodeViewCurPos = CodeView.Items.Count then
    MessageDlg('Warning', 'Reached program end !', mtWarning, [mbOK], 0)
  Else
  Begin
    w := GPUReadWord(GPUPC, true);
    if w <> -1 then
    Begin
      noPCrefresh := false;

      GPUStep(w, true);
      UpdatePCView;
      UpdateRegView;
      UpdateFlagView;

      if noPCrefresh = false then
      Begin
        CodeView.Items.Item[CodeViewCurPos].ImageIndex := IMG_NODE_NOTHING;
        Inc(CodeViewCurPos);
        if CodeViewCurPos < CodeView.Items.Count then
        Begin
          CodeView.Items.Item[CodeViewCurPos].ImageIndex := IMG_NODE_CURRENT;
          CodeView.TopItem := GDBUG.CodeView.Items.Item[CodeViewCurPos];
        End;
        CodeView.Refresh;
      End;
    End;
  End;
end;

procedure TGDBUG.FormKeyUp(Sender: TObject; var Key: Word; Shift: TShiftState);
begin
  if Key = VK_F9 then
  Begin
    if RunGPUButton.Enabled = true then
      RunGPUButtonClick(Sender);
  End
  else if Key = VK_F10 then
  Begin
    if StepButton.Enabled = true then
      StepButtonClick(Sender);
  End
  else if Key = VK_F11 then
  Begin
    if SkipButton.Enabled = true then
      SkipButtonClick(Sender);
  End
  else if Key = VK_F12 then
  Begin
    if ResetGPUButton.Enabled = true then
      ResetGPUButtonClick(Sender);
  End;
  Key := 0;
end;


procedure TGDBUG.RunGPUButtonClick(Sender: TObject);
var
  value : integer;
begin
  if gpurun = false then
  Begin
    RunGPUButton.Caption := 'Stop GPU (F9)';
    StepButton.Enabled := false;
    SkipButton.Enabled := false;
    ResetGPUButton.Enabled := false;
    Button1.Enabled := false;
    RunGPU;
  End
  else
  Begin
    StopGPU;
  End
end;


procedure TGDBUG.RegBank0MouseUp(Sender: TOBject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
Var
  str : string;
  reg : integer;
  r : boolean;
begin
  If RegBank0.Selected <> nil then
  Begin
    str := copy(RegBank0.Selected.Text, 3, 1);
    if str = ':' then
      reg := StrToInt(copy(RegBank0.Selected.Text, 2, 1))
    Else
      reg := StrToInt(copy(RegBank0.Selected.Text, 2, 2));
    str := '$' + IntToHex(RegBank[0, reg], 8);
    r := InputQuery('Register modification', 'r' + IntToStr(reg) + ' =', str);
    if r then
    Begin
      RegBank[0, reg] := StrToInt(str);
      UpdateRegView;
    End;
  End;
end;

procedure TGDBUG.GPUModeClick(Sender: TObject);
begin
  LoadAddressEdit.Text := '$00F03000';
end;

procedure TGDBUG.DSPModeClick(Sender: TObject);
begin
  LoadAddressEdit.Text := '$00F1B000';
end;

procedure TGDBUG.GPUPCEditKeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
  if Key = VK_RETURN then
  Begin
    GPUPC := StrToInt(GPUPCEdit.Text);
    CheckGPUPC;
    UpdateGPUPCView;
    UpdatePCView;
  End;
end;

procedure TGDBUG.RegBank1MouseUp(Sender: TOBject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
Var
  str : string;
  reg : integer;
  r : boolean;
begin
  If RegBank1.Selected <> nil then
  Begin
    str := copy(RegBank1.Selected.Text, 3, 1);
    if str = ':' then
      reg := StrToInt(copy(RegBank1.Selected.Text, 2, 1))
    Else
      reg := StrToInt(copy(RegBank1.Selected.Text, 2, 2));
    str := '$' + IntToHex(RegBank[1, reg], 8);
    r := InputQuery('Register modification', 'r' + IntToStr(reg) + ' =', str);
    if r then
    Begin
      RegBank[1, reg] := StrToInt(str);
      UpdateRegView;
    End;
  End;
end;

initialization
  {$I Unit1.lrs}

end.
