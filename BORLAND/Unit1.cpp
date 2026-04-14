//---------------------------------------------------------------------------
// ESP32_CALC_PAINT — Windows программа для управления ESP32
// Часть 1: подключение к ESP32 через COM-порт
//---------------------------------------------------------------------------

// Подключаем заголовочные файлы с описаниями всего что используем
#include <vcl.h>          // Visual Component Library — главная библиотека C++ Builder
#include <windows.h>      // Windows API — функции для работы с COM-портом, файлами и т.д.
#include <registry.hpp>   // Работа с реестром Windows (там хранится список COM-портов)
#include <IniFiles.hpp>   // TIniFile — работа с INI-файлами для сохранения настроек
#include <shellapi.h>     // ShellExecute — более современный способ запуска программ

#pragma hdrstop  // Прекомпилированные заголовки (ускоряет сборку в C++ Builder)

#include "Unit1.h"  // Подключаем описание нашей формы

#pragma package(smart_init)  // Умная инициализация компонентов (особенность Builder)
#pragma resource "*.dfm"     // Подключаем ресурсы формы (дизайн из визуального редактора)

// Создаём глобальную переменную Form1 — наше главное окно
TForm1 *Form1;

//---------------------------------------------------------------------------
// КОНСТРУКТОР — выполняется ПЕРВЫМ при создании объекта формы.
// Это специальная функция которая вызывается когда программа только стартует.
// 
// __fastcall — способ вызова функции (быстрый, через регистры процессора)
// TComponent* Owner — "владелец" формы, обычно само приложение (Application)
// : TForm(Owner) — вызываем конструктор родительского класса TForm
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
    : TForm(Owner)  // Сначала даём родительскому классу всё настроить
{
    // Порт ещё не открыт — ставим специальное значение "нет дескриптора"
    // INVALID_HANDLE_VALUE — это как "-1" для номерков HANDLE,
    // означает "невалидный, нерабочий, порт закрыт"
    hCom = INVALID_HANDLE_VALUE;

    // Ещё не подключены
    connected = false;

    // Никакая команда не ожидает ответа (мы ничего не отправляли)
    commandPending   = false;
    lastCommand      = "";
    commandStartTime = 0;

    // Инициализируем пути к программам СТАНДАРТНЫМИ значениями
    // Если пользователь ещё ничего не выбирал — будут запускаться эти
    D4_ProgramPath  = "calc.exe";
    D16_ProgramPath = "mspaint.exe";
}

