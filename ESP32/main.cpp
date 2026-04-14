// =====================================================
// ESP32 Dev Module + SSD1306 + две кнопки
// Связь с Windows программой через COM-порт (USB Serial)
//
// ЖЕЛЕЗО:
//   ESP32 Dev Module  — основная плата
//   SSD1306 128x64    — OLED дисплей
//   BTN_CALC  на D4   — кнопка запуска Calculator
//   BTN_PAINT на D16  — кнопка запуска MS Paint
//   LED       на D2   — встроенный светодиод ESP32
//
// ПРОТОКОЛ ОБЩЕНИЯ С WINDOWS:
//   Windows → ESP32:  "TEST\n"
//   ESP32   → Windows: "ESP32_READY" (при старте)
//                      "ESP32_OK"    (ответ на TEST)
//                      "BTN_CALC:1"  (кнопка D4 нажата)
//                      "BTN_CALC:0"  (кнопка D4 отпущена)
//                      "BTN_PAINT:1" (кнопка D16 нажата)
//                      "BTN_PAINT:0" (кнопка D16 отпущена)
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

#define LED_PIN       2    // Встроенный светодиод ESP32
#define BTN_CALC      4    // Кнопка D4 — запуск Calculator
#define BTN_PAINT     16   // Кнопка D16 — запуск MS Paint

#define DEBOUNCE_MS   5    // Время антидребезга (мс)
#define BTN_SEND_MS   50   // Минимальный интервал между отправками (мс)

// =====================================================
// ОБЪЕКТ ДИСПЛЕЯ
// =====================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// ПЕРЕМЕННЫЕ СОСТОЯНИЯ КНОПОК
// Для каждой кнопки — свой независимый набор переменных
// =====================================================

// --- Кнопка CALC (D4) ---
int           calcRaw          = HIGH;  // Сырое (с дребезгом) состояние пина
int           calcLastStable   = HIGH;  // Подтверждённое состояние после антидребезга
unsigned long calcDebounceStart = 0;   // Момент когда сигнал начал меняться
bool          calcPressed       = false; // Логическое состояние для дисплея
unsigned long calcLastSendTime  = 0;   // Время последней отправки в Serial

// --- Кнопка PAINT (D16) ---
int           paintRaw          = HIGH;
int           paintLastStable   = HIGH;
unsigned long paintDebounceStart = 0;
bool          paintPressed       = false;
unsigned long paintLastSendTime  = 0;

// --- Флаг обновления дисплея ---
bool displayNeedsUpdate = false;

// УТОЧНИТЬ ДЛЯ ЧЕГО ЭТА ПЕРЕМЕННАЯ
bool displayConnected = false;

// =====================================================
// ФУНКЦИЯ: обновление дисплея
// =====================================================

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);

    display.setCursor(0, 0);
    // Заголовок меняется в зависимости от состояния подключения
    if (displayConnected) {
        display.println(" ESP32: PC CONNECTED");
    } else {
        display.println("  ESP32 CALC & PAINT");
    }
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 18);
    display.print("CALC  (D4):  ");
    display.println(calcPressed ? "PRESSED" : "released");

    display.setCursor(0, 36);
    display.print("PAINT (D16): ");
    display.println(paintPressed ? "PRESSED" : "released");

    display.setCursor(0, 54);
    // Внизу тоже показываем статус подключения
    display.println(displayConnected ? "Windows: ONLINE" : "Windows: offline");

    display.display();
}

// =====================================================
// ФУНКЦИЯ: обработка одной кнопки с антидребезгом
//
// Принимает все переменные кнопки по ссылке (&) —
// работает напрямую с оригинальными переменными.
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

  // Если сигнал стабилен дольше DEBOUNCE_MS — это настоящее нажатие
  if (millis() - debounceStart >= DEBOUNCE_MS && currentRaw != lastStable) {
    lastStable = currentRaw;

    // Защита от спама — не чаще BTN_SEND_MS
    if (millis() - lastSendTime >= BTN_SEND_MS) {
      lastSendTime = millis();

      if (lastStable == LOW) {
        pressed = true;
        Serial.println(pressMsg);   // например "BTN_CALC:1"
      } else {
        pressed = false;
        Serial.println(releaseMsg); // например "BTN_CALC:0"
      }

      displayNeedsUpdate = true;
    }
  }
}

// =====================================================
// setup() — выполняется один раз при старте
// =====================================================

void setup() {
  pinMode(LED_PIN,   OUTPUT);
  pinMode(BTN_CALC,  INPUT_PULLUP);
  pinMode(BTN_PAINT, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);

  // Ждём пока USB-соединение установится.
  // Важно: Windows программа отправит TEST только после
  // того как порт откроется — ESP32 должна быть готова.
  delay(1000);

  // Сообщаем о готовности — Windows программа ищет эту строку
  // среди первых сообщений при подключении
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
//   1. Чтение команд от Windows (Serial)
//   2. Обработка кнопки CALC  (антидребезг)
//   3. Обработка кнопки PAINT (антидребезг)
//   4. Обновление дисплея     (если нужно)
// =====================================================

void loop() {

  // =====================================================
  // 1. КОМАНДЫ ОТ WINDOWS ПРОГРАММЫ
  //
  // Windows отправляет команды заканчивающиеся на \n.
  // readStringUntil('\n') читает всё до символа \n.
  // trim() убирает лишние пробелы и \r по краям.
  // =====================================================
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    // ---------- TEST ----------
    // Windows программа отправляет TEST при подключении
    // чтобы убедиться что на другом конце именно наш ESP32.
    // Мы должны ответить "ESP32_OK" — именно эту строку
    // ищет CheckESP32() в Windows программе.
    if (command == "TEST") {
      Serial.println("ESP32_OK");
    }
        // Новое: Windows сообщает что подключение установлено
    else if (command == "CONNECTED") {
        // Показываем на дисплее что связь с Windows активна
        displayConnected = true;
        displayNeedsUpdate = true;
        Serial.println("CONNECTED_OK");
    }

    // Новое: Windows сообщает что отключилась
    else if (command == "DISCONNECTED") {
        displayConnected = false;
        displayNeedsUpdate = true;
    }

    // Здесь в будущем можно добавить другие команды от Windows
    // по образцу: else if (command == "XXX") { ... }
  }

  // =====================================================
  // 2. КНОПКА CALC (D4)
  // =====================================================
  processButton(
    BTN_CALC,
    calcRaw,
    calcLastStable,
    calcDebounceStart,
    calcPressed,
    calcLastSendTime,
    "BTN_CALC:1",
    "BTN_CALC:0"
  );

  // =====================================================
  // 3. КНОПКА PAINT (D16)
  // =====================================================
  processButton(
    BTN_PAINT,
    paintRaw,
    paintLastStable,
    paintDebounceStart,
    paintPressed,
    paintLastSendTime,
    "BTN_PAINT:1",
    "BTN_PAINT:0"
  );

  // =====================================================
  // 4. ОБНОВЛЕНИЕ ДИСПЛЕЯ
  // =====================================================
  if (displayNeedsUpdate) {
    updateDisplay();
    displayNeedsUpdate = false;
  }

  delay(1);
}
