//---------------------------------------------------------------------------
// ФАЙЛ: Unit1.cpp
// ПРОГРАММА: ESP32 CALC & PAINT v1.0
//
// НАЗНАЧЕНИЕ:
// Windows программа которая подключается к ESP32 через COM-порт и
// запускает Windows программы по нажатию физических кнопок на ESP32.
//
// СТРУКТУРА ФАЙЛА (логические блоки):
// ════════════════════════════════════════════════════════════════════
//  БЛОК 1 — ИНИЦИАЛИЗАЦИЯ      Конструктор и FormCreate
//  БЛОК 2 — COM-ПОРТ           Поиск портов, подключение, чтение
//  БЛОК 3 — ПРОТОКОЛ ESP32     Отправка команд, разбор ответов
//  БЛОК 4 — НАСТРОЙКИ INI      Загрузка и сохранение путей к программам
//  БЛОК 5 — ЗАПУСК ПРОГРАММ    Выполнение .exe файлов
//  БЛОК 6 — ОБНОВЛЕНИЕ UI      Синхронизация интерфейса с реальностью
//  БЛОК 7 — ОБРАБОТЧИКИ КНОПОК Реакция на действия пользователя
// ════════════════════════════════════════════════════════════════════
//
// ПРОТОКОЛ ОБЩЕНИЯ С ESP32:
//   Windows → ESP32:  "TEST\n"        проверка связи
//                     "CONNECTED\n"   уведомление об успешном подключении
//                     "DISCONNECTED\n" уведомление об отключении
//   ESP32 → Windows:  "ESP32_READY"   ESP32 стартовала
//                     "ESP32_OK"      ответ на TEST
//                     "CONNECTED_OK"  ответ на CONNECTED
//                     "BTN_CALC:1"    кнопка D4 нажата
//                     "BTN_CALC:0"    кнопка D4 отпущена
//                     "BTN_PAINT:1"   кнопка D16 нажата
//                     "BTN_PAINT:0"   кнопка D16 отпущена
//---------------------------------------------------------------------------

// ════════════════════════════════════════════════════════════════════
// ПОДКЛЮЧЕНИЕ ЗАГОЛОВОЧНЫХ ФАЙЛОВ
// ════════════════════════════════════════════════════════════════════

#include <vcl.h>          // Visual Component Library — главная библиотека C++ Builder
                          // Без неё не работают TForm, TButton, TLabel и всё остальное

#include <windows.h>      // Windows API — низкоуровневые функции операционной системы:
                          // CreateFile, ReadFile, WriteFile — работа с COM-портом
                          // GetLastError — узнать код последней ошибки
                          // GetTickCount — текущее время в миллисекундах

#include <registry.hpp>   // TRegistry — работа с реестром Windows.
                          // Реестр — огромная база данных настроек Windows.
                          // Там хранится список всех COM-портов.

#include <IniFiles.hpp>   // TIniFile — работа с INI-файлами.
                          // INI — простой текстовый формат для настроек:
                          // [Раздел]
                          // Ключ=Значение

#include <shellapi.h>     // ShellExecute — запуск программ и файлов через Windows Shell.
                          // Умнее чем WinExec: понимает ассоциации файлов,
                          // права администратора и т.д.

#pragma hdrstop           // Директива Borland: всё ДО этой строки компилируется
                          // один раз и кешируется (ускоряет повторную сборку).
                          // Всё ПОСЛЕ — компилируется заново при каждом изменении.

#include "Unit1.h"        // Описание нашей формы (должно идти ПОСЛЕ hdrstop)

#pragma package(smart_init)  // Оптимизация порядка инициализации модулей
#pragma resource "*.dfm"     // Подключаем файл с дизайном формы (расположение кнопок и т.д.)

// Глобальная переменная — указатель на главное окно программы.
// Создаётся один раз здесь, объявлена как extern в Unit1.h.
TForm1 *Form1;


// ════════════════════════════════════════════════════════════════════
//
//   БЛОК 1: ИНИЦИАЛИЗАЦИЯ
//   Что происходит при запуске программы
//
// ════════════════════════════════════════════════════════════════════

//---------------------------------------------------------------------------
// КОНСТРУКТОР TForm1
//
// Вызывается САМЫМ ПЕРВЫМ при создании объекта формы.
// Задача: инициализировать переменные до того как форма станет видна.
//
// ВАЖНО: здесь нельзя обращаться к компонентам формы (кнопкам, меткам)
// потому что они ещё не созданы! Только переменные.
// Компоненты будут готовы только в FormCreate.
//
// Синтаксис: __fastcall TForm1::TForm1(TComponent* Owner)
//   __fastcall   — способ передачи аргументов (через регистры, быстрее)
//   TForm1::     — этот конструктор принадлежит классу TForm1
//   TForm1(...)  — имя совпадает с классом = это конструктор
//   : TForm(Owner) — сначала вызываем конструктор родителя TForm
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
    : TForm(Owner)  // Передаём Owner родителю — он создаёт базовое окно Windows
{
    // --- Инициализация COM-порта ---

    // INVALID_HANDLE_VALUE = (HANDLE)-1 = специальное значение "нет дескриптора"
    // Пока порт не открыт — ставим это значение как признак "закрыто"
    hCom = INVALID_HANDLE_VALUE;

    // Ещё не подключены к ESP32
    connected = false;

    // --- Инициализация системы команд ---

    // Никакая команда не отправлена — не нужно ждать ответа
    commandPending   = false;
    lastCommand      = "";    // Пустая строка = не было команд
    commandStartTime = 0;     // Время = 0 = команд не было

    // --- Значения по умолчанию для путей к программам ---

    // Если пользователь ещё ничего не выбирал (первый запуск),
    // используем стандартные системные программы Windows.
    // "calc.exe" и "mspaint.exe" Windows найдёт сама через PATH.
    D4_ProgramPath  = "calc.exe";
    D16_ProgramPath = "mspaint.exe";
}

//---------------------------------------------------------------------------
// FormCreate — обработчик события OnCreate формы.
//
// Вызывается АВТОМАТИЧЕСКИ когда форма полностью создана и все
// компоненты (кнопки, метки, поля) уже существуют в памяти.
// Именно здесь безопасно настраивать внешний вид и начальные значения UI.
//
// Порядок запуска при старте программы:
//   1. Конструктор TForm1() — создаём объект, инициализируем переменные
//   2. VCL создаёт все компоненты из .dfm файла
//   3. FormCreate() — настраиваем UI (здесь мы сейчас)
//   4. Форма становится видимой пользователю
//---------------------------------------------------------------------------
void __fastcall TForm1::FormCreate(TObject *Sender)
{
    // --- Заголовок окна ---
    // Caption — это текст в верхней синей полоске окна
    Caption = "ESP32 CALC & PAINT v1.0";

    // --- Начальные тексты на метках (Labels) ---
    // Caption — свойство которое хранит текст метки
    LBL_CONNECTION_STATUS->Caption = "CONNECTION STATUS: DISCONNECTED";
    LBL_CALC_STATUS->Caption       = "INVOKE CALC.  PRESS D4.  STATUS:";
    LBL_PAINT_STATUS->Caption      = "INVOKE PAINT. PRESS D16. STATUS:";
    LBL_D4_PINNED_TO->Caption      = "D4 PINNED TO:";
    LBL_D16_PINNED_TO->Caption     = "D16 PINNED TO:";

    // --- Настройка таймера ---
    // Таймер ВЫКЛЮЧЕН при старте — включится только после подключения к ESP32.
    // Если включить сразу — он будет вызывать TimerReadComTimer каждые 50 мс
    // и пытаться читать порт который ещё не открыт.
    TimerReadCom->Enabled  = false;
    // Interval = 50 мс = 20 раз в секунду.
    // Это баланс: достаточно часто чтобы не пропустить нажатие кнопки,
    // не слишком часто чтобы не нагружать систему.
    TimerReadCom->Interval = 50;

    // --- Настройка диалога выбора файла ---
    // Filter определяет какие файлы видны в диалоге.
    // Формат: "Описание|*.расширение|Описание2|*.расширение2"
    // Первая часть до | — текст в выпадающем списке типов файлов
    // Вторая часть после | — маска (какие файлы показывать)
    OpenDialog1->Filter     = "Executable files (*.exe)|*.EXE|All files (*.*)|*.*";
    OpenDialog1->Title      = "Select program to pin";
    // InitialDir — папка которая откроется когда диалог появится.
    // System32 — там живут calc.exe, mspaint.exe и другие системные программы.
    OpenDialog1->InitialDir = "C:\\Windows\\System32\\";

    // --- Настройка полей отображения путей ---
    // ReadOnly = true — пользователь ВИДИТ путь но не может его редактировать.
    // Изменить путь можно только через кнопку PIN TO D4/D16.
    EDIT_D4->ReadOnly  = true;
    EDIT_D16->ReadOnly = true;
    // clBtnFace — серый цвет как у кнопок Windows.
    // Визуально показывает что поле недоступно для редактирования.
    EDIT_D4->Color  = clBtnFace;
    EDIT_D16->Color = clBtnFace;

    // --- Заполняем список COM-портов ---
    // Читаем реестр Windows и находим все доступные порты.
    // Делаем это АВТОМАТИЧЕСКИ — пользователю не нужно нажимать REFRESH.
    RefreshComPorts();

    // --- Загружаем сохранённые настройки ---
    // Читаем файл esp32_pins.ini если он уже существует.
    // Если нет — остаются значения по умолчанию из конструктора.
    LoadSettings();

    // --- Обновляем отображение путей в полях EDIT ---
    // После загрузки настроек показываем пути в интерфейсе.
    UpdateD4PathDisplay();
    UpdateD16PathDisplay();

    SB_MAIN_STATUS_BAR->SimpleText = "Ready";
}


// ════════════════════════════════════════════════════════════════════
//
//   БЛОК 2: COM-ПОРТ
//   Поиск портов, открытие соединения, чтение данных
//
// ════════════════════════════════════════════════════════════════════

//---------------------------------------------------------------------------
// RefreshComPorts — сканирует реестр Windows и заполняет список COM-портов.
//
// КАК WINDOWS ЗНАЕТ О COM-ПОРТАХ?
// При подключении USB-устройства (например ESP32) Windows устанавливает
// драйвер и регистрирует COM-порт в реестре по адресу:
// HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM
//
// Там хранятся записи вида:
//   \Device\VCP0 = COM13
//   \Device\Serial0 = COM1
// Мы читаем значения (правые части) и добавляем их в список.
//---------------------------------------------------------------------------
void TForm1::RefreshComPorts()
{
    // Очищаем список перед новым заполнением
    CMB_COM_PORT->Clear();

    // Создаём объект для работы с реестром.
    // new — выделяет память в куче (heap).
    // ПРАВИЛО: всё что создано через new — нужно удалить через delete!
    TRegistry *reg = new TRegistry();

    // RootKey — это "корневая папка" с которой начинается поиск.
    // Реестр Windows имеет несколько корневых веток:
    //   HKEY_LOCAL_MACHINE — настройки компьютера (для всех пользователей)
    //   HKEY_CURRENT_USER  — настройки текущего пользователя
    //   HKEY_CLASSES_ROOT  — ассоциации файлов (.pdf → Adobe, .docx → Word)
    // COM-порты хранятся в HKEY_LOCAL_MACHINE — это железо компьютера.
    reg->RootKey = HKEY_LOCAL_MACHINE;

    // Открываем нужную ветку реестра только для чтения (ReadOnly).
    // Двойной обратный слеш \\ — это экранирование: в C++ \ — спецсимвол,
    // поэтому реальный \ пишется как \\.
    // Реальный путь: HARDWARE\DEVICEMAP\SERIALCOMM
    if (reg->OpenKeyReadOnly("HARDWARE\\DEVICEMAP\\SERIALCOMM"))
    {
        // TStringList — динамический список строк из VCL.
        // Умеет добавлять, удалять, сортировать строки.
        TStringList *values = new TStringList();

        // GetValueNames — получает имена ВСЕХ записей в текущем ключе.
        // Заполняет наш список values именами вроде "\Device\VCP0".
        // Это имена записей, не их значения!
        reg->GetValueNames(values);

        // Перебираем все найденные имена записей
        // values->Count — количество элементов в списке
        // values->Strings[i] — i-й элемент (индексация с 0!)
        for (int i = 0; i < values->Count; i++)
        {
            // ReadString(имя) — читает ЗНАЧЕНИЕ записи по её имени.
            // Например: ReadString("\Device\VCP0") вернёт "COM13"
            String portName = reg->ReadString(values->Strings[i]);

            // Pos("COM") — ищет подстроку "COM" в строке.
            // Возвращает позицию первого вхождения (в C++ Builder — с 1, не с 0!).
            // == 1 означает что "COM" стоит в самом начале строки.
            // Это фильтр: пропускаем только строки начинающиеся с "COM".
            if (portName.Pos("COM") == 1)
            {
                // Add — добавляем имя порта в выпадающий список
                CMB_COM_PORT->Items->Add(portName);
            }
        }

        // Освобождаем память — удаляем то что создали через new
        delete values;
        // Закрываем ключ реестра (как закрыть папку после просмотра)
        reg->CloseKey();
    }

    // Удаляем объект реестра — освобождаем память
    delete reg;

    // Если ни одного COM-порта не нашли — показываем заглушку.
    // Items->Count — количество элементов в выпадающем списке.
    if (CMB_COM_PORT->Items->Count == 0)
    {
        CMB_COM_PORT->Items->Add("No COM ports found");
    }

    // ItemIndex — индекс выбранного элемента.
    // 0 = выбираем первый элемент в списке автоматически.
    CMB_COM_PORT->ItemIndex = 0;
}

