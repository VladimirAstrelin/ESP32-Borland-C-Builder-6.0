//---------------------------------------------------------------------------
// ESP32_CALC_PAINT Ч Windows программа дл¤ управлени¤ ESP32
// „асть 1: подключение к ESP32 через COM-порт
//---------------------------------------------------------------------------

// ѕодключаем заголовочные файлы с описани¤ми всего что используем
#include <vcl.h>          // Visual Component Library Ч главна¤ библиотека C++ Builder
#include <windows.h>      // Windows API Ч функции дл¤ работы с COM-портом, файлами и т.д.
#include <registry.hpp>   // –абота с реестром Windows (там хранитс¤ список COM-портов)
#include <IniFiles.hpp>   // TIniFile Ч работа с INI-файлами дл¤ сохранени¤ настроек
#include <shellapi.h>     // ShellExecute Ч более современный способ запуска программ

#pragma hdrstop  // ѕрекомпилированные заголовки (ускор¤ет сборку в C++ Builder)

#include "Unit1.h"  // ѕодключаем описание нашей формы

#pragma package(smart_init)  // ”мна¤ инициализаци¤ компонентов (особенность Builder)
#pragma resource "*.dfm"     // ѕодключаем ресурсы формы (дизайн из визуального редактора)

// —оздаЄм глобальную переменную Form1 Ч наше главное окно
TForm1 *Form1;

//---------------------------------------------------------------------------
//  ќЌ—“–” “ќ– Ч выполн¤етс¤ ѕ≈–¬џћ при создании объекта формы.
// Ёто специальна¤ функци¤ котора¤ вызываетс¤ когда программа только стартует.
// 
// __fastcall Ч способ вызова функции (быстрый, через регистры процессора)
// TComponent* Owner Ч "владелец" формы, обычно само приложение (Application)
// : TForm(Owner) Ч вызываем конструктор родительского класса TForm
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
    : TForm(Owner)  // —начала даЄм родительскому классу всЄ настроить
{
    // ѕорт ещЄ не открыт Ч ставим специальное значение "нет дескриптора"
    // INVALID_HANDLE_VALUE Ч это как "-1" дл¤ номерков HANDLE,
    // означает "невалидный, нерабочий, порт закрыт"
    hCom = INVALID_HANDLE_VALUE;

    // ≈щЄ не подключены
    connected = false;

    // Ќикака¤ команда не ожидает ответа (мы ничего не отправл¤ли)
    commandPending   = false;
    lastCommand      = "";
    commandStartTime = 0;

    // »нициализируем пути к программам —“јЌƒј–“Ќџћ» значени¤ми
    // ≈сли пользователь ещЄ ничего не выбирал Ч будут запускатьс¤ эти
    D4_ProgramPath  = "calc.exe";
    D16_ProgramPath = "mspaint.exe";
}

