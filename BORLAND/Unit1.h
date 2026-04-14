//---------------------------------------------------------------------------
#ifndef Unit1H  // ≈сли константа Unit1H ещЄ Ќ≈ определена...
#define Unit1H  // ...то определ¤ем еЄ пр¤мо сейчас
//---------------------------------------------------------------------------
// “рюк с #ifndef / #define нужен чтобы файл не подключилс¤ дважды случайно.
// ѕредставь что это как библиотечна¤ книга Ч мы ставим штамп "¬џƒјЌќ",
// чтобы второй раз ту же книгу не выдали другому читателю.

// ѕодключаем библиотеки компонентов C++ Builder
#include <Classes.hpp>    // Ѕазовые классы дл¤ всех объектов
#include <Controls.hpp>   // Ёлементы управлени¤ (кнопки, пол¤ ввода)
#include <StdCtrls.hpp>   // —тандартные контролы (TButton, TLabel, TEdit)
#include <Forms.hpp>      // ‘орма Ч главное окно программы
#include <ComCtrls.hpp>   // ƒополнительные контролы (TStatusBar)
#include <ExtCtrls.hpp>   // TTimer Ч таймер, срабатывает через интервалы
#include <Dialogs.hpp>    // TOpenDialog Ч стандартное окно "ќткрыть файл"
#include <IniFiles.hpp>   // TIniFile Ч работа с INI-файлами настроек
//---------------------------------------------------------------------------

// ќбъ¤вл¤ем класс TForm1 Ч это главное окно нашей программы
// ƒвоеточие после TForm означает "наследуем от TForm", то есть
// берЄм всЄ что умеет обычна¤ форма и добавл¤ем свои возможности
class TForm1 : public TForm
{
__published:  // ¬сЄ что ниже Ч видно в редакторе форм и доступно извне
        // ========== √–”ѕѕј ѕќƒ Ћё„≈Ќ»я   ESP32 ==========
        // TGroupBox Ч рамка с заголовком, объедин¤ет св¤занные элементы
        TGroupBox *GB_ESP_CONNECTION;
        TLabel    *LBL_CONNECTION_STATUS;  // Ќадпись "CONNECTED" или "DISCONNECTED"
        TLabel    *LBL_CHOOSE_COM_PORT;    // Ќадпись "Choose COM port:"
        TComboBox *CMB_COM_PORT;           // ¬ыпадающий список COM-портов
        TButton   *BTN_CONNECT;            //  нопка "Connect"
        TButton   *BTN_DISCONNECT;         //  нопка "Disconnect"

        // ========== √–”ѕѕј CALC / PAINT ==========
        TGroupBox *GB_ESP_INVOKE_CALC_PAINT;
        TLabel    *LBL_CALC_STATUS;   // —татус кнопки CALC (нажата/отпущена)
        TLabel    *LBL_PAINT_STATUS;  // —татус кнопки PAINT (нажата/отпущена)

        // ========== √–”ѕѕј Ќј«Ќј„≈Ќ»я ЋёЅќ… ѕ–ќ√–јћћџ ==========
        TGroupBox *GB_ESP_INVOKE_ANY_PROGRAM;
        TButton   *BTN_PIN_TO_D4;     //  нопка выбора программы дл¤ D4
        TLabel    *LBL_D4_PINNED_TO;  // Ќадпись "D4 PINNED TO: calc.exe"
        TEdit     *EDIT_D4;           // ѕоле с полным путЄм к программе (только чтение)
        TButton   *BTN_PIN_TO_D16;    //  нопка выбора программы дл¤ D16
        TLabel    *LBL_D16_PINNED_TO; // Ќадпись "D16 PINNED TO: mspaint.exe"
        TEdit     *EDIT_D16;          // ѕоле с полным путЄм к программе (только чтение)

        // ========== —“ј“”—Ќјя —“–ќ ј (внизу окна) ==========
        TStatusBar *SB_MAIN_STATUS_BAR;

        // ========== “ј…ћ≈– „“≈Ќ»я COM-ѕќ–“ј ==========
        // —рабатывает каждые 50 миллисекунд и провер¤ет не пришли ли данные
        TTimer *TimerReadCom;

        // ========== ƒ»јЋќ√ ¬џЅќ–ј ‘ј…Ћј ==========
        // —тандартное окно Windows "ќткрыть", где можно выбрать .exe файл
        TOpenDialog *OpenDialog1;

