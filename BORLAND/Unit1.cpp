//---------------------------------------------------------------------------
// ФАЙЛ: Unit1.cpp
// ПРОГРАММА: ESP32 Button Launcher v2.1
//
// НАЗНАЧЕНИЕ:
// Windows программа которая подключается к ESP32 через COM-порт и
// запускает Windows программы по нажатию трёх физических кнопок на ESP32.
//
// ЛОГИКА РАБОТЫ:
// - Короткое нажатие на кнопку ESP32 > ЗАПУСК привязанной программы
// - Длинное нажатие (2+ секунды) > ОТКРЫВАЕТ ДИАЛОГ выбора новой программы
//---------------------------------------------------------------------------

#include <vcl.h>
#include <windows.h>
#include <registry.hpp>
#include <IniFiles.hpp>
#include <shellapi.h>

#pragma hdrstop

#include "Unit1.h"

#pragma package(smart_init)
#pragma resource "*.dfm"

TForm1 *Form1;


// ========================================================================
//
//   БЛОК 1: ИНИЦИАЛИЗАЦИЯ
//
// ========================================================================

__fastcall TForm1::TForm1(TComponent* Owner)
    : TForm(Owner)
{
    hCom             = INVALID_HANDLE_VALUE;
    connected        = false;
    commandPending   = false;
    lastCommand      = "";
    commandStartTime = 0;

    // Значения по умолчанию - стандартные программы Windows
    D4_ProgramPath  = "calc.exe";
    D16_ProgramPath = "mspaint.exe";
    D17_ProgramPath = "notepad.exe";
}

void __fastcall TForm1::FormCreate(TObject *Sender)
{
    Caption = "ESP32 Button Launcher v3.0 (LONG PRESS = change program)";

    // --- Начальные тексты ---
    LBL_CONNECTION_STATUS->Caption = "CONNECTION STATUS: DISCONNECTED";

    LBL_D4_PINNED_TO->Caption      = "D4 PINNED TO:";
    LBL_D16_PINNED_TO->Caption     = "D16 PINNED TO:";
    LBL_D17_PINNED_TO->Caption     = "D17 PINNED TO:";

    // --- Таймер ---
    TimerReadCom->Enabled  = false;
    TimerReadCom->Interval = 50;

    // --- Диалог выбора файла ---
    OpenDialog1->Filter     = "Executable files (*.exe)|*.EXE|All files (*.*)|*.*";
    OpenDialog1->Title      = "Select program to pin";
    OpenDialog1->InitialDir = "C:\\Windows\\System32\\";

    // --- Поля путей (только для чтения) ---
    EDIT_D4->ReadOnly  = true;
    EDIT_D16->ReadOnly = true;
    EDIT_D17->ReadOnly = true;
    EDIT_D4->Color  = clBtnFace;
    EDIT_D16->Color = clBtnFace;
    EDIT_D17->Color = clBtnFace;

    // --- Заполняем список портов и загружаем настройки ---
    RefreshComPorts();
    LoadSettings();

    // --- Обновляем отображение всех трёх путей ---
    UpdateD4PathDisplay();
    UpdateD16PathDisplay();
    UpdateD17PathDisplay();

    SB_MAIN_STATUS_BAR->SimpleText = "Ready";
}


// ====================================================================
//
//   БЛОК 2: COM-ПОРТ
//
// ====================================================================

void TForm1::RefreshComPorts()
{
    CMB_COM_PORT->Clear();

    TRegistry *reg = new TRegistry();
    reg->RootKey = HKEY_LOCAL_MACHINE;

    if (reg->OpenKeyReadOnly("HARDWARE\\DEVICEMAP\\SERIALCOMM"))
    {
        TStringList *values = new TStringList();
        reg->GetValueNames(values);

        for (int i = 0; i < values->Count; i++)
        {
            String portName = reg->ReadString(values->Strings[i]);
            if (portName.Pos("COM") == 1)
                CMB_COM_PORT->Items->Add(portName);
        }

        delete values;
        reg->CloseKey();
    }

    delete reg;

    if (CMB_COM_PORT->Items->Count == 0)
        CMB_COM_PORT->Items->Add("No COM ports found");

    CMB_COM_PORT->ItemIndex = 0;
}

