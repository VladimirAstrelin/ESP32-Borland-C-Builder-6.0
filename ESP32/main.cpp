// =====================================================
// ESP32 Dev Module + SSD1306 + ТРИ кнопки на пинах D4, D16, D17
// Связь с Windows программой через COM-порт (USB Serial)
//
// ВЕРСИЯ 2.1 - С ДЛИННЫМ НАЖАТИЕМ
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

#define LED_PIN   2
#define BTN_1     4
#define BTN_2     16
#define BTN_3     17

#define DEBOUNCE_MS      5     // Антидребезг (мс)
#define LONG_PRESS_MS    2000  // Длинное нажатие (2 секунды)

// =====================================================
// ОБЪЕКТ ДИСПЛЕЯ
// =====================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// ПЕРЕМЕННЫЕ КНОПКИ 1 (D4)
// =====================================================
int           btn1Raw          = HIGH;
int           btn1LastStable   = HIGH;
unsigned long btn1DebounceStart = 0;
bool          btn1Pressed       = false;
unsigned long btn1PressStart    = 0;     // Время начала нажатия (для длинного)
bool          btn1HoldSent      = false; // Отправили ли длинное нажатие

// =====================================================
// ПЕРЕМЕННЫЕ КНОПКИ 2 (D16)
// =====================================================
int           btn2Raw          = HIGH;
int           btn2LastStable   = HIGH;
unsigned long btn2DebounceStart = 0;
bool          btn2Pressed       = false;
unsigned long btn2PressStart    = 0;
bool          btn2HoldSent      = false;

// =====================================================
// ПЕРЕМЕННЫЕ КНОПКИ 3 (D17)
// =====================================================
int           btn3Raw          = HIGH;
int           btn3LastStable   = HIGH;
unsigned long btn3DebounceStart = 0;
bool          btn3Pressed       = false;
unsigned long btn3PressStart    = 0;
bool          btn3HoldSent      = false;

// =====================================================
// ОБЩИЕ ПЕРЕМЕННЫЕ
// =====================================================
bool displayNeedsUpdate = false;
bool displayConnected = false;

// =====================================================
// ФУНКЦИЯ: обновление дисплея
// =====================================================

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);

    display.setCursor(0, 0);
    if (displayConnected) {
        display.println(" ESP32: PC CONNECTED");
    } else {
        display.println("   ESP32 v2.1 READY");
    }

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 16);
    display.print("D4 : ");
    display.println(btn1Pressed ? "PRESSED" : "released");

    display.setCursor(0, 28);
    display.print("D16: ");
    display.println(btn2Pressed ? "PRESSED" : "released");

    display.setCursor(0, 40);
    display.print("D17: ");
    display.println(btn3Pressed ? "PRESSED" : "released");

    display.setCursor(0, 56);
    display.println(displayConnected ? "Windows: ONLINE" : "Windows: offline");

    display.display();
}

// =====================================================
// ФУНКЦИЯ: обработка одной кнопки (с длинным нажатием)
// =====================================================

void processButton(
  int pin,
  int &raw,
  int &lastStable,
  unsigned long &debounceStart,
  bool &pressed,
  unsigned long &pressStart,
  bool &holdSent,
  const char* shortPressMsg,
  const char* holdMsg)
{
    int currentRaw = digitalRead(pin);
    unsigned long now = millis();

    // ========== 1. АНТИДРЕБЕЗГ (как и было) ==========
    if (currentRaw != raw) {
        raw = currentRaw;
        debounceStart = now;
    }

    if (now - debounceStart >= DEBOUNCE_MS && currentRaw != lastStable) {
        lastStable = currentRaw;

        if (lastStable == LOW) {
            // КНОПКА НАЖАТА
            pressed = true;
            pressStart = now;      // Запоминаем когда нажали
            holdSent = false;      // Сбрасываем флаг длинного нажатия
            displayNeedsUpdate = true;
            
            // НЕ отправляем сразу "нажата" — ждём, может быть длинное нажатие
        } else {
            // КНОПКА ОТПУЩЕНА
            pressed = false;
            displayNeedsUpdate = true;
            
            // Если длинное нажатие НЕ отправляли — значит это короткое
            if (!holdSent) {
                Serial.println(shortPressMsg);  // "BTN_D4:1"
            }
        }
    }
    
    // ========== 2. ПРОВЕРКА ДЛИННОГО НАЖАТИЯ ==========
    // Если кнопка нажата, длинное нажатие ещё не отправлено,
    // и прошло больше LONG_PRESS_MS — отправляем HOLD
    if (pressed && !holdSent && (now - pressStart >= LONG_PRESS_MS)) {
        holdSent = true;
        Serial.println(holdMsg);   // "BTN_D4:HOLD"
        displayNeedsUpdate = true;
    }
}

