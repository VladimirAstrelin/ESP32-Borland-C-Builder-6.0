// =====================================================
// ESP32 Dev Module + SSD1306 + ТРИ кнопки на пинах D4, D16, D17
// Связь с Windows программой через COM-порт (USB Serial)
//
// ЖЕЛЕЗО:
//   ESP32 Dev Module  — основная плата
//   SSD1306 128x64    — OLED дисплей
//   BTN_1 на D4       — кнопка 1 (запускает программу назначенную на D4)
//   BTN_2 на D16      — кнопка 2 (запускает программу назначенную на D16)
//   BTN_3 на D17      — кнопка 3 (запускает программу назначенную на D17)
//   LED   на D2       — встроенный светодиод ESP32
//
// ПРОТОКОЛ ОБЩЕНИЯ С WINDOWS:
//   Windows → ESP32:   "TEST\n"         проверка связи
//                      "CONNECTED\n"    Windows подключилась
//                      "DISCONNECTED\n" Windows отключилась
//   ESP32   → Windows: "ESP32_READY"    ESP32 стартовала
//                      "ESP32_OK"       ответ на TEST
//                      "CONNECTED_OK"   ответ на CONNECTED
//                      "BTN_D4:1"       кнопка D4 нажата
//                      "BTN_D4:0"       кнопка D4 отпущена
//                      "BTN_D16:1"      кнопка D16 нажата
//                      "BTN_D16:0"      кнопка D16 отпущена
//                      "BTN_D17:1"      кнопка D17 нажата
//                      "BTN_D17:0"      кнопка D17 отпущена
// =====================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// НАСТРОЙКИ
// =====================================================

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C

#define LED_PIN   2    // Встроенный светодиод
#define BTN_1     4    // Кнопка 1 — пин D4
#define BTN_2     16   // Кнопка 2 — пин D16
#define BTN_3     17   // Кнопка 3 — пин D17

#define DEBOUNCE_MS  5   // Время антидребезга (мс)
#define BTN_SEND_MS  50  // Минимальный интервал между отправками (мс)

// =====================================================
// ОБЪЕКТ ДИСПЛЕЯ
// =====================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// ПЕРЕМЕННЫЕ СОСТОЯНИЯ КНОПОК
// Для каждой кнопки — свой независимый набор переменных
// =====================================================

// --- Кнопка 1 (D4) ---
int           btn1Raw          = HIGH;
int           btn1LastStable   = HIGH;
unsigned long btn1DebounceStart = 0;
bool          btn1Pressed       = false;
unsigned long btn1LastSendTime  = 0;

// --- Кнопка 2 (D16) ---
int           btn2Raw          = HIGH;
int           btn2LastStable   = HIGH;
unsigned long btn2DebounceStart = 0;
bool          btn2Pressed       = false;
unsigned long btn2LastSendTime  = 0;

// --- Кнопка 3 (D17) ---
int           btn3Raw          = HIGH;
int           btn3LastStable   = HIGH;
unsigned long btn3DebounceStart = 0;
bool          btn3Pressed       = false;
unsigned long btn3LastSendTime  = 0;

// --- Флаг обновления дисплея ---
bool displayNeedsUpdate = false;

// --- Статус подключения к Windows ---
bool displayConnected = false;

