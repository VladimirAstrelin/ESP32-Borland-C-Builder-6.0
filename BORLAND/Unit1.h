//---------------------------------------------------------------------------
#ifndef Unit1H
#define Unit1H
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>   // TTimer
//---------------------------------------------------------------------------

class TForm1 : public TForm
{
__published:
        // --- Группа подключения ---
        TGroupBox *GB_ESP_CONNECTION;
        TLabel    *LBL_CONNECTION_STATUS;
        TLabel    *LBL_CHOOSE_COM_PORT;
        TComboBox *CMB_COM_PORT;
        TButton   *BTN_CONNECT;
        TButton   *BTN_DISCONNECT;

        // --- Группа CALC / PAINT ---
        TGroupBox *GB_ESP_INVOKE_CALC_PAINT;
        TLabel    *LBL_CALC_STATUS;
        TLabel    *LBL_PAINT_STATUS;

        // --- Группа назначения любой программы ---
        TGroupBox *GB_ESP_INVOKE_ANY_PROGRAM;
        TButton   *BTN_PIN_TO_D4;
        TLabel    *LBL_D4_PINNED_TO;
        TButton   *BTN_PIN_TO_D16;
        TLabel    *LBL_D16_PINNED_TO;

        // --- Статусная строка ---
        TStatusBar *SB_MAIN_STATUS_BAR;

        // --- Таймер чтения COM-порта ---
        TTimer *TimerReadCom;

        // --- Обработчики событий ---
        void __fastcall FormCreate(TObject *Sender);
        void __fastcall BTN_CONNECTClick(TObject *Sender);
        void __fastcall BTN_DISCONNECTClick(TObject *Sender);
        void __fastcall TimerReadComTimer(TObject *Sender);

        void __fastcall BTN_PIN_TO_D4Click(TObject *Sender);
        void __fastcall BTN_PIN_TO_D16Click(TObject *Sender);

private:
        // --- Дескриптор COM-порта ---
        // HANDLE — тип Windows для "номера договора" с устройством.
        // Через него все операции с портом: чтение, запись, закрытие.
        HANDLE hCom;

        // --- Флаг подключения ---
        bool connected;

        // --- Переменные ожидания ответа на команду ---
        bool          commandPending;      // true = ждём ответ от ESP32
        AnsiString    lastCommand;         // Последняя отправленная команда
        unsigned long commandStartTime;    // Когда отправили (для таймаута)

        // --- Приватные методы ---
        void RefreshComPorts();                  // Обновить список COM-портов
        bool CheckESP32(HANDLE hPort);           // Проверить что это наш ESP32
        void SendCommand(AnsiString command);    // Отправить команду в порт
        void ParseData(AnsiString data);         // Разобрать данные от ESP32
        void SetConnectedState(bool state,
                               AnsiString port); // Обновить UI при подключении/отключении

public:
        __fastcall TForm1(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif

