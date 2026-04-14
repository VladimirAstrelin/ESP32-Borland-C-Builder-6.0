//---------------------------------------------------------------------------
// ESP32_CALC_PAINT — Windows программа для управления ESP32
// Часть 1: подключение к ESP32 через COM-порт
//---------------------------------------------------------------------------

#include <vcl.h>
#include <windows.h>      // Windows API: CreateFile, ReadFile, WriteFile...
#include <registry.hpp>   // Работа с реестром Windows (список COM-портов)

#pragma hdrstop

#include "Unit1.h"

#pragma package(smart_init)
#pragma resource "*.dfm"

TForm1 *Form1;

//---------------------------------------------------------------------------
// КОНСТРУКТОР — выполняется при создании объекта формы.
// Инициализируем только логические переменные.
// Всё что касается UI — в FormCreate.
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
    : TForm(Owner)
{
    // Порт ещё не открыт — ставим специальное значение "нет дескриптора"
    hCom = INVALID_HANDLE_VALUE;

    // Ещё не подключены
    connected = false;

    // Никакая команда не ожидает ответа
    commandPending   = false;
    lastCommand      = "";
    commandStartTime = 0;
}

//---------------------------------------------------------------------------
// FormCreate — вызывается автоматически когда форма полностью создана.
// Здесь настраиваем UI и заполняем список COM-портов.
// FormCreate безопаснее конструктора для работы с компонентами —
// к этому моменту все кнопки, метки и списки уже существуют в памяти.
//---------------------------------------------------------------------------
void __fastcall TForm1::FormCreate(TObject *Sender)
{
    // Заголовок окна
    Caption = "ESP32 CALC & PAINT v1.0";

    // Начальные надписи
    LBL_CONNECTION_STATUS->Caption = "CONNECTION STATUS: DISCONNECTED";
    LBL_CALC_STATUS->Caption       = "INVOKE CALC.  PRESS D4.  STATUS:";
    LBL_PAINT_STATUS->Caption      = "INVOKE PAINT. PRESS D16. STATUS:";
    LBL_D4_PINNED_TO->Caption      = "D4 PINNED TO:";
    LBL_D16_PINNED_TO->Caption     = "D16 PINNED TO:";

    // Таймер пока выключен — включится только после подключения
    TimerReadCom->Enabled  = false;
    TimerReadCom->Interval = 50;  // Срабатывает каждые 50 мс

    // Заполняем список COM-портов автоматически при старте.
    // Пользователю не нужно нажимать REFRESH — список уже готов.
    RefreshComPorts();

    SB_MAIN_STATUS_BAR->SimpleText = "Ready";
}

//---------------------------------------------------------------------------
// RefreshComPorts — читает реестр Windows и заполняет выпадающий список
// актуальными COM-портами.
//
// COM-порты хранятся в реестре по адресу:
// HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM
// Там Windows регистрирует каждый подключённый COM-порт или USB-адаптер.
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
        //ShowMessage("DEBUG: BTN_CALC:1 received!");  // временно

        LBL_CALC_STATUS->Caption = "INVOKE CALC.  PRESS D4.  STATUS: PRESSED";
        SB_MAIN_STATUS_BAR->SimpleText = "D4 pressed — launching Calculator";

        // WinExec — простейший способ запустить программу в Windows.
        // "calc" — системная команда, Windows сама найдёт калькулятор.
        // SW_SHOW — показать окно программы нормально (не свёрнутым).
        // Возвращает > 31 если успешно, <= 31 если ошибка.
        if (WinExec("calc", SW_SHOW) <= 31)
                SB_MAIN_STATUS_BAR->SimpleText = "Error launching Calculator!";
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
        SB_MAIN_STATUS_BAR->SimpleText = "D16 pressed — launching MS Paint";

        // "mspaint" — системная команда для MS Paint
        if (WinExec("mspaint", SW_SHOW) <= 31)
                SB_MAIN_STATUS_BAR->SimpleText = "Error launching MS Paint!";
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
    // TODO (Часть 3): открыть диалог выбора программы для D4
    SB_MAIN_STATUS_BAR->SimpleText = "PIN TO D4 — coming in Part 3";
}

void __fastcall TForm1::BTN_PIN_TO_D16Click(TObject *Sender)
{
    // TODO (Часть 3): открыть диалог выбора программы для D16
    SB_MAIN_STATUS_BAR->SimpleText = "PIN TO D16 — coming in Part 3";
}
//---------------------------------------------------------------------------