bool TForm1::CheckESP32(HANDLE hPort)
{
    if (hPort == INVALID_HANDLE_VALUE)
        return false;

    char testCmd[] = "TEST\n";
    DWORD bytesWritten;

    for (int attempt = 0; attempt < 3; attempt++)
    {
        PurgeComm(hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);

        if (!WriteFile(hPort, testCmd, strlen(testCmd), &bytesWritten, NULL))
        {
            Sleep(100);
            continue;
        }

        Sleep(300);

        char buffer[256] = {0};
        DWORD bytesRead;

        if (!ReadFile(hPort, buffer, sizeof(buffer)-1, &bytesRead, NULL))
        {
            Sleep(100);
            continue;
        }

        if (bytesRead == 0)
        {
            Sleep(100);
            continue;
        }

        buffer[bytesRead] = '\0';

        TStringList *lines = new TStringList();
        lines->Text = String(buffer);

        bool found = false;
        for (int i = 0; i < lines->Count; i++)
        {
            if (lines->Strings[i].Trim() == "ESP32_OK")
            {
                found = true;
                break;
            }
        }

        delete lines;

        if (found)
            return true;

        Sleep(100);
    }

    return false;
}

void __fastcall TForm1::TimerReadComTimer(TObject *Sender)
{
    if (!connected || hCom == INVALID_HANDLE_VALUE)
        return;

    char  buffer[512];
    DWORD bytesRead;

    if (ReadFile(hCom, buffer, sizeof(buffer)-1, &bytesRead, NULL))
    {
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';

            TStringList *lines = new TStringList();
            lines->Text = String(buffer);

            for (int i = 0; i < lines->Count; i++)
            {
                AnsiString line = AnsiString(lines->Strings[i]).Trim();
                if (!line.IsEmpty())
                    ParseData(line);
            }

            delete lines;
        }
    }
    else
    {
        DWORD err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_INVALID_HANDLE)
        {
            BTN_DISCONNECTClick(NULL);
            ShowMessage("ESP32 disconnected unexpectedly!\nCheck USB cable.");
        }
    }

    if (commandPending && (GetTickCount() - commandStartTime > 5000))
    {
        commandPending = false;
        SB_MAIN_STATUS_BAR->SimpleText = "Command timeout — no response from ESP32";
    }
}


// ====================================================================
//
//   БЛОК 3: ПРОТОКОЛ ESP32
//
// ====================================================================

void TForm1::SendCommand(AnsiString command)
{
    if (!connected || hCom == INVALID_HANDLE_VALUE)
        return;

    if (commandPending)
    {
        SB_MAIN_STATUS_BAR->SimpleText = "Busy — waiting for ESP32 response...";
        return;
    }

    AnsiString cmd = command + "\n";
    DWORD bytesWritten;

    if (WriteFile(hCom, cmd.c_str(), cmd.Length(), &bytesWritten, NULL))
    {
        commandPending   = true;
        lastCommand      = command;
        commandStartTime = GetTickCount();
    }
}