// =====================================================
// ФУНКЦИЯ: обновление дисплея
//
// Дисплей 128x64 пикселей, шрифт размера 1 = 6x8 пикселей на символ.
// В одну строку влезает 21 символ, всего строк до 8 шт.
//
// МАКЕТ ЭКРАНА:
// ┌─────────────────────┐  y=0
// │ ESP32: PC CONNECTED │  заголовок (меняется)
// ├─────────────────────┤  y=10 (линия)
// │ D4 : PRESSED        │  y=16 состояние кнопки 1
// │ D16: released       │  y=28 состояние кнопки 2
// │ D17: released       │  y=40 состояние кнопки 3
// │                     │
// │ Windows: ONLINE     │  y=56 статус подключения
// └─────────────────────┘  y=64
// =====================================================

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);

    // --- Заголовок (строка 1) ---
    display.setCursor(0, 0);
    if (displayConnected) {
        display.println(" ESP32: PC CONNECTED");
    } else {
        display.println("   ESP32 v2.0 READY");
    }

    // --- Разделитель ---
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // --- Кнопка 1 (D4) ---
    // Формат: "D4 : PRESSED" или "D4 : released"
    display.setCursor(0, 16);
    display.print("D4 : ");
    display.println(btn1Pressed ? "PRESSED" : "released");

    // --- Кнопка 2 (D16) ---
    display.setCursor(0, 28);
    display.print("D16: ");
    display.println(btn2Pressed ? "PRESSED" : "released");

    // --- Кнопка 3 (D17) ---
    display.setCursor(0, 40);
    display.print("D17: ");
    display.println(btn3Pressed ? "PRESSED" : "released");

    // --- Статус подключения (последняя строка) ---
    display.setCursor(0, 56);
    display.println(displayConnected ? "Windows: ONLINE" : "Windows: offline");

    // Отправляем буфер на физический экран по I2C
    display.display();
}

// =====================================================
// ФУНКЦИЯ: обработка одной кнопки с антидребезгом
//
// Принимает переменные кнопки по ссылке (&) —
// изменения внутри функции меняют оригинальные переменные.
// =====================================================

void processButton(
  int pin,
  int &raw,
  int &lastStable,
  unsigned long &debounceStart,
  bool &pressed,
  unsigned long &lastSendTime,
  const char* pressMsg,
  const char* releaseMsg)
{
    int currentRaw = digitalRead(pin);

    // Если сигнал изменился — начинаем отсчёт антидребезга заново
    if (currentRaw != raw) {
        raw           = currentRaw;
        debounceStart = millis();
    }

    // Если сигнал стабилен дольше DEBOUNCE_MS и действительно изменился
    if (millis() - debounceStart >= DEBOUNCE_MS && currentRaw != lastStable) {
        lastStable = currentRaw;

        // Защита от спама — не чаще BTN_SEND_MS
        if (millis() - lastSendTime >= BTN_SEND_MS) {
            lastSendTime = millis();

            if (lastStable == LOW) {
                // LOW = кнопка нажата (пин соединён с GND)
                pressed = true;
                Serial.println(pressMsg);
            } else {
                // HIGH = кнопка отпущена (внутренняя подтяжка вернула +3.3V)
                pressed = false;
                Serial.println(releaseMsg);
            }

            displayNeedsUpdate = true;
        }
    }
}

// =====================================================
// setup() — выполняется один раз при старте
// =====================================================

void setup() {
    // Настройка пинов
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_1,   INPUT_PULLUP);  
    pinMode(BTN_2,   INPUT_PULLUP);
    pinMode(BTN_3,   INPUT_PULLUP);  

    digitalWrite(LED_PIN, LOW);  // Светодиод выключен

    // Запуск Serial
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32_READY");

    // Инициализация дисплея
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("Display init failed!");
        while (true) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(200);
        }
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    updateDisplay();

    Serial.println("Ready.");
}

// =====================================================
// loop() — выполняется бесконечно
//
// Структура каждой итерации:
//   1. Команды от Windows (Serial)
//   2. Кнопка 1 D4  (антидребезг)
//   3. Кнопка 2 D16 (антидребезг)
//   4. Кнопка 3 D17 (антидребезг) 
//   5. Обновление дисплея (если нужно)
// =====================================================