//---------------------------------------------------------------------------
// FormCreate Ч вызываетс¤ автоматически когда форма ѕќЋЌќ—“№ё создана.
// 
// ќтличие от конструктора:
// - ¬ конструкторе компоненты формы (кнопки, метки) ещЄ Ќ≈ готовы
// - ¬ FormCreate всЄ уже создано и можно настраивать UI
// 
// ѕоэтому настройки интерфейса делаем именно здесь, а не в конструкторе.
//---------------------------------------------------------------------------
void __fastcall TForm1::FormCreate(TObject *Sender)
{
    // ========== Ќј—“–ќ… ј «ј√ќЋќ¬ ј ќ Ќј ==========
    // Caption Ч это текст в верхней полоске окна
    Caption = "ESP32 CALC & PAINT v1.0";

    // ========== Ќј„јЋ№Ќџ≈ Ќјƒѕ»—» Ќј ћ≈“ ј’ ==========
    // —трелочка -> означает "поле объекта", то есть мы обращаемс¤ к свойству Caption
    LBL_CONNECTION_STATUS->Caption = "CONNECTION STATUS: DISCONNECTED";
    LBL_CALC_STATUS->Caption       = "INVOKE CALC.  PRESS D4.  STATUS:";
    LBL_PAINT_STATUS->Caption      = "INVOKE PAINT. PRESS D16. STATUS:";
    LBL_D4_PINNED_TO->Caption      = "D4 PINNED TO:";
    LBL_D16_PINNED_TO->Caption     = "D16 PINNED TO:";

    // ========== Ќј—“–ќ… ј “ј…ћ≈–ј ==========
    // “аймер пока выключен Ч включитс¤ только после успешного подключени¤
    TimerReadCom->Enabled  = false;
    // Interval = 50 означает "срабатывать каждые 50 миллисекунд"
    // 50 мс = 20 раз в секунду Ч достаточно быстро чтобы не пропустить данные
    TimerReadCom->Interval = 50;  // —рабатывает каждые 50 мс

    // ========== «јѕќЋЌя≈ћ —ѕ»—ќ  COM-ѕќ–“ќ¬ ==========
    // ¬ызываем функцию котора¤ читает реестр Windows и находит все COM-порты
    RefreshComPorts();

    SB_MAIN_STATUS_BAR->SimpleText = "Ready";

    // ========== Ќј—“–ќ… ј ƒ»јЋќ√ј ¬џЅќ–ј ‘ј…Ћј ==========
    // Filter Ч какие файлы показывать в диалоге
    // "Executable files (*.exe)|*.EXE" Ч описание и маска файлов через |
    // ¬тора¤ часть "All files (*.*)|*.*" Ч показать вообще все файлы
    OpenDialog1->Filter = "Executable files (*.exe)|*.EXE|All files (*.*)|*.*";
    // Title Ч заголовок окна диалога
    OpenDialog1->Title = "Select program to pin";
    // InitialDir Ч папка котора¤ откроетс¤ по умолчанию
    // System32 Ч там лежат calc.exe и mspaint.exe
    OpenDialog1->InitialDir = "C:\\Windows\\System32\\";
    
    // ========== Ќј—“–ќ… ј ѕќЋ≈… ¬¬ќƒј ѕ”“≈… ==========
    // ReadOnly = true Ч пользователь не может печатать в поле, только смотреть
    EDIT_D4->ReadOnly = true;
    EDIT_D16->ReadOnly = true;
    // Color = clBtnFace Ч цвет как у кнопки (серый), показывает что поле недоступно
    EDIT_D4->Color = clBtnFace;
    EDIT_D16->Color = clBtnFace;
    
    // ========== «ј√–”∆ј≈ћ —ќ’–јЌ®ЌЌџ≈ Ќј—“–ќ… » »« INI-‘ј…Ћј ==========
    // ≈сли пользователь уже выбирал программы Ч они загруз¤тс¤
    LoadSettings();
    
    // ========== ќЅЌќ¬Ћя≈ћ ќ“ќЅ–ј∆≈Ќ»≈ ѕ”“≈… Ќј ‘ќ–ћ≈ ==========
    // ѕоказываем в пол¤х EDIT_D4 и EDIT_D16 текущие пути
    UpdateD4PathDisplay();
    UpdateD16PathDisplay();
    // ========== “≈ —“ ¬ —“ј“”—Ќќ… —“–ќ ≈ ==========
    // SimpleText Ч простой текст в статусной строке (без панелей)    
    SB_MAIN_STATUS_BAR->SimpleText = "Ready";
}

//---------------------------------------------------------------------------
// RefreshComPorts Ч читает реестр Windows и заполн¤ет выпадающий список
// актуальными COM-портами.
//
// √де Windows хранит список COM-портов?
// ¬ реестре по адресу: HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM
// 
// –еестр Windows Ч это огромна¤ база данных где хран¤тс¤ ¬—≈ настройки системы.
// ћы идЄм в специальную ветку где Windows записывает каждый COM-порт.
//---------------------------------------------------------------------------
void TForm1::RefreshComPorts()
{
    // ќчищаем список перед заполнением
    CMB_COM_PORT->Clear();

    // —оздаЄм объект дл¤ работы с реестром.
    // new Ч выдел¤ем пам¤ть в куче. ¬ конце функции об¤зательно delete.
    TRegistry *reg = new TRegistry();

    // ”станавливаем свойство RootKey Ч указываем с какой корневой ветки
    // реестра начинать поиск. HKEY_LOCAL_MACHINE Ч ветка с информацией
    // о железе компьютера, именно там Windows хранит список COM-портов.
    reg->RootKey = HKEY_LOCAL_MACHINE;

    // ќткрываем ключ реестра только дл¤ чтени¤
    if (reg->OpenKeyReadOnly("HARDWARE\\DEVICEMAP\\SERIALCOMM"))
    {
        // TStringList Ч список строк из VCL.
        // Ѕудем хранить в нЄм имена записей реестра.
        TStringList *values = new TStringList();

        // GetValueNames заполн¤ет список именами всех записей в ключе.
        // Ќапример: "\Device\Serial0", "\Device\VCP0" и т.д.
        reg->GetValueNames(values);

        // ѕеребираем все найденные записи
        for (int i = 0; i < values->Count; i++)
        {
            // ReadString по имени записи возвращает еЄ значение Ч
            // реальное им¤ порта, например "COM3" или "COM13"
            String portName = reg->ReadString(values->Strings[i]);

            // Pos("COM") == 1 Ч строка начинаетс¤ с "COM".
            // ¬ C++ Builder индексаци¤ строк с 1, не с 0.
            if (portName.Pos("COM") == 1)
            {
                CMB_COM_PORT->Items->Add(portName);
            }
        }

        // ќсвобождаем пам¤ть Ч всЄ что создано через new нужно удалить
        delete values;
        reg->CloseKey();
    }

    delete reg;

    // ≈сли портов не нашли Ч добавл¤ем заглушку
    if (CMB_COM_PORT->Items->Count == 0)
    {
        CMB_COM_PORT->Items->Add("No COM ports found");
    }

    // јвтоматически выбираем первый порт в списке
    CMB_COM_PORT->ItemIndex = 0;
}