void TForm1::ParseData(AnsiString data)
{
    data = data.Trim();
    if (data.IsEmpty()) return;

    // --- Служебные сообщения ESP32 ---
    if (data == "ESP32_OK" || data == "ESP32_READY" || data == "CONNECTED_OK")
    {
        SB_MAIN_STATUS_BAR->SimpleText = "ESP32: " + data;
        return;
    }

    // ================================================================
    // КНОПКА D4
    // ================================================================
    if (data == "BTN_D4:1")  // Короткое нажатие > ЗАПУСК
    {
        SB_MAIN_STATUS_BAR->SimpleText = "D4 short press - launching: " +
                                          ExtractFileName(D4_ProgramPath);
        ExecuteD4Program();
        return;
    }
    
    if (data == "BTN_D4:HOLD")  // Длинное нажатие > ВЫБОР ПРОГРАММЫ
    {
        SB_MAIN_STATUS_BAR->SimpleText = "D4 long press - opening dialog...";
        
        OpenDialog1->Title = "Select program for D4 button";
        OpenDialog1->FileName = "";
        
        if (OpenDialog1->Execute())
        {
            D4_ProgramPath = OpenDialog1->FileName;
            UpdateD4PathDisplay();
            SaveSettings();
            SB_MAIN_STATUS_BAR->SimpleText = "D4 now pinned to: " + 
                                              ExtractFileName(D4_ProgramPath);
        }
        else
        {
            SB_MAIN_STATUS_BAR->SimpleText = "D4 - selection cancelled";
        }
        return;
    }

    // ================================================================
    // КНОПКА D16
    // ================================================================
    if (data == "BTN_D16:1")  // Короткое нажатие > ЗАПУСК
    {
        SB_MAIN_STATUS_BAR->SimpleText = "D16 short press - launching: " +
                                          ExtractFileName(D16_ProgramPath);
        ExecuteD16Program();
        return;
    }
    
    if (data == "BTN_D16:HOLD")  // Длинное нажатие > ВЫБОР ПРОГРАММЫ
    {
        SB_MAIN_STATUS_BAR->SimpleText = "D16 long press - opening dialog...";
        
        OpenDialog1->Title = "Select program for D16 button";
        OpenDialog1->FileName = "";
        
        if (OpenDialog1->Execute())
        {
            D16_ProgramPath = OpenDialog1->FileName;
            UpdateD16PathDisplay();
            SaveSettings();
            SB_MAIN_STATUS_BAR->SimpleText = "D16 now pinned to: " + 
                                              ExtractFileName(D16_ProgramPath);
        }
        else
        {
            SB_MAIN_STATUS_BAR->SimpleText = "D16 - selection cancelled";
        }
        return;
    }

    // ================================================================
    // КНОПКА D17
    // ================================================================
    if (data == "BTN_D17:1")  // Короткое нажатие > ЗАПУСК
    {
        SB_MAIN_STATUS_BAR->SimpleText = "D17 short press - launching: " +
                                          ExtractFileName(D17_ProgramPath);
        ExecuteD17Program();
        return;
    }
    
    if (data == "BTN_D17:HOLD")  // Длинное нажатие > ВЫБОР ПРОГРАММЫ
    {
        SB_MAIN_STATUS_BAR->SimpleText = "D17 long press - opening dialog...";
        
        OpenDialog1->Title = "Select program for D17 button";
        OpenDialog1->FileName = "";
        
        if (OpenDialog1->Execute())
        {
            D17_ProgramPath = OpenDialog1->FileName;
            UpdateD17PathDisplay();
            SaveSettings();
            SB_MAIN_STATUS_BAR->SimpleText = "D17 now pinned to: " + 
                                              ExtractFileName(D17_ProgramPath);
        }
        else
        {
            SB_MAIN_STATUS_BAR->SimpleText = "D17 - selection cancelled";
        }
        return;
    }

    // --- Всё остальное ---
    SB_MAIN_STATUS_BAR->SimpleText = "ESP32: " + data;
}

void TForm1::SetConnectedState(bool state, AnsiString port)
{
    connected = state;

    if (state)
    {
        LBL_CONNECTION_STATUS->Caption =
            "CONNECTION STATUS: CONNECTED to " + port;
        LBL_CONNECTION_STATUS->Font->Color = clGreen;
        BTN_CONNECT->Enabled    = false;
        BTN_DISCONNECT->Enabled = true;
        CMB_COM_PORT->Enabled   = false;
        TimerReadCom->Enabled   = true;

        Sleep(100);
        char cmd[] = "CONNECTED\n";
        DWORD bytesWritten;
        WriteFile(hCom, cmd, strlen(cmd), &bytesWritten, NULL);
    }
    else
    {
        LBL_CONNECTION_STATUS->Caption = "CONNECTION STATUS: DISCONNECTED";
        LBL_CONNECTION_STATUS->Font->Color = clRed;
        BTN_CONNECT->Enabled    = true;
        BTN_DISCONNECT->Enabled = false;
        CMB_COM_PORT->Enabled   = true;
        TimerReadCom->Enabled   = false;

        commandPending = false;
        lastCommand    = "";
    }
}


// ====================================================================
//
//   БЛОК 4: НАСТРОЙКИ INI
//
// ====================================================================
//
// INI файл esp32_pins.ini выглядит так:
//   [Buttons]
//   D4=C:\Windows\System32\calc.exe
//   D16=C:\Windows\System32\mspaint.exe
//   D17=C:\Windows\System32\notepad.exe

void TForm1::LoadSettings()
{
    String iniPath = ExtractFilePath(Application->ExeName) + "esp32_pins.ini";

    if (FileExists(iniPath))
    {
        TIniFile *ini = new TIniFile(iniPath);

        D4_ProgramPath  = ini->ReadString("Buttons", "D4",  "calc.exe");
        D16_ProgramPath = ini->ReadString("Buttons", "D16", "mspaint.exe");
        D17_ProgramPath = ini->ReadString("Buttons", "D17", "notepad.exe");

        delete ini;

        SB_MAIN_STATUS_BAR->SimpleText = "Settings loaded from: " + iniPath;
    }
    else
    {
        SB_MAIN_STATUS_BAR->SimpleText = "First run — using default program paths";
    }
}