//---------------------------------------------------------------------------
// CheckESP32 — проверяет что на указанном порту действительно наш ESP32.
//
// КАК ЭТО РАБОТАЕТ:
// 1. Отправляем команду "TEST\n"
// 2. Ждём 300 мс — ESP32 обрабатывает команду
// 3. Читаем ответ и ищем строку "ESP32_OK"
// 4. Если нашли — это наш ESP32!
//
// ПОЧЕМУ ТРИ ПОПЫТКИ?
// ESP32 при подключении перезагружается (сигнал DTR сбрасывает чип).
// Первые несколько секунд она загружает скетч и может не ответить сразу.
// Три попытки с паузами дают ей время "проснуться".
//
// Параметр: hPort — дескриптор открытого COM-порта
// Возвращает: true = ESP32 найдена, false = не найдена
//---------------------------------------------------------------------------
bool TForm1::CheckESP32(HANDLE hPort)
{
    // Защита: если дескриптор недействителен — сразу выходим
    if (hPort == INVALID_HANDLE_VALUE)
        return false;

    // Команда которую отправим.
    // \n в конце обязателен — ESP32 читает до символа новой строки.
    char testCmd[] = "TEST\n";

    // Переменная куда WriteFile запишет сколько байт реально отправлено.
    // Мы её не проверяем, но WriteFile требует её передать.
    DWORD bytesWritten;

    // Три попытки установить связь
    for (int attempt = 0; attempt < 3; attempt++)
    {
        // Очищаем буферы порта перед каждой попыткой.
        // В буфере могут лежать данные от предыдущей попытки или
        // сообщения которые ESP32 отправила до нас (ESP32_READY и т.д.).
        // PURGE_RXCLEAR — очистить буфер ПРИЁМА (то что пришло от ESP32)
        // PURGE_TXCLEAR — очистить буфер ПЕРЕДАЧИ (то что мы ещё не отправили)
        // | — побитовое ИЛИ: комбинируем два флага в одну операцию
        PurgeComm(hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);

        // Отправляем "TEST\n" в порт.
        // WriteFile — универсальная Windows функция записи в файл/устройство.
        // hPort        — куда писать (наш COM-порт)
        // testCmd      — что писать (указатель на данные)
        // strlen(...)  — сколько байт записать (длина строки без нулевого байта)
        // &bytesWritten — адрес куда записать результат
        // NULL          — для синхронного режима всегда NULL
        if (!WriteFile(hPort, testCmd, strlen(testCmd), &bytesWritten, NULL))
        {
            // ! означает НЕ: если WriteFile вернул false (ошибка)...
            Sleep(100);   // ...ждём 100 мс (Sleep — функция Windows, блокирует поток)
            continue;     // ...и переходим к следующей итерации цикла
        }

        // Ждём 300 мс пока ESP32 обработает команду и пришлёт ответ.
        // Sleep здесь ДОПУСТИМ — мы ещё не запустили таймер,
        // интерфейс не "заморожен" потому что пользователь ждёт подключения.
        Sleep(300);

        // Буфер для ответа от ESP32.
        // char buffer[256] = {0} — массив 256 байт, заполненный нулями.
        // {0} инициализирует ВСЕ элементы нулём — защита от мусора в памяти.
        char buffer[256] = {0};
        DWORD bytesRead;  // Сюда запишется сколько байт прочитали

        // Читаем ответ из порта.
        // sizeof(buffer)-1 = 255 — читаем максимум 255 байт,
        // оставляя место для завершающего нулевого байта.
        if (!ReadFile(hPort, buffer, sizeof(buffer)-1, &bytesRead, NULL))
        {
            Sleep(100);
            continue;
        }

        // Если ничего не прочитали — следующая попытка
        if (bytesRead == 0)
        {
            Sleep(100);
            continue;
        }

        // Ставим нулевой байт ('\0') после прочитанных данных.
        // В C++ строки заканчиваются нулём — это признак конца строки.
        // Без этого String(buffer) может прочитать лишние байты из памяти.
        buffer[bytesRead] = '\0';

        // Превращаем массив байт в строку и разбиваем по строкам.
        // ESP32 мог прислать несколько строк сразу:
        //   "ESP32_READY\nESP32_OK\n"
        // TStringList автоматически разобьёт это по символам \n.
        TStringList *lines = new TStringList();
        lines->Text = String(buffer);

        bool found = false;
        for (int i = 0; i < lines->Count; i++)
        {
            // Trim() убирает пробелы, \r, \n по краям строки.
            // \r — символ возврата каретки (Windows добавляет его перед \n).
            if (lines->Strings[i].Trim() == "ESP32_OK")
            {
                found = true;
                break;  // Нашли! Выходим из цикла.
            }
        }

        delete lines;  // Освобождаем память

        if (found)
            return true;  // ESP32 подтверждена — выходим из функции

        Sleep(100);  // Пауза перед следующей попыткой
    }

    // Три попытки исчерпаны, "ESP32_OK" так и не получили
    return false;
}