// =====================================================
// setup()
// =====================================================

void setup() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_1,   INPUT_PULLUP);
    pinMode(BTN_2,   INPUT_PULLUP);
    pinMode(BTN_3,   INPUT_PULLUP);

    digitalWrite(LED_PIN, LOW);

    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32_READY");

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
// loop()
// =====================================================

void loop() {

    // ========== 1. КОМАНДЫ ОТ WINDOWS ==========
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "TEST") {
            Serial.println("ESP32_OK");
        }
        else if (command == "CONNECTED") {
            displayConnected = true;
            displayNeedsUpdate = true;
            Serial.println("CONNECTED_OK");
        }
        else if (command == "DISCONNECTED") {
            displayConnected = false;
            displayNeedsUpdate = true;
        }
    }

    // ========== 2. КНОПКА 1 (D4) ==========
    processButton(
        BTN_1,
        btn1Raw, btn1LastStable, btn1DebounceStart,
        btn1Pressed, btn1PressStart, btn1HoldSent,
        "BTN_D4:1", "BTN_D4:HOLD"
    );

    // ========== 3. КНОПКА 2 (D16) ==========
    processButton(
        BTN_2,
        btn2Raw, btn2LastStable, btn2DebounceStart,
        btn2Pressed, btn2PressStart, btn2HoldSent,
        "BTN_D16:1", "BTN_D16:HOLD"
    );

    // ========== 4. КНОПКА 3 (D17) ==========
    processButton(
        BTN_3,
        btn3Raw, btn3LastStable, btn3DebounceStart,
        btn3Pressed, btn3PressStart, btn3HoldSent,
        "BTN_D17:1", "BTN_D17:HOLD"
    );

    // ========== 5. ОБНОВЛЕНИЕ ДИСПЛЕЯ ==========
    if (displayNeedsUpdate) {
        updateDisplay();
        displayNeedsUpdate = false;
    }

    delay(1);
}




// // =====================================================
// // ESP32 Dev Module + SSD1306 + ТРИ кнопки на пинах D4, D16, D17
// // Связь с Windows программой через COM-порт (USB Serial)
// //
// // ЖЕЛЕЗО:
// //   ESP32 Dev Module  — основная плата
// //   SSD1306 128x64    — OLED дисплей
// //   BTN_1 на D4       — кнопка 1 (запускает программу назначенную на D4)
// //   BTN_2 на D16      — кнопка 2 (запускает программу назначенную на D16)
// //   BTN_3 на D17      — кнопка 3 (запускает программу назначенную на D17)
// //   LED   на D2       — встроенный светодиод ESP32
// //
// // ПРОТОКОЛ ОБЩЕНИЯ С WINDOWS:
// //   Windows → ESP32:   "TEST\n"         проверка связи
// //                      "CONNECTED\n"    Windows подключилась
// //                      "DISCONNECTED\n" Windows отключилась
// //   ESP32   → Windows: "ESP32_READY"    ESP32 стартовала
// //                      "ESP32_OK"       ответ на TEST
// //                      "CONNECTED_OK"   ответ на CONNECTED
// //                      "BTN_D4:1"       кнопка D4 нажата
// //                      "BTN_D4:0"       кнопка D4 отпущена
// //                      "BTN_D16:1"      кнопка D16 нажата
// //                      "BTN_D16:0"      кнопка D16 отпущена
// //                      "BTN_D17:1"      кнопка D17 нажата
// //                      "BTN_D17:0"      кнопка D17 отпущена
// // =====================================================

// #include <Arduino.h>
// #include <Wire.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_SSD1306.h>

// // =====================================================
// // НАСТРОЙКИ
// // =====================================================

// #define SCREEN_WIDTH  128
// #define SCREEN_HEIGHT 64
// #define OLED_RESET    -1
// #define OLED_ADDR     0x3C

