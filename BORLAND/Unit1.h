//---------------------------------------------------------------------------
// ФАЙЛ: Unit1.h
// НАЗНАЧЕНИЕ: Описание главного окна программы ESP32 Button Launcher
//---------------------------------------------------------------------------
#ifndef Unit1H
#define Unit1H

#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>   // TTimer
#include <Dialogs.hpp>    // TOpenDialog
#include <IniFiles.hpp>   // TIniFile

class TForm1 : public TForm
{
__published:

    // ================================================================
    // БЛОК 1: ПОДКЛЮЧЕНИЕ К ESP32
    // ================================================================
    TGroupBox *GB_ESP_CONNECTION;
    TLabel    *LBL_CONNECTION_STATUS;
    TLabel    *LBL_CHOOSE_COM_PORT;
    TComboBox *CMB_COM_PORT;
    TButton   *BTN_CONNECT;
    TButton   *BTN_DISCONNECT;

    // ================================================================
    // БЛОК 3: НАЗНАЧЕНИЕ ПРОГРАММ НА КНОПКИ
    // ================================================================
    TGroupBox *GB_ESP_INVOKE_ANY_PROGRAM;

    // --- Кнопка D4 ---
    TButton   *BTN_PIN_TO_D4;
    TLabel    *LBL_D4_PINNED_TO;
    TEdit     *EDIT_D4;

    // --- Кнопка D16 ---
    TButton   *BTN_PIN_TO_D16;
    TLabel    *LBL_D16_PINNED_TO;
    TEdit     *EDIT_D16;

    // --- Кнопка D17 ---
    TButton   *BTN_PIN_TO_D17;
    TLabel    *LBL_D17_PINNED_TO;
    TEdit     *EDIT_D17;

    // ================================================================
    // ВСПОМОГАТЕЛЬНЫЕ КОМПОНЕНТЫ
    // ================================================================
    TStatusBar  *SB_MAIN_STATUS_BAR;
    TTimer      *TimerReadCom;
    TOpenDialog *OpenDialog1;

    // ================================================================
    // ОБРАБОТЧИКИ СОБЫТИЙ
    // ================================================================
    void __fastcall FormCreate(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall BTN_CONNECTClick(TObject *Sender);
    void __fastcall BTN_DISCONNECTClick(TObject *Sender);
    void __fastcall TimerReadComTimer(TObject *Sender);
    void __fastcall BTN_PIN_TO_D4Click(TObject *Sender);
    void __fastcall BTN_PIN_TO_D16Click(TObject *Sender);
    void __fastcall BTN_PIN_TO_D17Click(TObject *Sender);  // новая

private:

    // ================================================================
    // ПЕРЕМЕННЫЕ COM-ПОРТА
    // ================================================================
    HANDLE        hCom;             // Дескриптор открытого COM-порта
    bool          connected;        // true = подключены к ESP32
    bool          commandPending;   // true = ждём ответ от ESP32
    AnsiString    lastCommand;      // Последняя отправленная команда
    unsigned long commandStartTime; // Когда отправили команду (для таймаута)

    // ================================================================
    // ПУТИ К ПРОГРАММАМ
    // Полные пути к .exe файлам для каждой кнопки ESP32
    // ================================================================
    String D4_ProgramPath;    // Например: "C:\\Windows\\System32\\calc.exe"
    String D16_ProgramPath;   // Например: "C:\\Windows\\System32\\mspaint.exe"
    String D17_ProgramPath;   // Например: "C:\\Windows\\System32\\notepad.exe"

    // ================================================================
    // ПРИВАТНЫЕ МЕТОДЫ
    // ================================================================

    // --- Группа 1: COM-порт ---
    void RefreshComPorts();
    bool CheckESP32(HANDLE hPort);
    void SendCommand(AnsiString cmd);
    void ParseData(AnsiString data);
    void SetConnectedState(bool state, AnsiString port);

    // --- Группа 2: Настройки INI ---
    void LoadSettings();
    void SaveSettings();

    // --- Группа 3: Обновление UI ---
    void UpdateD4PathDisplay();
    void UpdateD16PathDisplay();
    void UpdateD17PathDisplay();  // новая

    // --- Группа 4: Запуск программ ---
    void ExecuteD4Program();
    void ExecuteD16Program();
    void ExecuteD17Program();     // новая

public:
    __fastcall TForm1(TComponent* Owner);
};

extern PACKAGE TForm1 *Form1;

#endif

