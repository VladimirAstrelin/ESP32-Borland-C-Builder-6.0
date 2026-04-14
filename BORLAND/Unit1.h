//---------------------------------------------------------------------------
#ifndef Unit1H  // Если константа Unit1H ещё НЕ определена...
#define Unit1H  // ...то определяем её прямо сейчас
//---------------------------------------------------------------------------
// Трюк с #ifndef / #define нужен чтобы файл не подключился дважды случайно.
// Представь что это как библиотечная книга — мы ставим штамп "ВЫДАНО",
// чтобы второй раз ту же книгу не выдали другому читателю.

// Подключаем библиотеки компонентов C++ Builder
#include <Classes.hpp>    // Базовые классы для всех объектов
#include <Controls.hpp>   // Элементы управления (кнопки, поля ввода)
#include <StdCtrls.hpp>   // Стандартные контролы (TButton, TLabel, TEdit)
#include <Forms.hpp>      // Форма — главное окно программы
#include <ComCtrls.hpp>   // Дополнительные контролы (TStatusBar)
#include <ExtCtrls.hpp>   // TTimer — таймер, срабатывает через интервалы
#include <Dialogs.hpp>    // TOpenDialog — стандартное окно "Открыть файл"
#include <IniFiles.hpp>   // TIniFile — работа с INI-файлами настроек
//---------------------------------------------------------------------------

// Объявляем класс TForm1 — это главное окно нашей программы
// Двоеточие после TForm означает "наследуем от TForm", то есть
// берём всё что умеет обычная форма и добавляем свои возможности
class TForm1 : public TForm
{
__published:  // Всё что ниже — видно в редакторе форм и доступно извне
        // ========== ГРУППА ПОДКЛЮЧЕНИЯ К ESP32 ==========
        // TGroupBox — рамка с заголовком, объединяет связанные элементы
        TGroupBox *GB_ESP_CONNECTION;
        TLabel    *LBL_CONNECTION_STATUS;  // Надпись "CONNECTED" или "DISCONNECTED"
        TLabel    *LBL_CHOOSE_COM_PORT;    // Надпись "Choose COM port:"
        TComboBox *CMB_COM_PORT;           // Выпадающий список COM-портов
        TButton   *BTN_CONNECT;            // Кнопка "Connect"
        TButton   *BTN_DISCONNECT;         // Кнопка "Disconnect"

        // ========== ГРУППА CALC / PAINT ==========
        TGroupBox *GB_ESP_INVOKE_CALC_PAINT;
        TLabel    *LBL_CALC_STATUS;   // Статус кнопки CALC (нажата/отпущена)
        TLabel    *LBL_PAINT_STATUS;  // Статус кнопки PAINT (нажата/отпущена)

        // ========== ГРУППА НАЗНАЧЕНИЯ ЛЮБОЙ ПРОГРАММЫ ==========
        TGroupBox *GB_ESP_INVOKE_ANY_PROGRAM;
        TButton   *BTN_PIN_TO_D4;     // Кнопка выбора программы для D4
        TLabel    *LBL_D4_PINNED_TO;  // Надпись "D4 PINNED TO: calc.exe"
        TEdit     *EDIT_D4;           // Поле с полным путём к программе (только чтение)
        TButton   *BTN_PIN_TO_D16;    // Кнопка выбора программы для D16
        TLabel    *LBL_D16_PINNED_TO; // Надпись "D16 PINNED TO: mspaint.exe"
        TEdit     *EDIT_D16;          // Поле с полным путём к программе (только чтение)

        // ========== СТАТУСНАЯ СТРОКА (внизу окна) ==========
        TStatusBar *SB_MAIN_STATUS_BAR;

        // ========== ТАЙМЕР ЧТЕНИЯ COM-ПОРТА ==========
        // Срабатывает каждые 50 миллисекунд и проверяет не пришли ли данные
        TTimer *TimerReadCom;

        // ========== ДИАЛОГ ВЫБОРА ФАЙЛА ==========
        // Стандартное окно Windows "Открыть", где можно выбрать .exe файл
        TOpenDialog *OpenDialog1;