// #define LED_PIN   2    // Встроенный светодиод
// #define BTN_1     4    // Кнопка 1 — пин D4
// #define BTN_2     16   // Кнопка 2 — пин D16
// #define BTN_3     17   // Кнопка 3 — пин D17

// #define DEBOUNCE_MS  5   // Время антидребезга (мс)
// #define BTN_SEND_MS  50  // Минимальный интервал между отправками (мс)

// // =====================================================
// // ОБЪЕКТ ДИСПЛЕЯ
// // =====================================================

// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// // =====================================================
// // ПЕРЕМЕННЫЕ СОСТОЯНИЯ КНОПОК
// // Для каждой кнопки — свой независимый набор переменных
// // =====================================================

// // --- Кнопка 1 (D4) ---
// int           btn1Raw          = HIGH;
// int           btn1LastStable   = HIGH;
// unsigned long btn1DebounceStart = 0;
// bool          btn1Pressed       = false;
// unsigned long btn1LastSendTime  = 0;

// // --- Кнопка 2 (D16) ---
// int           btn2Raw          = HIGH;
// int           btn2LastStable   = HIGH;
// unsigned long btn2DebounceStart = 0;
// bool          btn2Pressed       = false;
// unsigned long btn2LastSendTime  = 0;

// // --- Кнопка 3 (D17) ---
// int           btn3Raw          = HIGH;
// int           btn3LastStable   = HIGH;
// unsigned long btn3DebounceStart = 0;
// bool          btn3Pressed       = false;
// unsigned long btn3LastSendTime  = 0;

// // --- Флаг обновления дисплея ---
// bool displayNeedsUpdate = false;

// // --- Статус подключения к Windows ---
// bool displayConnected = false;

// // =====================================================
// // ФУНКЦИЯ: обновление дисплея
// //
// // Дисплей 128x64 пикселей, шрифт размера 1 = 6x8 пикселей на символ.
// // В одну строку влезает 21 символ, всего строк до 8 шт.
// //
// // МАКЕТ ЭКРАНА:
// // ┌─────────────────────┐  y=0
// // │ ESP32: PC CONNECTED │  заголовок (меняется)
// // ├─────────────────────┤  y=10 (линия)
// // │ D4 : PRESSED        │  y=16 состояние кнопки 1
// // │ D16: released       │  y=28 состояние кнопки 2
// // │ D17: released       │  y=40 состояние кнопки 3
// // │                     │
// // │ Windows: ONLINE     │  y=56 статус подключения
// // └─────────────────────┘  y=64
// // =====================================================

// void updateDisplay() {
//     display.clearDisplay();
//     display.setTextSize(1);

//     // --- Заголовок (строка 1) ---
//     display.setCursor(0, 0);
//     if (displayConnected) {
//         display.println(" ESP32: PC CONNECTED");
//     } else {
//         display.println("   ESP32 v2.0 READY");
//     }

//     // --- Разделитель ---
//     display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

//     // --- Кнопка 1 (D4) ---
//     // Формат: "D4 : PRESSED" или "D4 : released"
//     display.setCursor(0, 16);
//     display.print("D4 : ");
//     display.println(btn1Pressed ? "PRESSED" : "released");

//     // --- Кнопка 2 (D16) ---
//     display.setCursor(0, 28);
//     display.print("D16: ");
//     display.println(btn2Pressed ? "PRESSED" : "released");

//     // --- Кнопка 3 (D17) ---
//     display.setCursor(0, 40);
//     display.print("D17: ");
//     display.println(btn3Pressed ? "PRESSED" : "released");

//     // --- Статус подключения (последняя строка) ---
//     display.setCursor(0, 56);
//     display.println(displayConnected ? "Windows: ONLINE" : "Windows: offline");

//     // Отправляем буфер на физический экран по I2C
//     display.display();
// }

// // =====================================================
// // ФУНКЦИЯ: обработка одной кнопки с антидребезгом
// //
// // Принимает переменные кнопки по ссылке (&) —
// // изменения внутри функции меняют оригинальные переменные.
// // =====================================================

// void processButton(
//   int pin,
//   int &raw,
//   int &lastStable,
//   unsigned long &debounceStart,
//   bool &pressed,
//   unsigned long &lastSendTime,
//   const char* pressMsg,
//   const char* releaseMsg)
// {
//     int currentRaw = digitalRead(pin);

//     // Если сигнал изменился — начинаем отсчёт антидребезга заново
//     if (currentRaw != raw) {
//         raw           = currentRaw;
//         debounceStart = millis();
//     }