//---------------------------------------------------------------------------
// TimerReadComTimer — срабатывает каждые 50 мс пока подключены.
//
// ЭТО ГЛАВНЫЙ ЦИКЛ РАБОЧЕГО РЕЖИМА.
// Аналог loop() в Arduino — выполняется бесконечно пока программа работает.
//
// Что делает каждые 50 мс:
//   1. Проверяет есть ли данные в буфере COM-порта
//   2. Если есть — читает их и разбивает на строки
//   3. Каждую строку передаёт в ParseData для обработки
//   4. Проверяет не истёк ли таймаут ожидания ответа
//---------------------------------------------------------------------------
void __fastcall TForm1::TimerReadComTimer(TObject *Sender)
{
    // Если не подключены — выходим немедленно, делать нечего
    if (!connected || hCom == INVALID_HANDLE_VALUE)
        return;

    // Буфер для входящих данных.
    // 512 байт — с запасом для нескольких сообщений за раз.
    char  buffer[512];
    DWORD bytesRead;

    // Пытаемся прочитать данные из порта.
    // С БЫСТРЫМИ ТАЙМАУТАМИ (установленными в BTN_CONNECTClick) эта
    // функция возвращается МГНОВЕННО:
    //   - если данные есть — возвращает их
    //   - если данных нет — возвращает 0 прочитанных байт
    // Это ключевой момент: ReadFile НЕ БЛОКИРУЕТ интерфейс.
    if (ReadFile(hCom, buffer, sizeof(buffer)-1, &bytesRead, NULL))
    {
        // Если что-то пришло (bytesRead > 0)
        if (bytesRead > 0)
        {
            // Завершаем строку нулём
            buffer[bytesRead] = '\0';

            // Разбиваем полученный текст на отдельные строки.
            // За 50 мс могло прийти несколько сообщений подряд:
            // "BTN_CALC:1\nBTN_CALC:0\n"
            // TStringList разобьёт это на ["BTN_CALC:1", "BTN_CALC:0"]
            TStringList *lines = new TStringList();
            lines->Text = String(buffer);

            // Обрабатываем каждую строку отдельно
            for (int i = 0; i < lines->Count; i++)
            {
                // AnsiString — 8-битная строка Borland (в отличие от String/UnicodeString)
                // Trim() убирает пробелы и \r\n по краям
                AnsiString line = AnsiString(lines->Strings[i]).Trim();

                // Пустые строки игнорируем
                if (!line.IsEmpty())
                    ParseData(line);  // Разбираем каждое сообщение
            }

            // Освобождаем память — обязательно!
            delete lines;
        }
    }
    else
    {
        // ReadFile вернул ошибку — порт мог отключиться физически
        // (пользователь выдернул USB кабель)
        DWORD err = GetLastError();

        // ERROR_BROKEN_PIPE    — соединение разорвано
        // ERROR_INVALID_HANDLE — дескриптор недействителен
        if (err == ERROR_BROKEN_PIPE || err == ERROR_INVALID_HANDLE)
        {
            // Имитируем нажатие кнопки DISCONNECT — убираем за собой
            BTN_DISCONNECTClick(NULL);
            ShowMessage("ESP32 disconnected unexpectedly!\nCheck USB cable.");
        }
    }

    // --- Проверка таймаута команды ---
    // Если мы отправили команду и 5 секунд нет ответа — что-то пошло не так.
    // GetTickCount() — время в мс с момента старта Windows.
    // GetTickCount() - commandStartTime = сколько мс прошло с отправки.
    if (commandPending && (GetTickCount() - commandStartTime > 5000))
    {
        commandPending = false;  // Сбрасываем флаг ожидания
        SB_MAIN_STATUS_BAR->SimpleText = "Command timeout — no response from ESP32";
    }
}


// ════════════════════════════════════════════════════════════════════
//
//   БЛОК 3: ПРОТОКОЛ ESP32
//   Отправка команд и разбор входящих данных
//
// ════════════════════════════════════════════════════════════════════

//---------------------------------------------------------------------------
// SendCommand — отправляет текстовую команду в COM-порт.
//
// Перед отправкой проверяет:
//   1. Мы вообще подключены?
//   2. Не ждём ли мы ответа на предыдущую команду?
//
// Параметр: command — текст команды БЕЗ \n (функция добавит сама)
//---------------------------------------------------------------------------
void TForm1::SendCommand(AnsiString command)
{
    // Проверяем подключение
    if (!connected || hCom == INVALID_HANDLE_VALUE)
        return;

    // Защита от очереди команд: если предыдущая команда ещё не получила
    // ответ — не отправляем новую. ESP32 обрабатывает команды последовательно,
    // не нужно её заваливать.
    if (commandPending)
    {
        SB_MAIN_STATUS_BAR->SimpleText = "Busy — waiting for ESP32 response...";
        return;
    }

    // Добавляем \n в конец — ESP32 читает до символа новой строки
    AnsiString cmd = command + "\n";
    DWORD bytesWritten;

    // Отправляем команду.
    // cmd.c_str() — конвертируем AnsiString в обычную C-строку (char*)
    // cmd.Length() — длина строки в байтах
    if (WriteFile(hCom, cmd.c_str(), cmd.Length(), &bytesWritten, NULL))
    {
        // Успешно отправлено — запоминаем что ждём ответ
        commandPending   = true;
        lastCommand      = command;
        // GetTickCount() — текущее время, запомним для проверки таймаута
        commandStartTime = GetTickCount();
    }
}