//---------------------------------------------------------------------------
// FormCreate — вызывается автоматически когда форма ПОЛНОСТЬЮ создана.
// 
// Отличие от конструктора:
// - В конструкторе компоненты формы (кнопки, метки) ещё НЕ готовы
// - В FormCreate всё уже создано и можно настраивать UI
// 
// Поэтому настройки интерфейса делаем именно здесь, а не в конструкторе.
//---------------------------------------------------------------------------
void __fastcall TForm1::FormCreate(TObject *Sender)
{
    // ========== НАСТРОЙКА ЗАГОЛОВКА ОКНА ==========
    // Caption — это текст в верхней полоске окна
    Caption = "ESP32 CALC & PAINT v1.0";

    // ========== НАЧАЛЬНЫЕ НАДПИСИ НА МЕТКАХ ==========
    // Стрелочка -> означает "поле объекта", то есть мы обращаемся к свойству Caption
    LBL_CONNECTION_STATUS->Caption = "CONNECTION STATUS: DISCONNECTED";
    LBL_CALC_STATUS->Caption       = "INVOKE CALC.  PRESS D4.  STATUS:";
    LBL_PAINT_STATUS->Caption      = "INVOKE PAINT. PRESS D16. STATUS:";
    LBL_D4_PINNED_TO->Caption      = "D4 PINNED TO:";
    LBL_D16_PINNED_TO->Caption     = "D16 PINNED TO:";

    // ========== НАСТРОЙКА ТАЙМЕРА ==========
    // Таймер пока выключен — включится только после успешного подключения
    TimerReadCom->Enabled  = false;
    // Interval = 50 означает "срабатывать каждые 50 миллисекунд"
    // 50 мс = 20 раз в секунду — достаточно быстро чтобы не пропустить данные
    TimerReadCom->Interval = 50;  // Срабатывает каждые 50 мс

    // ========== ЗАПОЛНЯЕМ СПИСОК COM-ПОРТОВ ==========
    // Вызываем функцию которая читает реестр Windows и находит все COM-порты
    RefreshComPorts();

    SB_MAIN_STATUS_BAR->SimpleText = "Ready";

    // ========== НАСТРОЙКА ДИАЛОГА ВЫБОРА ФАЙЛА ==========
    // Filter — какие файлы показывать в диалоге
    // "Executable files (*.exe)|*.EXE" — описание и маска файлов через |
    // Вторая часть "All files (*.*)|*.*" — показать вообще все файлы
    OpenDialog1->Filter = "Executable files (*.exe)|*.EXE|All files (*.*)|*.*";
    // Title — заголовок окна диалога
    OpenDialog1->Title = "Select program to pin";
    // InitialDir — папка которая откроется по умолчанию
    // System32 — там лежат calc.exe и mspaint.exe
    OpenDialog1->InitialDir = "C:\\Windows\\System32\\";
    
    // ========== НАСТРОЙКА ПОЛЕЙ ВВОДА ПУТЕЙ ==========
    // ReadOnly = true — пользователь не может печатать в поле, только смотреть
    EDIT_D4->ReadOnly = true;
    EDIT_D16->ReadOnly = true;
    // Color = clBtnFace — цвет как у кнопки (серый), показывает что поле недоступно
    EDIT_D4->Color = clBtnFace;
    EDIT_D16->Color = clBtnFace;
    
    // ========== ЗАГРУЖАЕМ СОХРАНЁННЫЕ НАСТРОЙКИ ИЗ INI-ФАЙЛА ==========
    // Если пользователь уже выбирал программы — они загрузятся
    LoadSettings();
    
    // ========== ОБНОВЛЯЕМ ОТОБРАЖЕНИЕ ПУТЕЙ НА ФОРМЕ ==========
    // Показываем в полях EDIT_D4 и EDIT_D16 текущие пути
    UpdateD4PathDisplay();
    UpdateD16PathDisplay();
    // ========== ТЕКСТ В СТАТУСНОЙ СТРОКЕ ==========
    // SimpleText — простой текст в статусной строке (без панелей)    
    SB_MAIN_STATUS_BAR->SimpleText = "Ready";
}

//---------------------------------------------------------------------------
// RefreshComPorts — читает реестр Windows и заполняет выпадающий список
// актуальными COM-портами.
//
// Где Windows хранит список COM-портов?
// В реестре по адресу: HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM
// 
// Реестр Windows — это огромная база данных где хранятся ВСЕ настройки системы.
// Мы идём в специальную ветку где Windows записывает каждый COM-порт.
//---------------------------------------------------------------------------
void TForm1::RefreshComPorts()
{
    // Очищаем список перед заполнением
    CMB_COM_PORT->Clear();

    // Создаём объект для работы с реестром.
    // new — выделяем память в куче. В конце функции обязательно delete.
    TRegistry *reg = new TRegistry();

    // Устанавливаем свойство RootKey — указываем с какой корневой ветки
    // реестра начинать поиск. HKEY_LOCAL_MACHINE — ветка с информацией
    // о железе компьютера, именно там Windows хранит список COM-портов.
    reg->RootKey = HKEY_LOCAL_MACHINE;

    // Открываем ключ реестра только для чтения
    if (reg->OpenKeyReadOnly("HARDWARE\\DEVICEMAP\\SERIALCOMM"))
    {
        // TStringList — список строк из VCL.
        // Будем хранить в нём имена записей реестра.
        TStringList *values = new TStringList();

        // GetValueNames заполняет список именами всех записей в ключе.
        // Например: "\Device\Serial0", "\Device\VCP0" и т.д.
        reg->GetValueNames(values);

        // Перебираем все найденные записи
        for (int i = 0; i < values->Count; i++)
        {
            // ReadString по имени записи возвращает её значение —
            // реальное имя порта, например "COM3" или "COM13"
            String portName = reg->ReadString(values->Strings[i]);

            // Pos("COM") == 1 — строка начинается с "COM".
            // В C++ Builder индексация строк с 1, не с 0.
            if (portName.Pos("COM") == 1)
            {
                CMB_COM_PORT->Items->Add(portName);
            }
        }

        // Освобождаем память — всё что создано через new нужно удалить
        delete values;
        reg->CloseKey();
    }

    delete reg;

    // Если портов не нашли — добавляем заглушку
    if (CMB_COM_PORT->Items->Count == 0)
    {
        CMB_COM_PORT->Items->Add("No COM ports found");
    }

    // Автоматически выбираем первый порт в списке
    CMB_COM_PORT->ItemIndex = 0;
}