//---------------------------------------------------------------------------
// CheckESP32 Ч отправл¤ет команду TEST и ищет ответ "ESP32_OK"
// среди всех строк которые прислал ESP32.
//
// ”читываем что ESP32 при старте сначала отправл¤ет "ESP32_READY",
// поэтому читаем весь буфер и ищем нужную строку среди всех.
//
// ¬озвращает true если ESP32 найден и отвечает, false иначе.
//---------------------------------------------------------------------------
bool TForm1::CheckESP32(HANDLE hPort)
{
    if (hPort == INVALID_HANDLE_VALUE)
        return false;

    char testCmd[] = "TEST\n";
    DWORD bytesWritten;

    // “ри попытки Ч ESP32 может быть зан¤та или не сразу ответит
    for (int attempt = 0; attempt < 3; attempt++)
    {
        // ќчищаем буфер перед каждой попыткой.
        // PURGE_RXCLEAR Ч очистить буфер приЄма
        // PURGE_TXCLEAR Ч очистить буфер передачи
        // Ёто убирает ARDUINO_READY и другой "мусор" накопившийс¤ в буфере
        PurgeComm(hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);

        // ќтправл¤ем TEST в порт
        if (!WriteFile(hPort, testCmd, strlen(testCmd), &bytesWritten, NULL))
        {
            Sleep(100);
            continue;
        }

        // ∆дЄм пока ESP32 обработает команду и ответит.
        // Sleep здесь допустим Ч это единственное место в программе
        // где мы блокируемс¤ намеренно, до запуска таймера.
        Sleep(300);

        // „итаем всЄ что пришло в буфер
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

        // —тавим нулевой байт в конце Ч признак конца строки в C
        buffer[bytesRead] = '\0';

        // ѕревращаем сырой буфер в строку и разбиваем по строкам.
        // ESP32 мог прислать несколько сообщений подр¤д, например:
        // "ESP32_READY\nESP32_OK\n"
        // TStringList автоматически разобьЄт текст по \n
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
// SendCommand Ч отправл¤ет команду в COM-порт.
// ѕровер¤ет подключение и флаг ожидани¤ ответа.
//---------------------------------------------------------------------------
void TForm1::SendCommand(AnsiString command)
{
    if (!connected || hCom == INVALID_HANDLE_VALUE)
        return;

    // Ќе отправл¤ем новую команду пока не получили ответ на предыдущую
    if (commandPending)
    {
        SB_MAIN_STATUS_BAR->SimpleText = "Busy Ч waiting for ESP32 response...";
        return;
    }

    // ESP32 ждЄт команду с символом \n в конце
    AnsiString cmd = command + "\n";
    DWORD bytesWritten;

    if (WriteFile(hCom, cmd.c_str(), cmd.Length(), &bytesWritten, NULL))
    {
        commandPending   = true;              // “еперь ждЄм ответ
        lastCommand      = command;           // «апоминаем что отправили
        commandStartTime = GetTickCount();    // «апоминаем врем¤ отправки
    }
}

//---------------------------------------------------------------------------
// ParseData Ч разбирает каждую строку пришедшую от ESP32.
// ¬ызываетс¤ из TimerReadComTimer дл¤ каждой строки отдельно.
//---------------------------------------------------------------------------
void TForm1::ParseData(AnsiString data)
{
    // ”бираем пробелы и переносы строк по кра¤м
    data = data.Trim();

    SB_MAIN_STATUS_BAR->SimpleText = "GOT: " + data;

    // ѕустую строку игнорируем
    if (data.IsEmpty()) return;

    // --- ќтвет на TEST (от CheckESP32, не должен сюда попасть,
    //     но на вс¤кий случай обрабатываем) ---
    if (data == "ESP32_OK" || data == "ESP32_READY")
    {
        SB_MAIN_STATUS_BAR->SimpleText = "ESP32: " + data;
        return;
    }

    // ---  нопка CALC (D4) ---
    // ‘ормат: "BTN_CALC:1" (нажата) или "BTN_CALC:0" (отпущена)
    if (data == "BTN_CALC:1")
    {
        LBL_CALC_STATUS->Caption = "INVOKE CALC.  PRESS D4.  STATUS: PRESSED";
        SB_MAIN_STATUS_BAR->SimpleText = "D4 pressed Ч launching assigned program";
        
        // «апускаем программу назначенную на D4
        ExecuteD4Program();
        return;
    }

    if (data == "BTN_CALC:0")
    {
        LBL_CALC_STATUS->Caption = "INVOKE CALC.  PRESS D4.  STATUS: released";
        SB_MAIN_STATUS_BAR->SimpleText = "D4 released";
        return;
    }

    // ---  нопка PAINT (D16) ---
    if (data == "BTN_PAINT:1")
    {
        LBL_PAINT_STATUS->Caption = "INVOKE PAINT. PRESS D16. STATUS: PRESSED";
        SB_MAIN_STATUS_BAR->SimpleText = "D16 pressed Ч launching assigned program";
        
        // «апускаем программу назначенную на D16
        ExecuteD16Program();
        return;
    }

    if (data == "BTN_PAINT:0")
    {
        LBL_PAINT_STATUS->Caption = "INVOKE PAINT. PRESS D16. STATUS: released";
        SB_MAIN_STATUS_BAR->SimpleText = "D16 released";
        return;
    }

    // --- ¬сЄ остальное Ч показываем в статусной строке ---
    SB_MAIN_STATUS_BAR->SimpleText = "ESP32: " + data;
}

//---------------------------------------------------------------------------
// SetConnectedState Ч обновл¤ет весь UI в зависимости от состо¤ни¤
// подключени¤. ≈дина¤ точка обновлени¤ интерфейса.
//---------------------------------------------------------------------------
void TForm1::SetConnectedState(bool state, AnsiString port)
{
    connected = state;

    if (state)
    {
        LBL_CONNECTION_STATUS->Caption =
            "CONNECTION STATUS: CONNECTED to " + port;
        LBL_CONNECTION_STATUS->Font->Color = clGreen;
        BTN_CONNECT->Enabled    = false;  // Ќельз¤ подключитьс¤ повторно
        BTN_DISCONNECT->Enabled = true;
        CMB_COM_PORT->Enabled   = false;  // Ќельз¤ мен¤ть порт во врем¤ работы
        TimerReadCom->Enabled   = true;   // «апускаем чтение порта

        // —ообщаем ESP32 что Windows подключилась
        // Ќебольша¤ пауза чтобы ESP32 успела переключитьс¤
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
        TimerReadCom->Enabled   = false;  // ќстанавливаем чтение порта

        // —брасываем статусы кнопок
        LBL_CALC_STATUS->Caption  = "INVOKE CALC.  PRESS D4.  STATUS:";
        LBL_PAINT_STATUS->Caption = "INVOKE PAINT. PRESS D16. STATUS:";

        // —брасываем флаг ожидани¤ команды
        commandPending = false;
        lastCommand    = "";
    }
}

//---------------------------------------------------------------------------
// BTN_CONNECTClick Ч открывает порт, настраивает параметры,
// провер¤ет что это наш ESP32, запускает таймер.
//---------------------------------------------------------------------------
void __fastcall TForm1::BTN_CONNECTClick(TObject *Sender)
{
    if (connected)
    {
        ShowMessage("Already connected!");
        return;
    }

    // --- ѕолучаем им¤ порта из списка ---
    if (CMB_COM_PORT->ItemIndex < 0 ||
        CMB_COM_PORT->Items->Strings[CMB_COM_PORT->ItemIndex] == "No COM ports found")
    {
        ShowMessage("Please select a COM port.");
        return;
    }

    String portName = CMB_COM_PORT->Items->Strings[CMB_COM_PORT->ItemIndex];

    // --- ‘ормируем им¤ порта дл¤ Windows API ---
    // COM1-COM9 открываютс¤ просто по имени: "COM3"
    // COM10 и выше требуют специального префикса: "\\.\\COM13"
    // Ѕез префикса CreateFile вернЄт ошибку дл¤ портов с двузначным номером
    String comPort;
    String numStr  = portName.SubString(4, portName.Length() - 3);
    int    portNum = StrToIntDef(numStr, 0);

    if (portNum > 9)
        comPort = "\\\\.\\"+portName;   // COM10+: добавл¤ем префикс
    else
        comPort = portName;             // COM1-COM9: оставл¤ем как есть

    // --- ќткрываем COM-порт через Windows API ---
    // CreateFile Ч универсальна¤ функци¤ Windows дл¤ открыти¤ файлов и устройств.
    // COM-порт дл¤ Windows тоже ¤вл¤етс¤ устройством Ч открываем так же как файл.
    hCom = CreateFile(
        comPort.c_str(),               // »м¤ порта в стиле C-строки
        GENERIC_READ | GENERIC_WRITE,  // –азрешаем чтение и запись
        0,                             // Ёксклюзивный доступ Ч никто другой не откроет
        NULL,                          // Ѕезопасность по умолчанию
        OPEN_EXISTING,                 // ќткрываем существующее устройство
        FILE_ATTRIBUTE_NORMAL,         // ќбычные атрибуты
        NULL                           // Ѕез шаблона
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

    // --- Ќастраиваем параметры порта ---
    // DCB (Device Control Block) Ч структура Windows с настройками порта.
    // —начала читаем текущие настройки, потом мен¤ем только нужные нам четыре.
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(hCom, &dcb))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error reading port settings.");
        return;
    }

    // ”станавливаем параметры 115200 / 8N1 Ч стандарт дл¤ ESP32
    dcb.BaudRate = CBR_115200;   // —корость 115200 бод Ч должна совпадать с ESP32
    dcb.ByteSize = 8;            // 8 бит данных
    dcb.StopBits = ONESTOPBIT;   // 1 стоп-бит
    dcb.Parity   = NOPARITY;     // Ѕез проверки чЄтности

    if (!SetCommState(hCom, &dcb))
    {
        CloseHandle(hCom);
        hCom = INVALID_HANDLE_VALUE;
        ShowMessage("Error applying port settings.");
        return;
    }

    // --- Ё“јѕ 1: ћедленные таймауты дл¤ CheckESP32 ---
    // ReadFile должен ∆ƒј“№ ответа Ч ESP32 нужно врем¤ чтобы ответить.
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

    // --- ѕровер¤ем что это действительно наш ESP32 ---
    SB_MAIN_STATUS_BAR->SimpleText = "Checking ESP32...";
    Application->ProcessMessages();  // ƒаЄм UI обновитьс¤ пока идЄт проверка

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

    // --- Ё“јѕ 2: Ѕыстрые таймауты дл¤ рабочего режима ---
    // ReadFile должен возвращатьс¤ ћ√Ќќ¬≈ЌЌќ.
    //  омбинаци¤ MAXDWORD + 0 + 0 означает:
    // "если в буфере есть данные Ч отдай, нет Ч сразу вернись с 0 байт"
    // Ѕез этого ReadFile заблокирует UI на врем¤ ожидани¤.
    COMMTIMEOUTS workTO;
    workTO.ReadIntervalTimeout         = 0xFFFFFFFF;  // MAXDWORD
    workTO.ReadTotalTimeoutMultiplier  = 0;
    workTO.ReadTotalTimeoutConstant    = 0;
    workTO.WriteTotalTimeoutMultiplier = 10;
    workTO.WriteTotalTimeoutConstant   = 50;

    SetCommTimeouts(hCom, &workTO);
    PurgeComm(hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);

    // --- ¬сЄ готово Ч обновл¤ем UI и запускаем таймер ---
    SetConnectedState(true, portName);
    SB_MAIN_STATUS_BAR->SimpleText = "Connected to ESP32 on " + portName;
}

//---------------------------------------------------------------------------
// BTN_DISCONNECTClick Ч закрывает порт и сбрасывает UI.
//---------------------------------------------------------------------------
void __fastcall TForm1::BTN_DISCONNECTClick(TObject *Sender)
{
    if (!connected)
    {
        ShowMessage("Not connected!");
        return;
    }

    // ќстанавливаем таймер первым делом Ч прекращаем читать порт
    TimerReadCom->Enabled = false;

    if (hCom != INVALID_HANDLE_VALUE)
    {
        // ќтправл¤ем LED_OFF напр¤мую через WriteFile мину¤ SendCommand.
        // ѕри отключении не нужна защита commandPending и не нужен ответ Ч
        // просто отправл¤ем и сразу закрываем порт.
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
// TimerReadComTimer Ч срабатывает каждые 50 мс.
// „итает всЄ что накопилось в буфере порта,
// разбивает на строки и передаЄт каждую в ParseData.
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

            // –азбиваем полученные данные на строки.
            // ћожет прийти сразу несколько сообщений, например:
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
        // ќшибка чтени¤ Ч порт мог отключитьс¤ физически
        DWORD err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_INVALID_HANDLE)
        {
            // ¬ызываем Disconnect как будто нажали кнопку
            BTN_DISCONNECTClick(NULL);
            ShowMessage("ESP32 disconnected unexpectedly!");
        }
    }

    // ѕровер¤ем таймаут команды (5 секунд без ответа)
    if (commandPending && (GetTickCount() - commandStartTime > 5000))
    {
        commandPending = false;
        SB_MAIN_STATUS_BAR->SimpleText = "Command timeout Ч no response from ESP32";
    }
}