//---------------------------------------------------------------------------
// ParseData — разбирает ОДНУ строку пришедшую от ESP32.
//
// Вызывается из TimerReadComTimer для каждой строки отдельно.
// Каждое сообщение от ESP32 обрабатывается здесь и превращается
// в конкретное действие (обновление UI, запуск программы и т.д.)
//
// Параметр: data — одна строка от ESP32 (уже без \n)
//---------------------------------------------------------------------------
void TForm1::ParseData(AnsiString data)
{
    // Убираем пробелы, \r, \n по краям на всякий случай
    data = data.Trim();

    // Пустая строка — ничего делать не нужно
    if (data.IsEmpty()) return;

    // --- Служебные сообщения ESP32 ---
    // Эти строки ESP32 отправляет сама при старте или в ответ на TEST.
    // До TimerReadComTimer они обычно не доходят (перехватываются в CheckESP32),
    // но обрабатываем на всякий случай.
    if (data == "ESP32_OK" || data == "ESP32_READY" || data == "CONNECTED_OK")
    {
        SB_MAIN_STATUS_BAR->SimpleText = "ESP32: " + data;
        return;
    }

    // --- Кнопка D4 (CALC) нажата ---
    // ESP32 отправляет "BTN_CALC:1" когда кнопка D4 переходит в LOW
    if (data == "BTN_CALC:1")
    {
        LBL_CALC_STATUS->Caption = "INVOKE CALC.  PRESS D4.  STATUS: PRESSED";
        SB_MAIN_STATUS_BAR->SimpleText = "D4 pressed — launching: " +
                                          ExtractFileName(D4_ProgramPath);
        ExecuteD4Program();   // Запускаем назначенную программу
        return;
    }

    // --- Кнопка D4 (CALC) отпущена ---
    if (data == "BTN_CALC:0")
    {
        LBL_CALC_STATUS->Caption = "INVOKE CALC.  PRESS D4.  STATUS: released";
        SB_MAIN_STATUS_BAR->SimpleText = "D4 released";
        return;
    }

    // --- Кнопка D16 (PAINT) нажата ---
    if (data == "BTN_PAINT:1")
    {
        LBL_PAINT_STATUS->Caption = "INVOKE PAINT. PRESS D16. STATUS: PRESSED";
        SB_MAIN_STATUS_BAR->SimpleText = "D16 pressed — launching: " +
                                          ExtractFileName(D16_ProgramPath);
        ExecuteD16Program();  // Запускаем назначенную программу
        return;
    }

    // --- Кнопка D16 (PAINT) отпущена ---
    if (data == "BTN_PAINT:0")
    {
        LBL_PAINT_STATUS->Caption = "INVOKE PAINT. PRESS D16. STATUS: released";
        SB_MAIN_STATUS_BAR->SimpleText = "D16 released";
        return;
    }

    // --- Всё остальное ---
    // Неизвестные сообщения просто показываем в статусной строке.
    // Это полезно при отладке — видно что приходит от ESP32.
    SB_MAIN_STATUS_BAR->SimpleText = "ESP32: " + data;
}

//---------------------------------------------------------------------------
// SetConnectedState — единая точка обновления UI при изменении подключения.
//
// Вместо того чтобы разбрасывать BTN_CONNECT->Enabled = false по всему коду,
// всё собрано здесь. Вызывается из BTN_CONNECTClick и BTN_DISCONNECTClick.
//
// Параметры:
//   state — true = подключились, false = отключились
//   port  — имя порта ("COM13") или "" при отключении
//---------------------------------------------------------------------------
void TForm1::SetConnectedState(bool state, AnsiString port)
{
    connected = state;

    if (state)
    {
        // ===== ПОДКЛЮЧИЛИСЬ =====
        LBL_CONNECTION_STATUS->Caption =
            "CONNECTION STATUS: CONNECTED to " + port;
        LBL_CONNECTION_STATUS->Font->Color = clGreen;

        BTN_CONNECT->Enabled    = false;  // Нельзя нажать CONNECT повторно
        BTN_DISCONNECT->Enabled = true;   // Теперь можно отключиться
        CMB_COM_PORT->Enabled   = false;  // Нельзя менять порт во время работы
        TimerReadCom->Enabled   = true;   // Запускаем чтение данных от ESP32

        // Уведомляем ESP32 что Windows готова к работе.
        // Небольшая пауза (100 мс) нужна чтобы ESP32 успела выйти из
        // режима обработки команды TEST и перейти в нормальный loop().
        Sleep(100);
        char cmd[] = "CONNECTED\n";
        DWORD bytesWritten;
        WriteFile(hCom, cmd, strlen(cmd), &bytesWritten, NULL);
        // ESP32 ответит "CONNECTED_OK" и обновит свой дисплей
    }
    else
    {
        // ===== ОТКЛЮЧИЛИСЬ =====
        LBL_CONNECTION_STATUS->Caption = "CONNECTION STATUS: DISCONNECTED";
        LBL_CONNECTION_STATUS->Font->Color = clRed;

        BTN_CONNECT->Enabled    = true;   // Можно снова подключиться
        BTN_DISCONNECT->Enabled = false;  // Нечего отключать
        CMB_COM_PORT->Enabled   = true;   // Можно выбрать другой порт
        TimerReadCom->Enabled   = false;  // Останавливаем чтение порта

        // Сбрасываем статусы кнопок — ESP32 больше не отправляет данные
        LBL_CALC_STATUS->Caption  = "INVOKE CALC.  PRESS D4.  STATUS:";
        LBL_PAINT_STATUS->Caption = "INVOKE PAINT. PRESS D16. STATUS:";

        // Сбрасываем флаг ожидания — команда уже не дождётся ответа
        commandPending = false;
        lastCommand    = "";
    }
}


// ════════════════════════════════════════════════════════════════════
//
//   БЛОК 4: НАСТРОЙКИ INI
//   Сохранение и загрузка путей к программам
//
// ════════════════════════════════════════════════════════════════════

//---------------------------------------------------------------------------
// LoadSettings — читает пути к программам из INI файла.
//
// ЧТО ТАКОЕ INI ФАЙЛ?
// Простой текстовый файл с настройками. Выглядит так:
//   [Buttons]
//   D4=C:\Windows\System32\calc.exe
//   D16=C:\Windows\System32\mspaint.exe
//
// [Buttons] — секция (раздел)
// D4        — ключ
// C:\...    — значение
//
// Файл лежит рядом с .exe программы: esp32_pins.ini
// Пользователь может открыть его в блокноте и посмотреть/изменить вручную.
//---------------------------------------------------------------------------
void TForm1::LoadSettings()
{
    // Путь к INI файлу — в той же папке где лежит наш .exe
    // ExtractFilePath("C:\prog\app.exe") вернёт "C:\prog\"
    // Application->ExeName — полный путь к нашему .exe файлу
    String iniPath = ExtractFilePath(Application->ExeName) + "esp32_pins.ini";

    // FileExists — проверяет существует ли файл.
    // Если файла нет (первый запуск) — используем значения из конструктора.
    if (FileExists(iniPath))
    {
        // Создаём объект для работы с INI файлом
        TIniFile *ini = new TIniFile(iniPath);

        // ReadString(секция, ключ, значение_по_умолчанию)
        // Читает значение из файла. Если ключ не найден — возвращает третий аргумент.
        D4_ProgramPath  = ini->ReadString("Buttons", "D4",  "calc.exe");
        D16_ProgramPath = ini->ReadString("Buttons", "D16", "mspaint.exe");

        delete ini;  // Закрываем файл и освобождаем память

        SB_MAIN_STATUS_BAR->SimpleText = "Settings loaded from: " + iniPath;
    }
    else
    {
        // Первый запуск — INI файла нет, значения из конструктора
        SB_MAIN_STATUS_BAR->SimpleText = "First run — using default program paths";
    }
}