void loop() {

    // ========== 1. КОМАНДЫ ОТ WINDOWS ==========
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "TEST") {
            // Проверка связи — Windows убеждается что это ESP32
            Serial.println("ESP32_OK");
        }
        else if (command == "CONNECTED") {
            // Windows подключилась — показываем на дисплее
            displayConnected   = true;
            displayNeedsUpdate = true;
            Serial.println("CONNECTED_OK");
        }
        else if (command == "DISCONNECTED") {
            // Windows отключилась
            displayConnected   = false;
            displayNeedsUpdate = true;
        }
    }

    // ========== 2. КНОПКА 1 (D4) ==========
    processButton(
        BTN_1,
        btn1Raw,
        btn1LastStable,
        btn1DebounceStart,
        btn1Pressed,
        btn1LastSendTime,
        "BTN_D4:1",    // Нажата
        "BTN_D4:0"     // Отпущена
    );

    // ========== 3. КНОПКА 2 (D16) ==========
    processButton(
        BTN_2,
        btn2Raw,
        btn2LastStable,
        btn2DebounceStart,
        btn2Pressed,
        btn2LastSendTime,
        "BTN_D16:1",   // Нажата
        "BTN_D16:0"    // Отпущена
    );

    // ========== 4. КНОПКА 3 (D17) ==========
    processButton(
        BTN_3,
        btn3Raw,
        btn3LastStable,
        btn3DebounceStart,
        btn3Pressed,
        btn3LastSendTime,
        "BTN_D17:1",   // Нажата
        "BTN_D17:0"    // Отпущена
    );

    // ========== 5. ОБНОВЛЕНИЕ ДИСПЛЕЯ ==========
    if (displayNeedsUpdate) {
        updateDisplay();
        displayNeedsUpdate = false;
    }

    delay(1);
}














// // =====================================================
// // ESP32 Dev Module + SSD1306 + две кнопки
// // Связь с Windows программой через COM-порт (USB Serial)
// //
// // ЖЕЛЕЗО:
// //   ESP32 Dev Module  — основная плата (микроконтроллер)
// //   SSD1306 128x64    — OLED дисплей (экранчик 128 на 64 пикселя)
// //   BTN_CALC  на D4   — кнопка запуска программы для D4
// //   BTN_PAINT на D16  — кнопка запуска программы для D16
// //   LED       на D2   — встроенный светодиод ESP32
// //
// // ПРОТОКОЛ ОБЩЕНИЯ С WINDOWS (язык на котором общаются):
// //   Windows → ESP32:  "TEST\n"         (проверка связи)
// //   ESP32   → Windows: "ESP32_READY"   (при старте, я готов)
// //                      "ESP32_OK"      (ответ на TEST, да это я)
// //                      "BTN_CALC:1"    (кнопка D4 нажата)
// //                      "BTN_CALC:0"    (кнопка D4 отпущена)
// //                      "BTN_PAINT:1"   (кнопка D16 нажата)
// //                      "BTN_PAINT:0"   (кнопка D16 отпущена)
// // =====================================================

// // Подключаем библиотеки для работы с железом
// #include <Arduino.h>      // Главная библиотека Arduino (функции pinMode, digitalRead и т.д.)
// #include <Wire.h>         // Протокол I2C — по нему общаемся с дисплеем
// #include <Adafruit_GFX.h> // Библиотека для рисования на дисплеях (графика)
// #include <Adafruit_SSD1306.h> // Библиотека конкретно для дисплея SSD1306

// // =====================================================
// // НАСТРОЙКИ (константы, которые не меняются)
// // =====================================================
// // #define — это как "найти и заменить" перед компиляцией
// // Везде где написано SCREEN_WIDTH — подставится 128

// #define SCREEN_WIDTH  128  // Ширина дисплея в пикселях
// #define SCREEN_HEIGHT 64   // Высота дисплея в пикселях
// #define OLED_RESET    -1   // Пин сброса дисплея (-1 = не используется)
// #define OLED_ADDR     0x3C // I2C адрес дисплея (как номер квартиры на общей шине)

// #define LED_PIN       2    // Встроенный светодиод подключён к пину D2
// #define BTN_CALC      4    // Кнопка CALC подключена к пину D4
// #define BTN_PAINT     16   // Кнопка PAINT подключена к пину D16