//---------------------------------------------------------------------------
// CheckESP32 — отправляет команду TEST и ищет ответ "ESP32_OK"
// среди всех строк которые прислал ESP32.
//
// Учитываем что ESP32 при старте сначала отправляет "ESP32_READY",
// поэтому читаем весь буфер и ищем нужную строку среди всех.
//
// Возвращает true если ESP32 найден и отвечает, false иначе.
//---------------------------------------------------------------------------
bool TForm1::CheckESP32(HANDLE hPort)
{
    if (hPort == INVALID_HANDLE_VALUE)
        return false;

    char testCmd[] = "TEST\n";
    DWORD bytesWritten;

    // Три попытки — ESP32 может быть занята или не сразу ответит
    for (int attempt = 0; attempt < 3; attempt++)
    {
        // Очищаем буфер перед каждой попыткой.
        // PURGE_RXCLEAR — очистить буфер приёма
        // PURGE_TXCLEAR — очистить буфер передачи
        // Это убирает ARDUINO_READY и другой "мусор" накопившийся в буфере
        PurgeComm(hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);

        // Отправляем TEST в порт
        if (!WriteFile(hPort, testCmd, strlen(testCmd), &bytesWritten, NULL))
        {
            Sleep(100);
            continue;
        }

        // Ждём пока ESP32 обработает команду и ответит.
        // Sleep здесь допустим — это единственное место в программе
        // где мы блокируемся намеренно, до запуска таймера.
        Sleep(300);

        // Читаем всё что пришло в буфер
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

        // Ставим нулевой байт в конце — признак конца строки в C
        buffer[bytesRead] = '\0';

        // Превращаем сырой буфер в строку и разбиваем по строкам.
        // ESP32 мог прислать несколько сообщений подряд, например:
        // "ESP32_READY\nESP32_OK\n"
        // TStringList автоматически разобьёт текст по \n
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

//---------------------------------------------------------------------------
// SendCommand — отправляет команду в COM-порт.
// Проверяет подключение и флаг ожидания ответа.
//---------------------------------------------------------------------------
void TForm1::SendCommand(AnsiString command)
{
    if (!connected || hCom == INVALID_HANDLE_VALUE)
        return;

    // Не отправляем новую команду пока не получили ответ на предыдущую
    if (commandPending)
    {
        SB_MAIN_STATUS_BAR->SimpleText = "Busy — waiting for ESP32 response...";
        return;
    }

    // ESP32 ждёт команду с символом \n в конце
    AnsiString cmd = command + "\n";
    DWORD bytesWritten;

    if (WriteFile(hCom, cmd.c_str(), cmd.Length(), &bytesWritten, NULL))
    {
        commandPending   = true;              // Теперь ждём ответ
        lastCommand      = command;           // Запоминаем что отправили
        commandStartTime = GetTickCount();    // Запоминаем время отправки
    }
}

//---------------------------------------------------------------------------
// ParseData — разбирает каждую строку пришедшую от ESP32.
// Вызывается из TimerReadComTimer для каждой строки отдельно.
//---------------------------------------------------------------------------
void TForm1::ParseData(AnsiString data)
{
    // Убираем пробелы и переносы строк по краям
    data = data.Trim();

    SB_MAIN_STATUS_BAR->SimpleText = "GOT: " + data;

    // Пустую строку игнорируем
    if (data.IsEmpty()) return;

    // --- Ответ на TEST (от CheckESP32, не должен сюда попасть,
    //     но на всякий случай обрабатываем) ---
    if (data == "ESP32_OK" || data == "ESP32_READY")
    {
        SB_MAIN_STATUS_BAR->SimpleText = "ESP32: " + data;
        return;
    }

    // --- Кнопка CALC (D4) ---
    // Формат: "BTN_CALC:1" (нажата) или "BTN_CALC:0" (отпущена)
    if (data == "BTN_CALC:1")
    {
        LBL_CALC_STATUS->Caption = "INVOKE CALC.  PRESS D4.  STATUS: PRESSED";
        SB_MAIN_STATUS_BAR->SimpleText = "D4 pressed — launching assigned program";
        
        // Запускаем программу назначенную на D4
        ExecuteD4Program();
        return;
    }

    if (data == "BTN_CALC:0")
    {
        LBL_CALC_STATUS->Caption = "INVOKE CALC.  PRESS D4.  STATUS: released";
        SB_MAIN_STATUS_BAR->SimpleText = "D4 released";
        return;
    }

    // --- Кнопка PAINT (D16) ---
    if (data == "BTN_PAINT:1")
    {
        LBL_PAINT_STATUS->Caption = "INVOKE PAINT. PRESS D16. STATUS: PRESSED";
        SB_MAIN_STATUS_BAR->SimpleText = "D16 pressed — launching assigned program";
        
        // Запускаем программу назначенную на D16
        ExecuteD16Program();
        return;
    }

    if (data == "BTN_PAINT:0")
    {
        LBL_PAINT_STATUS->Caption = "INVOKE PAINT. PRESS D16. STATUS: released";
        SB_MAIN_STATUS_BAR->SimpleText = "D16 released";
        return;
    }

    // --- Всё остальное — показываем в статусной строке ---
    SB_MAIN_STATUS_BAR->SimpleText = "ESP32: " + data;
}

//---------------------------------------------------------------------------
// SetConnectedState — обновляет весь UI в зависимости от состояния
// подключения. Единая точка обновления интерфейса.
//---------------------------------------------------------------------------
void TForm1::SetConnectedState(bool state, AnsiString port)
{
    connected = state;

    if (state)
    {
        LBL_CONNECTION_STATUS->Caption =
            "CONNECTION STATUS: CONNECTED to " + port;
        LBL_CONNECTION_STATUS->Font->Color = clGreen;
        BTN_CONNECT->Enabled    = false;  // Нельзя подключиться повторно
        BTN_DISCONNECT->Enabled = true;
        CMB_COM_PORT->Enabled   = false;  // Нельзя менять порт во время работы
        TimerReadCom->Enabled   = true;   // Запускаем чтение порта

        // Сообщаем ESP32 что Windows подключилась
        // Небольшая пауза чтобы ESP32 успела переключиться
        // из режима CheckESP32 в рабочий режим таймера
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
        TimerReadCom->Enabled   = false;  // Останавливаем чтение порта

        // Сбрасываем статусы кнопок
        LBL_CALC_STATUS->Caption  = "INVOKE CALC.  PRESS D4.  STATUS:";
        LBL_PAINT_STATUS->Caption = "INVOKE PAINT. PRESS D16. STATUS:";

        // Сбрасываем флаг ожидания команды
        commandPending = false;
        lastCommand    = "";
    }
}

//---------------------------------------------------------------------------
// BTN_CONNECTClick — открывает порт, настраивает параметры,
// проверяет что это наш ESP32, запускает таймер.
//---------------------------------------------------------------------------
void __fastcall TForm1::BTN_CONNECTClick(TObject *Sender)
{
    if (connected)
    {
        ShowMessage("Already connected!");
        return;
    }

    // --- Получаем имя порта из списка ---
    if (CMB_COM_PORT->ItemIndex < 0 ||
        CMB_COM_PORT->Items->Strings[CMB_COM_PORT->ItemIndex] == "No COM ports found")
    {
        ShowMessage("Please select a COM port.");
        return;
    }

    String portName = CMB_COM_PORT->Items->Strings[CMB_COM_PORT->ItemIndex];

    // --- Формируем имя порта для Windows API ---
    // COM1-COM9 открываются просто по имени: "COM3"
    // COM10 и выше требуют специального префикса: "\\.\\COM13"
    // Без префикса CreateFile вернёт ошибку для портов с двузначным номером
    String comPort;
    String numStr  = portName.SubString(4, portName.Length() - 3);
    int    portNum = StrToIntDef(numStr, 0);

    if (portNum > 9)
        comPort = "\\\\.\\"+portName;   // COM10+: добавляем префикс
    else
        comPort = portName;             // COM1-COM9: оставляем как есть

    // --- Открываем COM-порт через Windows API ---
    // CreateFile — универсальная функция Windows для открытия файлов и устройств.
    // COM-порт для Windows тоже является устройством — открываем так же как файл.
    hCom = CreateFile(
        comPort.c_str(),               // Имя порта в стиле C-строки
        GENERIC_READ | GENERIC_WRITE,  // Разрешаем чтение и запись
        0,                             // Эксклюзивный доступ — никто другой не откроет
        NULL,                          // Безопасность по умолчанию
        OPEN_EXISTING,                 // Открываем существующее устройство
        FILE_ATTRIBUTE_NORMAL,         // Обычные атрибуты
        NULL                           // Без шаблона
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

    // --- Настраиваем параметры порта ---
    // DCB (Device Control Block) — структура Windows с настройками порта.
    // Сначала читаем текущие настройки, потом меняем только нужные нам четыре.
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(hCom, &dcb))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error reading port settings.");
        return;
    }

    // Устанавливаем параметры 115200 / 8N1 — стандарт для ESP32
    dcb.BaudRate = CBR_115200;   // Скорость 115200 бод — должна совпадать с ESP32
    dcb.ByteSize = 8;            // 8 бит данных
    dcb.StopBits = ONESTOPBIT;   // 1 стоп-бит
    dcb.Parity   = NOPARITY;     // Без проверки чётности

    if (!SetCommState(hCom, &dcb))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error applying port settings.");
        return;
    }

    // --- ЭТАП 1: Медленные таймауты для CheckESP32 ---
    // ReadFile должен ЖДАТЬ ответа — ESP32 нужно время чтобы ответить.
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

    // --- Проверяем что это действительно наш ESP32 ---
    SB_MAIN_STATUS_BAR->SimpleText = "Checking ESP32...";
    Application->ProcessMessages();  // Даём UI обновиться пока идёт проверка

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

    // --- ЭТАП 2: Быстрые таймауты для рабочего режима ---
    // ReadFile должен возвращаться МГНОВЕННО.
    // Комбинация MAXDWORD + 0 + 0 означает:
    // "если в буфере есть данные — отдай, нет — сразу вернись с 0 байт"
    // Без этого ReadFile заблокирует UI на время ожидания.
    COMMTIMEOUTS workTO;
    workTO.ReadIntervalTimeout         = 0xFFFFFFFF;  // MAXDWORD
    workTO.ReadTotalTimeoutMultiplier  = 0;
    workTO.ReadTotalTimeoutConstant    = 0;
    workTO.WriteTotalTimeoutMultiplier = 10;
    workTO.WriteTotalTimeoutConstant   = 50;

    SetCommTimeouts(hCom, &workTO);
    PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);

    // --- Всё готово — обновляем UI и запускаем таймер ---
    SetConnectedState(true, portName);
    SB_MAIN_STATUS_BAR->SimpleText = "Connected to ESP32 on " + portName;
}