//---------------------------------------------------------------------------
// BTN_PIN_TO_D4Click и BTN_PIN_TO_D16Click Ч
// заглушки дл¤ „асти 3 (назначение любой программы).
// ѕока ничего не делают.
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
// FormClose Ч вызываетс¤ когда пользователь закрывает окно (крестик или Alt+F4)
// 
// TCloseAction &Action Ч параметр-ссылка, через него можно сказать:
// "не закрывайс¤, ¤ передумал" (Action = caNone)
void __fastcall TForm1::FormClose(TObject *Sender, TCloseAction &Action)
{
    // ѕеред закрытием сохран¤ем все настройки в INI-файл
    // „тобы при следующем запуске всЄ было как пользователь настроил
        SaveSettings();
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// LoadSettings Ч загружает пути к программам из INI файла
//---------------------------------------------------------------------------
void TForm1::LoadSettings()
{
    // ќпредел¤ем путь к INI файлу (р¤дом с EXE)
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
        // ѕервый запуск Ч используем стандартные значени¤
        SB_MAIN_STATUS_BAR->SimpleText = "Using default program paths";
    }
}

//---------------------------------------------------------------------------
// SaveSettings Ч сохран¤ет пути к программам в INI файл
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
// UpdateD4PathDisplay Ч обновл¤ет поле EDIT_D4 и подпись
//---------------------------------------------------------------------------
void TForm1::UpdateD4PathDisplay()
{
    EDIT_D4->Text = D4_ProgramPath;
    
    // »звлекаем только им¤ файла дл¤ краткой подписи
    String fileName = ExtractFileName(D4_ProgramPath);
    LBL_D4_PINNED_TO->Caption = "D4 PINNED TO: " + fileName;
}