//---------------------------------------------------------------------------
// SaveSettings — записывает пути к программам в INI файл.
//
// Вызывается в трёх случаях:
//   1. Пользователь выбрал программу через BTN_PIN_TO_D4Click
//   2. Пользователь выбрал программу через BTN_PIN_TO_D16Click
//   3. Пользователь закрывает программу (FormClose)
//---------------------------------------------------------------------------
void TForm1::SaveSettings()
{
    String iniPath = ExtractFilePath(Application->ExeName) + "esp32_pins.ini";

    // TIniFile создаёт файл если он не существует,
    // или открывает существующий для изменения.
    TIniFile *ini = new TIniFile(iniPath);

    // WriteString(секция, ключ, значение) — записывает строку в INI файл
    ini->WriteString("Buttons", "D4",  D4_ProgramPath);
    ini->WriteString("Buttons", "D16", D16_ProgramPath);

    // ВАЖНО: в C++ Builder TIniFile записывает данные в файл
    // только при вызове delete (или UpdateFile).
    // Без delete данные могут остаться только в памяти!
    delete ini;
}


// ════════════════════════════════════════════════════════════════════
//
//   БЛОК 5: ЗАПУСК ПРОГРАММ
//   Выполнение .exe файлов по нажатию кнопок ESP32
//
// ════════════════════════════════════════════════════════════════════

//---------------------------------------------------------------------------
// ExecuteD4Program — запускает программу назначенную на кнопку D4.
//
// КАК WINDOWS ЗАПУСКАЕТ ПРОГРАММЫ?
// Есть несколько способов:
//   WinExec("calc", SW_SHOW)  — старый, простой, работает с именем из PATH
//   ShellExecute(...)         — современный, понимает ассоциации файлов,
//                               может запросить права администратора
//
// Мы используем WinExec как основной способ.
// Если он не справляется (возвращает <= 31) — пробуем ShellExecute как запасной.
//---------------------------------------------------------------------------
void TForm1::ExecuteD4Program()
{
    // Если путь пустой — программа не назначена
    if (D4_ProgramPath.IsEmpty())
    {
        SB_MAIN_STATUS_BAR->SimpleText = "No program assigned to D4!";
        return;
    }

    // Оборачиваем путь в кавычки для корректной обработки пробелов в пути.
    // Например: "C:\Program Files\app.exe"
    // Без кавычек WinExec воспримет "Program" как отдельную команду.
    String cmd = "\"" + D4_ProgramPath + "\"";

    // WinExec — запускаем программу.
    // cmd.c_str() — путь к программе как C-строка
    // SW_SHOW — показать окно программы нормально (не свёрнутым, не на весь экран)
    // Возвращает > 31 если успешно, <= 31 если ошибка
    UINT result = WinExec(cmd.c_str(), SW_SHOW);

    if (result <= 31)
    {
        // WinExec не справился — пробуем ShellExecute.
        // ShellExecute умеет: запускать программы, открывать документы,
        // открывать URL-адреса — более умный запуск через Windows Shell.
        // NULL          — дескриптор родительского окна (NULL = без родителя)
        // "open"        — операция (open = запустить/открыть)
        // D4_ProgramPath.c_str() — путь к файлу
        // NULL          — параметры командной строки (нет)
        // NULL          — рабочая директория (текущая)
        // SW_SHOW       — как показать окно
        ShellExecute(NULL, "open", D4_ProgramPath.c_str(), NULL, NULL, SW_SHOW);
    }

    SB_MAIN_STATUS_BAR->SimpleText = "Launched: " + ExtractFileName(D4_ProgramPath);
}

//---------------------------------------------------------------------------
// ExecuteD16Program — запускает программу назначенную на кнопку D16.
// Логика полностью аналогична ExecuteD4Program.
//---------------------------------------------------------------------------
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
    {
        ShellExecute(NULL, "open", D16_ProgramPath.c_str(), NULL, NULL, SW_SHOW);
    }

    SB_MAIN_STATUS_BAR->SimpleText = "Launched: " + ExtractFileName(D16_ProgramPath);
}


// ════════════════════════════════════════════════════════════════════
//
//   БЛОК 6: ОБНОВЛЕНИЕ UI
//   Синхронизация интерфейса с переменными состояния
//
// ════════════════════════════════════════════════════════════════════

//---------------------------------------------------------------------------
// UpdateD4PathDisplay — обновляет поле EDIT_D4 и метку LBL_D4_PINNED_TO.
//
// Вызывается каждый раз когда меняется D4_ProgramPath:
//   - при загрузке настроек (LoadSettings)
//   - когда пользователь выбирает новую программу (BTN_PIN_TO_D4Click)
//---------------------------------------------------------------------------
void TForm1::UpdateD4PathDisplay()
{
    // Показываем полный путь в поле TEdit
    EDIT_D4->Text = D4_ProgramPath;

    // В метке показываем только имя файла (без полного пути) — экономим место.
    // ExtractFileName("C:\Windows\System32\calc.exe") вернёт "calc.exe"
    String fileName = ExtractFileName(D4_ProgramPath);
    LBL_D4_PINNED_TO->Caption = "D4 PINNED TO: " + fileName;
}

//---------------------------------------------------------------------------
// UpdateD16PathDisplay — аналогично UpdateD4PathDisplay но для D16.
//---------------------------------------------------------------------------
void TForm1::UpdateD16PathDisplay()
{
    EDIT_D16->Text = D16_ProgramPath;

    String fileName = ExtractFileName(D16_ProgramPath);
    LBL_D16_PINNED_TO->Caption = "D16 PINNED TO: " + fileName;
}