// #define DEBOUNCE_MS   5    // Время антидребезга в миллисекундах
// #define BTN_SEND_MS   50   // Минимальный интервал между отправками (защита от спама)

// // =====================================================
// // ОБЪЕКТ ДИСПЛЕЯ
// // =====================================================
// // Создаём объект display для работы с нашим экранчиком
// // Параметры: ширина, высота, интерфейс (Wire = I2C), пин сброса
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// // =====================================================
// // ПЕРЕМЕННЫЕ СОСТОЯНИЯ КНОПОК
// // 
// // Для КАЖДОЙ кнопки свой набор переменных.
// // Это нужно чтобы кнопки работали НЕЗАВИСИМО друг от друга.
// // =====================================================

// // --- Кнопка CALC (D4) ---
// int           calcRaw          = HIGH;  // Сырое (с дребезгом) состояние пина
// int           calcLastStable   = HIGH;  // Подтверждённое состояние после антидребезга
// unsigned long calcDebounceStart = 0;    // Момент когда сигнал начал меняться (в мс)
// bool          calcPressed       = false; // Логическое состояние для отображения
// unsigned long calcLastSendTime  = 0;    // Время последней отправки в Serial (для защиты от спама)

// // --- Кнопка PAINT (D16) ---
// int           paintRaw          = HIGH;
// int           paintLastStable   = HIGH;
// unsigned long paintDebounceStart = 0;
// bool          paintPressed       = false;
// unsigned long paintLastSendTime  = 0;

// // --- Флаг обновления дисплея ---
// // Если true — при следующей итерации loop() дисплей перерисуется
// bool displayNeedsUpdate = false;

// // --- Статус подключения к Windows ---
// // true = Windows программа подключена и общается с нами
// // false = Windows программа отключена
// bool displayConnected = false;

// // =====================================================
// // ФУНКЦИЯ: обновление дисплея
// // 
// // Полностью перерисовывает всё что на экране
// // Вызывается когда что-то изменилось (кнопка нажата, статус подключения)
// // =====================================================
// void updateDisplay() {
//     display.clearDisplay();  // Стираем всё с экрана
    
//     display.setTextSize(1);  // Размер текста: 1 = самый маленький
//     display.setCursor(0, 0); // Ставим курсор в левый верхний угол (x=0, y=0)
    
//     // Заголовок меняется в зависимости от состояния подключения
//     if (displayConnected) {
//         display.println(" ESP32: PC CONNECTED");  // println — печатает и переводит строку
//     } else {
//         display.println("  ESP32 CALC & PAINT");
//     }
    
//     // Рисуем горизонтальную линию-разделитель
//     // Параметры: x1, y1, x2, y2, цвет
//     display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

//     // Показываем состояние кнопки CALC
//     display.setCursor(0, 18);
//     display.print("CALC  (D4):  ");  // print — печатает без перевода строки
//     // Тернарный оператор: условие ? значение_если_да : значение_если_нет
//     display.println(calcPressed ? "PRESSED" : "released");

//     // Показываем состояние кнопки PAINT
//     display.setCursor(0, 36);
//     display.print("PAINT (D16): ");
//     display.println(paintPressed ? "PRESSED" : "released");

//     // В самом низу показываем статус подключения к Windows
//     display.setCursor(0, 54);
//     display.println(displayConnected ? "Windows: ONLINE" : "Windows: offline");

//     // display() — отправляем всё что нарисовали на физический экран
//     display.display();
// }