//---------------------------------------------------------------------------
// UpdateD16PathDisplay Ч обновл¤ет поле EDIT_D16 и подпись
//---------------------------------------------------------------------------
void TForm1::UpdateD16PathDisplay()
{
    EDIT_D16->Text = D16_ProgramPath;
    
    String fileName = ExtractFileName(D16_ProgramPath);
    LBL_D16_PINNED_TO->Caption = "D16 PINNED TO: " + fileName;
}

//---------------------------------------------------------------------------
// ExecuteD4Program Ч запускает программу назначенную на D4
//---------------------------------------------------------------------------
void TForm1::ExecuteD4Program()
{
    if (D4_ProgramPath.IsEmpty())
    {
        SB_MAIN_STATUS_BAR->SimpleText = "No program assigned to D4";
        return;
    }
    
    // ѕреобразуем String в char* дл¤ WinExec
    // »спользуем ShellExecute дл¤ лучшего контрол¤, но можно и WinExec
    String cmd = "\"" + D4_ProgramPath + "\"";
    
    UINT result = WinExec(cmd.c_str(), SW_SHOW);
    if (result <= 31)
    {
        // ѕробуем через ShellExecute если WinExec не справилс¤
        ShellExecute(NULL, "open", D4_ProgramPath.c_str(), NULL, NULL, SW_SHOW);
    }
    
    SB_MAIN_STATUS_BAR->SimpleText = "Launched D4 program: " + ExtractFileName(D4_ProgramPath);
}

//---------------------------------------------------------------------------
// ExecuteD16Program Ч запускает программу назначенную на D16
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