// ════════════════════════════════════════════════════════════════════
//
//   БЛОК 7: ОБРАБОТЧИКИ КНОПОК И СОБЫТИЙ ФОРМЫ
//   Реакция на действия пользователя
//
// ════════════════════════════════════════════════════════════════════

//---------------------------------------------------------------------------
// BTN_CONNECTClick — нажата кнопка CONNECT.
//
// ПОСЛЕДОВАТЕЛЬНОСТЬ ПОДКЛЮЧЕНИЯ:
//   1. Проверяем что ещё не подключены
//   2. Получаем имя порта из списка
//   3. Формируем правильное имя для Windows API
//   4. Открываем порт через CreateFile
//   5. Настраиваем параметры (скорость, биты данных)
//   6. Ставим медленные таймауты (для проверки ESP32)
//   7. Вызываем CheckESP32 — убеждаемся что это наш ESP32
//   8. Ставим быстрые таймауты (для рабочего режима)
//   9. Вызываем SetConnectedState — обновляем UI и запускаем таймер
//---------------------------------------------------------------------------
void __fastcall TForm1::BTN_CONNECTClick(TObject *Sender)
{
    // Защита от повторного подключения
    if (connected)
    {
        ShowMessage("Already connected!");
        return;
    }

    // --- Шаг 1: Получаем выбранный порт ---
    // ItemIndex == -1 означает "ничего не выбрано"
    if (CMB_COM_PORT->ItemIndex < 0 ||
        CMB_COM_PORT->Items->Strings[CMB_COM_PORT->ItemIndex] == "No COM ports found")
    {
        ShowMessage("Please select a COM port.");
        return;
    }

    String portName = CMB_COM_PORT->Items->Strings[CMB_COM_PORT->ItemIndex];

    // --- Шаг 2: Формируем имя порта для Windows API ---
    // ВАЖНАЯ ОСОБЕННОСТЬ WINDOWS:
    // COM1...COM9   — открываются просто по имени: "COM3"
    // COM10 и выше — ТРЕБУЮТ специального префикса: "\\.\\COM13"
    // Без этого CreateFile вернёт ERROR_FILE_NOT_FOUND для COM10+
    String comPort;
    // SubString(4, Length-3) вырезает число из "COM13" → "13"
    // В C++ Builder SubString(начало, длина), индексация с 1!
    String numStr  = portName.SubString(4, portName.Length() - 3);
    int    portNum = StrToIntDef(numStr, 0);  // "13" → 13, при ошибке → 0

    if (portNum > 9)
        comPort = "\\\\.\\"+portName;  // "\\.\\COM13" — для COM10 и выше
    else
        comPort = portName;            // "COM3" — для COM1-COM9

    // --- Шаг 3: Открываем COM-порт ---
    // CreateFile — несмотря на название, открывает не только файлы!
    // В Windows ВСЁ является файлом: диски, порты, принтеры.
    // Поэтому COM-порт открывается той же функцией что и обычный файл.
    hCom = CreateFile(
        comPort.c_str(),               // Имя порта как C-строка
        GENERIC_READ | GENERIC_WRITE,  // Разрешаем и чтение и запись
        0,                             // 0 = эксклюзивный доступ: никто другой
                                       // не сможет открыть этот порт
        NULL,                          // Атрибуты безопасности (по умолчанию)
        OPEN_EXISTING,                 // Открыть существующий порт
                                       // (CREATE_NEW — создать новый, нам не нужно)
        FILE_ATTRIBUTE_NORMAL,         // Обычные атрибуты файла
        NULL                           // Шаблон (не используется для портов)
    );

    // Проверяем удалось ли открыть порт
    if (hCom == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();  // Узнаём код ошибки Windows
        String msg = "Cannot open " + portName + ".\n";

        // Переводим код ошибки в понятный текст
        if      (err == ERROR_ACCESS_DENIED)   msg += "Port is busy. Close Serial Monitor.";
        else if (err == ERROR_FILE_NOT_FOUND)  msg += "Port not found.";
        else                                   msg += "Error code: " + IntToStr((int)err);

        ShowMessage(msg);
        return;
    }

    // --- Шаг 4: Настраиваем параметры порта ---
    // DCB = Device Control Block — структура Windows с настройками порта.
    // = {0} инициализирует все поля нулями.
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);  // Обязательное требование Windows API:
                                   // нужно указать размер структуры

    // Сначала читаем ТЕКУЩИЕ настройки порта.
    // Зачем? В DCB много полей которые мы не трогаем — они должны
    // остаться с системными значениями. Меняем только 4 нужных нам.
    if (!GetCommState(hCom, &dcb))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error reading port settings.");
        return;
    }

    // Устанавливаем стандарт 115200/8N1:
    // 115200 бод — скорость (должна совпадать с Serial.begin(115200) в ESP32)
    // 8         — 8 бит данных в каждом байте
    // N         — No parity (без проверки чётности)
    // 1         — 1 стоп-бит (маркер конца байта)
    dcb.BaudRate = CBR_115200;   // CBR_115200 — константа Windows = 115200
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;   // ONESTOPBIT — константа = 1 стоп-бит
    dcb.Parity   = NOPARITY;     // NOPARITY — константа = без чётности

    if (!SetCommState(hCom, &dcb))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error applying port settings.");
        return;
    }

    // --- Шаг 5: Медленные таймауты для проверки ESP32 ---
    // COMMTIMEOUTS — структура Windows с таймаутами операций чтения/записи.
    // На этом этапе ReadFile должен ЖДАТЬ ответа от ESP32.
    COMMTIMEOUTS checkTO;
    checkTO.ReadIntervalTimeout         = 50;   // Ждать 50 мс между байтами
    checkTO.ReadTotalTimeoutMultiplier  = 10;   // +10 мс на каждый байт
    checkTO.ReadTotalTimeoutConstant    = 50;   // +50 мс базовое время
    checkTO.WriteTotalTimeoutMultiplier = 10;
    checkTO.WriteTotalTimeoutConstant   = 50;

    if (!SetCommTimeouts(hCom, &checkTO))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error setting timeouts.");
        return;
    }

    PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);  // Очищаем буферы

    // --- Шаг 6: Проверяем что это ESP32 ---
    SB_MAIN_STATUS_BAR->SimpleText = "Checking ESP32...";
    // ProcessMessages — даём Windows обработать события и обновить UI
    // (без этого надпись "Checking..." не появится на экране)
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

    // --- Шаг 7: Быстрые таймауты для рабочего режима ---
    // Теперь ReadFile должен возвращаться МГНОВЕННО.
    //
    // МАГИЧЕСКАЯ КОМБИНАЦИЯ для мгновенного возврата:
    //   ReadIntervalTimeout = 0xFFFFFFFF (MAXDWORD)
    //   ReadTotalTimeoutMultiplier = 0
    //   ReadTotalTimeoutConstant = 0
    //
    // Это специальное значение описанное в документации MSDN:
    // "если в буфере есть данные — вернуть немедленно,
    //  если буфер пуст — тоже вернуть немедленно с 0 байт"
    //
    // БЕЗ ЭТОГО: таймер вызывает ReadFile → ReadFile ждёт данных → UI замирает!
    // С ЭТИМ:   таймер вызывает ReadFile → ReadFile сразу возвращается → UI не блокируется
    COMMTIMEOUTS workTO;
    workTO.ReadIntervalTimeout         = 0xFFFFFFFF;  // MAXDWORD
    workTO.ReadTotalTimeoutMultiplier  = 0;
    workTO.ReadTotalTimeoutConstant    = 0;
    workTO.WriteTotalTimeoutMultiplier = 10;
    workTO.WriteTotalTimeoutConstant   = 50;

    SetCommTimeouts(hCom, &workTO);
    PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);

    // --- Шаг 8: Всё готово! ---
    SetConnectedState(true, portName);
    SB_MAIN_STATUS_BAR->SimpleText = "Connected to ESP32 on " + portName;
}