// // =====================================================
// // ФУНКЦИЯ: обработка одной кнопки с антидребезгом
// // 
// // Эта функция — "мозг" обработки кнопки. Она:
// // 1. Читает сырое состояние пина (с дребезгом)
// // 2. Ждёт DEBOUNCE_MS миллисекунд чтобы убедиться что сигнал стабилен
// // 3. Если состояние действительно изменилось — отправляет сообщение в Serial
// // 
// // Параметры передаются ПО ССЫЛКЕ (знак &) — функция работает напрямую
// // с оригинальными переменными, а не с их копиями.
// // =====================================================
// void processButton(
//   int pin,                     // Номер пина к которому подключена кнопка
//   int &raw,                    // Сырое состояние (ссылка)
//   int &lastStable,             // Последнее стабильное состояние (ссылка)
//   unsigned long &debounceStart, // Время начала дребезга (ссылка)
//   bool &pressed,               // Нажата ли кнопка (ссылка)
//   unsigned long &lastSendTime, // Время последней отправки (ссылка)
//   const char* pressMsg,        // Сообщение при нажатии ("BTN_CALC:1")
//   const char* releaseMsg)      // Сообщение при отпускании ("BTN_CALC:0")
// {
//   // Читаем текущее состояние пина
//   // digitalRead возвращает HIGH (1) или LOW (0)
//   // Кнопка подключена через INPUT_PULLUP — значит:
//   //   HIGH = кнопка НЕ нажата
//   //   LOW  = кнопка НАЖАТА
//   int currentRaw = digitalRead(pin);

//   // Если сигнал ИЗМЕНИЛСЯ (был HIGH стал LOW или наоборот)
//   // Это может быть начало дребезга
//   if (currentRaw != raw) {
//     raw           = currentRaw;      // Запоминаем новое сырое состояние
//     debounceStart = millis();        // Запоминаем КОГДА произошло изменение
//   }

//   // Проверяем два условия:
//   // 1. Прошло ли больше DEBOUNCE_MS с момента последнего изменения
//   // 2. Отличается ли текущее состояние от последнего СТАБИЛЬНОГО
//   // 
//   // millis() возвращает количество миллисекунд с момента старта ESP32
//   if (millis() - debounceStart >= DEBOUNCE_MS && currentRaw != lastStable) {
//     lastStable = currentRaw;  // Запоминаем новое стабильное состояние

//     // Защита от спама — не отправляем сообщения чаще чем раз в BTN_SEND_MS
//     if (millis() - lastSendTime >= BTN_SEND_MS) {
//       lastSendTime = millis();  // Обновляем время последней отправки

//       if (lastStable == LOW) {      // LOW = кнопка НАЖАТА
//         pressed = true;
//         Serial.println(pressMsg);   // Отправляем в Serial (например "BTN_CALC:1")
//       } else {                      // HIGH = кнопка ОТПУЩЕНА
//         pressed = false;
//         Serial.println(releaseMsg); // Отправляем (например "BTN_CALC:0")
//       }

//       displayNeedsUpdate = true;  // Говорим что нужно обновить дисплей
//     }
//   }
// }

// // =====================================================
// // setup() — выполняется ОДИН РАЗ при старте ESP32
// // 
// // Здесь мы настраиваем всё железо и говорим "я готов"
// // =====================================================
// void setup() {
//   // Настраиваем пины на вход или выход
//   pinMode(LED_PIN,   OUTPUT);       // Светодиод — ВЫХОД (мы им управляем)
//   pinMode(BTN_CALC,  INPUT_PULLUP); // Кнопка — ВХОД с подтяжкой к питанию
//   pinMode(BTN_PAINT, INPUT_PULLUP); // INPUT_PULLUP включает встроенный резистор
  
//   digitalWrite(LED_PIN, LOW);       // Выключаем светодиод (LOW = 0 вольт)

//   // Запускаем Serial (связь с компьютером через USB)
//   // 115200 — скорость в бодах (бит в секунду), должна совпадать с Windows
//   Serial.begin(115200);

//   // Ждём 1 секунду чтобы USB-соединение установилось
//   delay(1000);

//   // Сообщаем компьютеру что ESP32 готова к работе
//   // Windows программа ищет эту строку среди первых сообщений
//   Serial.println("ESP32_READY");