        // ========== ОБРАБОТЧИКИ СОБЫТИЙ ==========
        // Когда что-то происходит (клик, создание формы, тик таймера)
        // вызываются эти функции
        
        // __fastcall — способ вызова функций в C++ Builder (быстрый вызов)
        // TObject *Sender — тот кто вызвал событие (например, какая кнопка)
        void __fastcall FormCreate(TObject *Sender);      // Форма создаётся
        void __fastcall BTN_CONNECTClick(TObject *Sender);    // Клик по Connect
        void __fastcall BTN_DISCONNECTClick(TObject *Sender); // Клик по Disconnect
        void __fastcall TimerReadComTimer(TObject *Sender);   // Тик таймера
        void __fastcall BTN_PIN_TO_D4Click(TObject *Sender);  // Клик по PIN TO D4
        void __fastcall BTN_PIN_TO_D16Click(TObject *Sender); // Клик по PIN TO D16
        void __fastcall FormClose(TObject *Sender, TCloseAction &Action); // Закрытие формы

private:  // Всё что ниже — скрыто от внешнего мира, только для внутреннего использования
        // ========== ДЕСКРИПТОР COM-ПОРТА ==========
        // HANDLE — это "номерок" который Windows выдаёт когда мы открываем порт.
        // Через этот номерок мы потом читаем и пишем в порт.
        // INVALID_HANDLE_VALUE означает "порт не открыт"
        HANDLE hCom;

        // ========== ФЛАГ ПОДКЛЮЧЕНИЯ ==========
        // true = мы подключены к ESP32, false = не подключены
        bool connected;

        // ========== ПЕРЕМЕННЫЕ ДЛЯ ОЖИДАНИЯ ОТВЕТА ==========
        // Когда мы отправляем команду ESP32, мы ждём ответ.
        // Эти переменные помогают не отправлять новую команду пока ждём.
        bool          commandPending;   // true = ждём ответ от ESP32
        AnsiString    lastCommand;      // Последняя отправленная команда (для справки)
        unsigned long commandStartTime; // Когда отправили команду (в миллисекундах)

        // ========== ПУТИ К НАЗНАЧЕННЫМ ПРОГРАММАМ ==========
        // Храним в памяти полные пути к .exe файлам
        String D4_ProgramPath;   // Например: "C:\\Windows\\System32\\calc.exe"
        String D16_ProgramPath;  // Например: "C:\\Windows\\System32\\mspaint.exe"

        // ========== ПРИВАТНЫЕ МЕТОДЫ (функции внутри класса) ==========
        void RefreshComPorts();       // Обновить список COM-портов из реестра Windows
        bool CheckESP32(HANDLE hPort); // Проверить что на порту именно наш ESP32
        void SendCommand(AnsiString command);  // Отправить команду в порт
        void ParseData(AnsiString data);       // Разобрать данные пришедшие от ESP32
        void SetConnectedState(bool state, AnsiString port); // Обновить UI при подключении/отключении
        
        // Новые методы для работы с настройками
        void LoadSettings();      // Загрузить пути к программам из INI-файла
        void SaveSettings();      // Сохранить пути к программам в INI-файл
        void UpdateD4PathDisplay();   // Обновить поле EDIT_D4 и подпись
        void UpdateD16PathDisplay();  // Обновить поле EDIT_D16 и подпись
        void ExecuteD4Program();      // Запустить программу назначенную на D4
        void ExecuteD16Program();     // Запустить программу назначенную на D16

public:  // Всё что ниже — доступно всем кто использует этот класс
        // ========== КОНСТРУКТОР ==========
        // Вызывается когда создаётся объект формы (при запуске программы)
        __fastcall TForm1(TComponent* Owner);
};
//---------------------------------------------------------------------------
// extern означает "эта переменная существует где-то в другом файле"
// PACKAGE — специальный тип для C++ Builder
// Form1 — глобальная переменная, через неё мы обращаемся к главному окну
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif  // Конец условия #ifndef Unit1H — закрываем защиту от повторного включения