//---------------------------------------------------------------------------
// BTN_DISCONNECTClick — нажата кнопка DISCONNECT.
//
// ПОСЛЕДОВАТЕЛЬНОСТЬ ОТКЛЮЧЕНИЯ:
//   1. Останавливаем таймер (прекращаем читать порт)
//   2. Отправляем "DISCONNECTED" ESP32 (она обновит свой дисплей)
//   3. Закрываем дескриптор порта
//   4. Обновляем UI через SetConnectedState
//---------------------------------------------------------------------------
void __fastcall TForm1::BTN_DISCONNECTClick(TObject *Sender)
{
    if (!connected)
    {
        ShowMessage("Not connected!");
        return;
    }

    // Сначала останавливаем таймер — он больше не должен читать порт
    TimerReadCom->Enabled = false;

    if (hCom != INVALID_HANDLE_VALUE)
    {
        // Отправляем DISCONNECTED напрямую через WriteFile (не через SendCommand).
        // Почему не SendCommand? Потому что:
        //   1. При отключении нам не нужен ответ от ESP32
        //   2. commandPending мог быть true — SendCommand отказала бы
        //   3. Нам нужно отправить и сразу закрыть порт
        char cmd[] = "DISCONNECTED\n";
        DWORD bytesWritten;
        WriteFile(hCom, cmd, strlen(cmd), &bytesWritten, NULL);
        // ESP32 получит команду и обновит свой дисплей (Windows: offline)

        // Закрываем дескриптор порта — освобождаем ресурс Windows
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;  // Помечаем как закрытый
    }

    // Обновляем UI: кнопки, метки, флаги
    SetConnectedState(false, "");
    SB_MAIN_STATUS_BAR->SimpleText = "Disconnected";
}

//---------------------------------------------------------------------------
// BTN_PIN_TO_D4Click — нажата кнопка PIN TO D4.
//
// Открывает стандартный диалог Windows "Открыть файл".
// Пользователь выбирает .exe программу.
// Путь сохраняется в D4_ProgramPath и в INI файл.
//---------------------------------------------------------------------------
void __fastcall TForm1::BTN_PIN_TO_D4Click(TObject *Sender)
{
    // Устанавливаем заголовок диалога — пользователь видит подсказку
    OpenDialog1->Title    = "Select program for D4 button";
    // Очищаем поле имени файла — диалог откроется без предвыбранного файла
    OpenDialog1->FileName = "";

    // Execute() — открывает диалог. Возвращает true если пользователь
    // нажал "Открыть", false если "Отмена".
    if (OpenDialog1->Execute())
    {
        // FileName — полный путь к выбранному файлу
        // Например: "C:\Windows\System32\notepad.exe"
        D4_ProgramPath = OpenDialog1->FileName;

        // Обновляем EDIT_D4 и LBL_D4_PINNED_TO
        UpdateD4PathDisplay();

        // Сразу сохраняем в INI файл — не хотим потерять выбор
        SaveSettings();

        // ExtractFileName убирает путь, оставляет только имя файла
        SB_MAIN_STATUS_BAR->SimpleText = "D4 pinned to: " +
                                          ExtractFileName(D4_ProgramPath);
    }
    // Если пользователь нажал "Отмена" — ничего не меняем
}

//---------------------------------------------------------------------------
// BTN_PIN_TO_D16Click — аналогично BTN_PIN_TO_D4Click но для D16.
//---------------------------------------------------------------------------
void __fastcall TForm1::BTN_PIN_TO_D16Click(TObject *Sender)
{
    OpenDialog1->Title    = "Select program for D16 button";
    OpenDialog1->FileName = "";

    if (OpenDialog1->Execute())
    {
        D16_ProgramPath = OpenDialog1->FileName;
        UpdateD16PathDisplay();
        SaveSettings();
        SB_MAIN_STATUS_BAR->SimpleText = "D16 pinned to: " +
                                          ExtractFileName(D16_ProgramPath);
    }
}

//---------------------------------------------------------------------------
// FormClose — вызывается когда пользователь закрывает окно.
// (нажимает крестик, Alt+F4 или через меню)
//
// TCloseAction &Action — параметр со ссылкой (&).
// Через него можно ОТМЕНИТЬ закрытие: Action = caNone.
// Мы не отменяем — просто сохраняем настройки и позволяем закрыться.
//---------------------------------------------------------------------------
void __fastcall TForm1::FormClose(TObject *Sender, TCloseAction &Action)
{
    // Сохраняем настройки перед выходом — на случай если пользователь
    // закрыл программу не через BTN_PIN (тогда SaveSettings не вызывался)
    SaveSettings();

    // Action остаётся caFree (по умолчанию) — форма закроется и память освободится
}