//---------------------------------------------------------------------------
// BTN_DISCONNECTClick — закрывает порт и сбрасывает UI.
//---------------------------------------------------------------------------
void __fastcall TForm1::BTN_DISCONNECTClick(TObject *Sender)
{
    if (!connected)
    {
        ShowMessage("Not connected!");
        return;
    }

    // Останавливаем таймер первым делом — прекращаем читать порт
    TimerReadCom->Enabled = false;

    if (hCom != INVALID_HANDLE_VALUE)
    {
        // Отправляем LED_OFF напрямую через WriteFile минуя SendCommand.
        // При отключении не нужна защита commandPending и не нужен ответ —
        // просто отправляем и сразу закрываем порт.
        char cmd[] = "DISCONNECTED\n";
        DWORD bytesWritten;
        WriteFile(hCom, cmd, strlen(cmd), &bytesWritten, NULL);

        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
    }

    SetConnectedState(false, "");
    SB_MAIN_STATUS_BAR->SimpleText = "Disconnected";
}

//---------------------------------------------------------------------------
// TimerReadComTimer — срабатывает каждые 50 мс.
// Читает всё что накопилось в буфере порта,
// разбивает на строки и передаёт каждую в ParseData.
//---------------------------------------------------------------------------
void __fastcall TForm1::TimerReadComTimer(TObject *Sender)
{
    // SB_MAIN_STATUS_BAR->SimpleText = "TIMER: " + IntToStr(GetTickCount());

    if (!connected || hCom == INVALID_HANDLE_VALUE)
        return;

    char  buffer[512];
    DWORD bytesRead;

    if (ReadFile(hCom, buffer, sizeof(buffer)-1, &bytesRead, NULL))
    {
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';

            // Разбиваем полученные данные на строки.
            // Может прийти сразу несколько сообщений, например:
            // "BTN_CALC:1\nBTN_CALC:0\n"
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
        // Ошибка чтения — порт мог отключиться физически
        DWORD err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_INVALID_HANDLE)
        {
            // Вызываем Disconnect как будто нажали кнопку
            BTN_DISCONNECTClick(NULL);
            ShowMessage("ESP32 disconnected unexpectedly!");
        }
    }

    // Проверяем таймаут команды (5 секунд без ответа)
    if (commandPending && (GetTickCount() - commandStartTime > 5000))
    {
        commandPending = false;
        SB_MAIN_STATUS_BAR->SimpleText = "Command timeout — no response from ESP32";
    }
}