//     // Если сигнал стабилен дольше DEBOUNCE_MS и действительно изменился
//     if (millis() - debounceStart >= DEBOUNCE_MS && currentRaw != lastStable) {
//         lastStable = currentRaw;

//         // Защита от спама — не чаще BTN_SEND_MS
//         if (millis() - lastSendTime >= BTN_SEND_MS) {
//             lastSendTime = millis();

//             if (lastStable == LOW) {
//                 // LOW = кнопка нажата (пин соединён с GND)
//                 pressed = true;
//                 Serial.println(pressMsg);
//             } else {
//                 // HIGH = кнопка отпущена (внутренняя подтяжка вернула +3.3V)
//                 pressed = false;
//                 Serial.println(releaseMsg);
//             }

//             displayNeedsUpdate = true;
//         }
//     }
// }

// // =====================================================
// // setup() — выполняется один раз при старте
// // =====================================================

// void setup() {
//     // Настройка пинов
//     pinMode(LED_PIN, OUTPUT);
//     pinMode(BTN_1,   INPUT_PULLUP);  
//     pinMode(BTN_2,   INPUT_PULLUP);
//     pinMode(BTN_3,   INPUT_PULLUP);  

//     digitalWrite(LED_PIN, LOW);  // Светодиод выключен

//     // Запуск Serial
//     Serial.begin(115200);
//     delay(1000);
//     Serial.println("ESP32_READY");

//     // Инициализация дисплея
//     if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
//         Serial.println("Display init failed!");
//         while (true) {
//             digitalWrite(LED_PIN, !digitalRead(LED_PIN));
//             delay(200);
//         }
//     }

//     display.clearDisplay();
//     display.setTextColor(SSD1306_WHITE);
//     display.setTextSize(1);
//     updateDisplay();

//     Serial.println("Ready.");
// }

// // =====================================================
// // loop() — выполняется бесконечно
// //
// // Структура каждой итерации:
// //   1. Команды от Windows (Serial)
// //   2. Кнопка 1 D4  (антидребезг)
// //   3. Кнопка 2 D16 (антидребезг)
// //   4. Кнопка 3 D17 (антидребезг) 
// //   5. Обновление дисплея (если нужно)
// // =====================================================

// void loop() {

//     // ========== 1. КОМАНДЫ ОТ WINDOWS ==========
//     if (Serial.available() > 0) {
//         String command = Serial.readStringUntil('\n');
//         command.trim();

//         if (command == "TEST") {
//             // Проверка связи — Windows убеждается что это ESP32
//             Serial.println("ESP32_OK");
//         }
//         else if (command == "CONNECTED") {
//             // Windows подключилась — показываем на дисплее
//             displayConnected   = true;
//             displayNeedsUpdate = true;
//             Serial.println("CONNECTED_OK");
//         }
//         else if (command == "DISCONNECTED") {
//             // Windows отключилась
//             displayConnected   = false;
//             displayNeedsUpdate = true;
//         }
//     }

//     // ========== 2. КНОПКА 1 (D4) ==========
//     processButton(
//         BTN_1,
//         btn1Raw,
//         btn1LastStable,
//         btn1DebounceStart,
//         btn1Pressed,
//         btn1LastSendTime,
//         "BTN_D4:1",    // Нажата
//         "BTN_D4:0"     // Отпущена
//     );

//     // ========== 3. КНОПКА 2 (D16) ==========
//     processButton(
//         BTN_2,
//         btn2Raw,
//         btn2LastStable,
//         btn2DebounceStart,
//         btn2Pressed,
//         btn2LastSendTime,
//         "BTN_D16:1",   // Нажата
//         "BTN_D16:0"    // Отпущена
//     );

//     // ========== 4. КНОПКА 3 (D17) ==========
//     processButton(
//         BTN_3,
//         btn3Raw,
//         btn3LastStable,
//         btn3DebounceStart,
//         btn3Pressed,
//         btn3LastSendTime,
//         "BTN_D17:1",   // Нажата
//         "BTN_D17:0"    // Отпущена
//     );

//     // ========== 5. ОБНОВЛЕНИЕ ДИСПЛЕЯ ==========
//     if (displayNeedsUpdate) {
//         updateDisplay();
//         displayNeedsUpdate = false;
//     }

//     delay(1);
// }