void TForm1::SaveSettings()
{
    String iniPath = ExtractFilePath(Application->ExeName) + "esp32_pins.ini";

    TIniFile *ini = new TIniFile(iniPath);

    ini->WriteString("Buttons", "D4",  D4_ProgramPath);
    ini->WriteString("Buttons", "D16", D16_ProgramPath);
    ini->WriteString("Buttons", "D17", D17_ProgramPath);

    delete ini;
}


// ====================================================================
//
//   БЛОК 5: ЗАПУСК ПРОГРАММ
//
// ====================================================================

void TForm1::ExecuteD4Program()
{
    if (D4_ProgramPath.IsEmpty())
    {
        SB_MAIN_STATUS_BAR->SimpleText = "No program assigned to D4!";
        return;
    }

    String cmd = "\"" + D4_ProgramPath + "\"";

    UINT result = WinExec(cmd.c_str(), SW_SHOW);
    if (result <= 31)
        ShellExecute(NULL, "open", D4_ProgramPath.c_str(), NULL, NULL, SW_SHOW);

    SB_MAIN_STATUS_BAR->SimpleText = "Launched: " + ExtractFileName(D4_ProgramPath);
}

void TForm1::ExecuteD16Program()
{
    if (D16_ProgramPath.IsEmpty())
    {
        SB_MAIN_STATUS_BAR->SimpleText = "No program assigned to D16!";
        return;
    }

    String cmd = "\"" + D16_ProgramPath + "\"";

    UINT result = WinExec(cmd.c_str(), SW_SHOW);
    if (result <= 31)
        ShellExecute(NULL, "open", D16_ProgramPath.c_str(), NULL, NULL, SW_SHOW);

    SB_MAIN_STATUS_BAR->SimpleText = "Launched: " + ExtractFileName(D16_ProgramPath);
}

void TForm1::ExecuteD17Program()
{
    if (D17_ProgramPath.IsEmpty())
    {
        SB_MAIN_STATUS_BAR->SimpleText = "No program assigned to D17!";
        return;
    }

    String cmd = "\"" + D17_ProgramPath + "\"";

    UINT result = WinExec(cmd.c_str(), SW_SHOW);
    if (result <= 31)
        ShellExecute(NULL, "open", D17_ProgramPath.c_str(), NULL, NULL, SW_SHOW);

    SB_MAIN_STATUS_BAR->SimpleText = "Launched: " + ExtractFileName(D17_ProgramPath);
}


// ====================================================================
//
//   БЛОК 6: ОБНОВЛЕНИЕ UI
//
// ====================================================================

void TForm1::UpdateD4PathDisplay()
{
    EDIT_D4->Text = D4_ProgramPath;
    LBL_D4_PINNED_TO->Caption = "D4 PINNED TO: " + ExtractFileName(D4_ProgramPath);
}

void TForm1::UpdateD16PathDisplay()
{
    EDIT_D16->Text = D16_ProgramPath;
    LBL_D16_PINNED_TO->Caption = "D16 PINNED TO: " + ExtractFileName(D16_ProgramPath);
}

void TForm1::UpdateD17PathDisplay()
{
    EDIT_D17->Text = D17_ProgramPath;
    LBL_D17_PINNED_TO->Caption = "D17 PINNED TO: " + ExtractFileName(D17_ProgramPath);
}


// =====================================================================
//
//   БЛОК 7: ОБРАБОТЧИКИ КНОПОК И СОБЫТИЙ ФОРМЫ
//
// =====================================================================