        // ========== ќЅ–јЅќ“„» » —ќЅџ“»… ==========
        //  огда что-то происходит (клик, создание формы, тик таймера)
        // вызываютс¤ эти функции
        
        // __fastcall Ч способ вызова функций в C++ Builder (быстрый вызов)
        // TObject *Sender Ч тот кто вызвал событие (например, кака¤ кнопка)
        void __fastcall FormCreate(TObject *Sender);      // ‘орма создаЄтс¤
        void __fastcall BTN_CONNECTClick(TObject *Sender);    //  лик по Connect
        void __fastcall BTN_DISCONNECTClick(TObject *Sender); //  лик по Disconnect
        void __fastcall TimerReadComTimer(TObject *Sender);   // “ик таймера
        void __fastcall BTN_PIN_TO_D4Click(TObject *Sender);  //  лик по PIN TO D4
        void __fastcall BTN_PIN_TO_D16Click(TObject *Sender); //  лик по PIN TO D16
        void __fastcall FormClose(TObject *Sender, TCloseAction &Action); // «акрытие формы

private:  // ¬сЄ что ниже Ч скрыто от внешнего мира, только дл¤ внутреннего использовани¤
        // ========== ƒ≈— –»ѕ“ќ– COM-ѕќ–“ј ==========
        // HANDLE Ч это "номерок" который Windows выдаЄт когда мы открываем порт.
        // „ерез этот номерок мы потом читаем и пишем в порт.
        // INVALID_HANDLE_VALUE означает "порт не открыт"
        HANDLE hCom;

        // ========== ‘Ћј√ ѕќƒ Ћё„≈Ќ»я ==========
        // true = мы подключены к ESP32, false = не подключены
        bool connected;

        // ========== ѕ≈–≈ћ≈ЌЌџ≈ ƒЋя ќ∆»ƒјЌ»я ќ“¬≈“ј ==========
        //  огда мы отправл¤ем команду ESP32, мы ждЄм ответ.
        // Ёти переменные помогают не отправл¤ть новую команду пока ждЄм.
        bool          commandPending;   // true = ждЄм ответ от ESP32
        AnsiString    lastCommand;      // ѕоследн¤¤ отправленна¤ команда (дл¤ справки)
        unsigned long commandStartTime; //  огда отправили команду (в миллисекундах)

        // ========== ѕ”“»   Ќј«Ќј„≈ЌЌџћ ѕ–ќ√–јћћјћ ==========
        // ’раним в пам¤ти полные пути к .exe файлам
        String D4_ProgramPath;   // Ќапример: "C:\\Windows\\System32\\calc.exe"
        String D16_ProgramPath;  // Ќапример: "C:\\Windows\\System32\\mspaint.exe"

        // ========== ѕ–»¬ј“Ќџ≈ ћ≈“ќƒџ (функции внутри класса) ==========
        void RefreshComPorts();       // ќбновить список COM-портов из реестра Windows
        bool CheckESP32(HANDLE hPort); // ѕроверить что на порту именно наш ESP32
        void SendCommand(AnsiString command);  // ќтправить команду в порт
        void ParseData(AnsiString data);       // –азобрать данные пришедшие от ESP32
        void SetConnectedState(bool state, AnsiString port); // ќбновить UI при подключении/отключении
        
        // Ќовые методы дл¤ работы с настройками
        void LoadSettings();      // «агрузить пути к программам из INI-файла
        void SaveSettings();      // —охранить пути к программам в INI-файл
        void UpdateD4PathDisplay();   // ќбновить поле EDIT_D4 и подпись
        void UpdateD16PathDisplay();  // ќбновить поле EDIT_D16 и подпись
        void ExecuteD4Program();      // «апустить программу назначенную на D4
        void ExecuteD16Program();     // «апустить программу назначенную на D16

public:  // ¬сЄ что ниже Ч доступно всем кто использует этот класс
        // ==========  ќЌ—“–” “ќ– ==========
        // ¬ызываетс¤ когда создаЄтс¤ объект формы (при запуске программы)
        __fastcall TForm1(TComponent* Owner);
};
//---------------------------------------------------------------------------
// extern означает "эта переменна¤ существует где-то в другом файле"
// PACKAGE Ч специальный тип дл¤ C++ Builder
// Form1 Ч глобальна¤ переменна¤, через неЄ мы обращаемс¤ к главному окну
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif  //  онец услови¤ #ifndef Unit1H Ч закрываем защиту от повторного включени¤