//   // Инициализация дисплея
//   // SSD1306_SWITCHCAPVCC — тип питания дисплея (внутренний повышающий преобразователь)
//   // OLED_ADDR — I2C адрес (0x3C)
//   if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
//     // Если дисплей не найден — сообщаем об ошибке
//     Serial.println("Display init failed!");
//     // Бесконечный цикл — мигаем светодиодом в знак ошибки
//     while (true) {
//       digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // Переключаем светодиод
//       delay(200);  // Ждём 200 мс
//     }
//   }

//   // Настраиваем дисплей
//   display.clearDisplay();                // Очищаем
//   display.setTextColor(SSD1306_WHITE);   // Белый текст (монохромный дисплей)
//   display.setTextSize(1);                // Размер текста 1
//   updateDisplay();                       // Рисуем первый кадр

//   Serial.println("Ready.");  // Сообщаем в Serial что всё готово
// }

// // =====================================================
// // loop() — выполняется БЕСКОНЕЧНО после setup()
// // 
// // Это главный цикл программы. Каждую итерацию мы:
// //   1. Проверяем не пришли ли команды от Windows
// //   2. Обрабатываем кнопку CALC (антидребезг)
// //   3. Обрабатываем кнопку PAINT (антидребезг)
// //   4. Обновляем дисплей (если нужно)
// // =====================================================
// void loop() {

//   // ========== 1. КОМАНДЫ ОТ WINDOWS ==========
//   // Serial.available() возвращает сколько байт ждёт в буфере
//   if (Serial.available() > 0) {
//     // readStringUntil('\n') — читает всё до символа новой строки
//     String command = Serial.readStringUntil('\n');
//     command.trim();  // Убираем пробелы и \r по краям

//     // ---------- TEST ----------
//     // Windows отправляет TEST чтобы убедиться что это ESP32
//     if (command == "TEST") {
//       Serial.println("ESP32_OK");  // Отвечаем — да, это я!
//     }
    
//     // ---------- CONNECTED ----------
//     // Windows сообщает что подключение установлено
//     else if (command == "CONNECTED") {
//       displayConnected = true;      // Запоминаем что подключены
//       displayNeedsUpdate = true;    // Просим обновить дисплей
//       Serial.println("CONNECTED_OK");
//     }

//     // ---------- DISCONNECTED ----------
//     // Windows сообщает что отключилась
//     else if (command == "DISCONNECTED") {
//       displayConnected = false;     // Запоминаем что отключены
//       displayNeedsUpdate = true;    // Просим обновить дисплей
//     }
//   }

//   // ========== 2. КНОПКА CALC (D4) ==========
//   // Вызываем функцию обработки и передаём ВСЕ переменные этой кнопки
//   processButton(
//     BTN_CALC,           // Пин D4
//     calcRaw,            // Сырое состояние
//     calcLastStable,     // Стабильное состояние
//     calcDebounceStart,  // Время начала дребезга
//     calcPressed,        // Нажата ли
//     calcLastSendTime,   // Время последней отправки
//     "BTN_CALC:1",       // Сообщение при нажатии
//     "BTN_CALC:0"        // Сообщение при отпускании
//   );

//   // ========== 3. КНОПКА PAINT (D16) ==========
//   // Всё то же самое для второй кнопки
//   processButton(
//     BTN_PAINT,
//     paintRaw,
//     paintLastStable,
//     paintDebounceStart,
//     paintPressed,
//     paintLastSendTime,
//     "BTN_PAINT:1",
//     "BTN_PAINT:0"
//   );

//   // ========== 4. ОБНОВЛЕНИЕ ДИСПЛЕЯ ==========
//   // Если флаг displayNeedsUpdate установлен — перерисовываем экран
//   if (displayNeedsUpdate) {
//     updateDisplay();
//     displayNeedsUpdate = false;  // Сбрасываем флаг
//   }

//   delay(1);  // Маленькая пауза (1 мс) чтобы дать процессору передохнуть
// }