void __fastcall TForm1::BTN_CONNECTClick(TObject *Sender)
{
    if (connected)
    {
        ShowMessage("Already connected!");
        return;
    }

    if (CMB_COM_PORT->ItemIndex < 0 ||
        CMB_COM_PORT->Items->Strings[CMB_COM_PORT->ItemIndex] == "No COM ports found")
    {
        ShowMessage("Please select a COM port.");
        return;
    }

    String portName = CMB_COM_PORT->Items->Strings[CMB_COM_PORT->ItemIndex];

    // COM10+ требуют префикса "\\.\" для Windows API
    String comPort;
    String numStr  = portName.SubString(4, portName.Length() - 3);
    int    portNum = StrToIntDef(numStr, 0);

    if (portNum > 9)
        comPort = "\\\\.\\"+portName;
    else
        comPort = portName;

    hCom = CreateFile(
        comPort.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hCom == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        String msg = "Cannot open " + portName + ".\n";

        if      (err == ERROR_ACCESS_DENIED)   msg += "Port is busy. Close Serial Monitor.";
        else if (err == ERROR_FILE_NOT_FOUND)  msg += "Port not found.";
        else                                   msg += "Error code: " + IntToStr((int)err);

        ShowMessage(msg);
        return;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(hCom, &dcb))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error reading port settings.");
        return;
    }

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;

    if (!SetCommState(hCom, &dcb))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error applying port settings.");
        return;
    }

    // Медленные таймауты для CheckESP32
    COMMTIMEOUTS checkTO;
    checkTO.ReadIntervalTimeout         = 50;
    checkTO.ReadTotalTimeoutMultiplier  = 10;
    checkTO.ReadTotalTimeoutConstant    = 50;
    checkTO.WriteTotalTimeoutMultiplier = 10;
    checkTO.WriteTotalTimeoutConstant   = 50;

    if (!SetCommTimeouts(hCom, &checkTO))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error setting timeouts.");
        return;
    }

    PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);

    SB_MAIN_STATUS_BAR->SimpleText = "Checking ESP32...";
    Application->ProcessMessages();

    if (!CheckESP32(hCom))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("ESP32 not detected on " + portName + "!\n"
                    "Make sure:\n"
                    "1. Correct COM port selected\n"
                    "2. ESP32 is powered\n"
                    "3. Sketch is running");
        SB_MAIN_STATUS_BAR->SimpleText = "Connection failed";
        return;
    }

    // Быстрые таймауты для рабочего режима
    COMMTIMEOUTS workTO;
    workTO.ReadIntervalTimeout         = 0xFFFFFFFF;
    workTO.ReadTotalTimeoutMultiplier  = 0;
    workTO.ReadTotalTimeoutConstant    = 0;
    workTO.WriteTotalTimeoutMultiplier = 10;
    workTO.WriteTotalTimeoutConstant   = 50;

    SetCommTimeouts(hCom, &workTO);
    PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);

    SetConnectedState(true, portName);
    SB_MAIN_STATUS_BAR->SimpleText = "Connected to ESP32 on " + portName;
}

void __fastcall TForm1::BTN_DISCONNECTClick(TObject *Sender)
{
    if (!connected)
    {
        ShowMessage("Not connected!");
        return;
    }

    TimerReadCom->Enabled = false;

    if (hCom != INVALID_HANDLE_VALUE)
    {
        char cmd[] = "DISCONNECTED\n";
        DWORD bytesWritten;
        WriteFile(hCom, cmd, strlen(cmd), &bytesWritten, NULL);

        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
    }

    SetConnectedState(false, "");
    SB_MAIN_STATUS_BAR->SimpleText = "Disconnected";
}

void __fastcall TForm1::BTN_PIN_TO_D4Click(TObject *Sender)
{
    OpenDialog1->Title    = "Select program for D4 button";
    OpenDialog1->FileName = "";

    if (OpenDialog1->Execute())
    {
        D4_ProgramPath = OpenDialog1->FileName;
        UpdateD4PathDisplay();
        SaveSettings();
        SB_MAIN_STATUS_BAR->SimpleText = "D4 pinned to: " + ExtractFileName(D4_ProgramPath);
    }
}

void __fastcall TForm1::BTN_PIN_TO_D16Click(TObject *Sender)
{
    OpenDialog1->Title    = "Select program for D16 button";
    OpenDialog1->FileName = "";

    if (OpenDialog1->Execute())
    {
        D16_ProgramPath = OpenDialog1->FileName;
        UpdateD16PathDisplay();
        SaveSettings();
        SB_MAIN_STATUS_BAR->SimpleText = "D16 pinned to: " + ExtractFileName(D16_ProgramPath);
    }
}

void __fastcall TForm1::BTN_PIN_TO_D17Click(TObject *Sender)
{
    OpenDialog1->Title    = "Select program for D17 button";
    OpenDialog1->FileName = "";

    if (OpenDialog1->Execute())
    {
        D17_ProgramPath = OpenDialog1->FileName;
        UpdateD17PathDisplay();
        SaveSettings();
        SB_MAIN_STATUS_BAR->SimpleText = "D17 pinned to: " + ExtractFileName(D17_ProgramPath);
    }
}

void __fastcall TForm1::FormClose(TObject *Sender, TCloseAction &Action)
{
    SaveSettings();
}