//---------------------------------------------------------------------------
// BTN_PIN_TO_D4Click и BTN_PIN_TO_D16Click —
// заглушки для Части 3 (назначение любой программы).
// Пока ничего не делают.
//---------------------------------------------------------------------------
void __fastcall TForm1::BTN_PIN_TO_D4Click(TObject *Sender)
{
    OpenDialog1->Title = "Select program for D4 button";
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
    OpenDialog1->Title = "Select program for D16 button";
    OpenDialog1->FileName = "";
    
    if (OpenDialog1->Execute())
    {
        D16_ProgramPath = OpenDialog1->FileName;
        UpdateD16PathDisplay();
        SaveSettings();
        SB_MAIN_STATUS_BAR->SimpleText = "D16 pinned to: " + ExtractFileName(D16_ProgramPath);
    }
}
//---------------------------------------------------------------------------
// FormClose — вызывается когда пользователь закрывает окно (крестик или Alt+F4)
// 
// TCloseAction &Action — параметр-ссылка, через него можно сказать:
// "не закрывайся, я передумал" (Action = caNone)
void __fastcall TForm1::FormClose(TObject *Sender, TCloseAction &Action)
{
    // Перед закрытием сохраняем все настройки в INI-файл
    // Чтобы при следующем запуске всё было как пользователь настроил
        SaveSettings();
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// LoadSettings — загружает пути к программам из INI файла
//---------------------------------------------------------------------------
void TForm1::LoadSettings()
{
    // Определяем путь к INI файлу (рядом с EXE)
    String iniPath = ExtractFilePath(Application->ExeName) + "esp32_pins.ini";
    
    if (FileExists(iniPath))
    {
        TIniFile *ini = new TIniFile(iniPath);
        
        D4_ProgramPath  = ini->ReadString("Buttons", "D4", "calc.exe");
        D16_ProgramPath = ini->ReadString("Buttons", "D16", "mspaint.exe");
        
        delete ini;
        
        SB_MAIN_STATUS_BAR->SimpleText = "Settings loaded from: " + iniPath;
    }
    else
    {
        // Первый запуск — используем стандартные значения
        SB_MAIN_STATUS_BAR->SimpleText = "Using default program paths";
    }
}

//---------------------------------------------------------------------------
// SaveSettings — сохраняет пути к программам в INI файл
//---------------------------------------------------------------------------
void TForm1::SaveSettings()
{
    String iniPath = ExtractFilePath(Application->ExeName) + "esp32_pins.ini";
    
    TIniFile *ini = new TIniFile(iniPath);
    
    ini->WriteString("Buttons", "D4", D4_ProgramPath);
    ini->WriteString("Buttons", "D16", D16_ProgramPath);
    
    delete ini;
}

//---------------------------------------------------------------------------
// UpdateD4PathDisplay — обновляет поле EDIT_D4 и подпись
//---------------------------------------------------------------------------
void TForm1::UpdateD4PathDisplay()
{
    EDIT_D4->Text = D4_ProgramPath;
    
    // Извлекаем только имя файла для краткой подписи
    String fileName = ExtractFileName(D4_ProgramPath);
    LBL_D4_PINNED_TO->Caption = "D4 PINNED TO: " + fileName;
}

//---------------------------------------------------------------------------
// UpdateD16PathDisplay — обновляет поле EDIT_D16 и подпись
//---------------------------------------------------------------------------
void TForm1::UpdateD16PathDisplay()
{
    EDIT_D16->Text = D16_ProgramPath;
    
    String fileName = ExtractFileName(D16_ProgramPath);
    LBL_D16_PINNED_TO->Caption = "D16 PINNED TO: " + fileName;
}

//---------------------------------------------------------------------------
// ExecuteD4Program — запускает программу назначенную на D4
//---------------------------------------------------------------------------
void TForm1::ExecuteD4Program()
{
    if (D4_ProgramPath.IsEmpty())
    {
        SB_MAIN_STATUS_BAR->SimpleText = "No program assigned to D4";
        return;
    }
    
    // Преобразуем String в char* для WinExec
    // Используем ShellExecute для лучшего контроля, но можно и WinExec
    String cmd = "\"" + D4_ProgramPath + "\"";
    
    UINT result = WinExec(cmd.c_str(), SW_SHOW);
    if (result <= 31)
    {
        // Пробуем через ShellExecute если WinExec не справился
        ShellExecute(NULL, "open", D4_ProgramPath.c_str(), NULL, NULL, SW_SHOW);
    }
    
    SB_MAIN_STATUS_BAR->SimpleText = "Launched D4 program: " + ExtractFileName(D4_ProgramPath);
}

//---------------------------------------------------------------------------
// ExecuteD16Program — запускает программу назначенную на D16
//---------------------------------------------------------------------------
void TForm1::ExecuteD16Program()
{
    if (D16_ProgramPath.IsEmpty())
    {
        SB_MAIN_STATUS_BAR->SimpleText = "No program assigned to D16";
        return;
    }
    
    String cmd = "\"" + D16_ProgramPath + "\"";
    
    UINT result = WinExec(cmd.c_str(), SW_SHOW);
    if (result <= 31)
    {
        ShellExecute(NULL, "open", D16_ProgramPath.c_str(), NULL, NULL, SW_SHOW);
    }
    
    SB_MAIN_STATUS_BAR->SimpleText = "Launched D16 program: " + ExtractFileName(D16_ProgramPath);
}

