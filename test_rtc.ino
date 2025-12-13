#include <ThreeWire.h>
#include <RtcDS1302.h>

ThreeWire myWire(6, 7, 5);
RtcDS1302<ThreeWire> Rtc(myWire);

const int buttonPin = 4;

// Переменные для УЛУЧШЕННОГО антидребезга
int buttonState;             // Текущее устойчивое состояние кнопки
int lastButtonState = HIGH;  // Предыдущее устойчивое состояние
int reading;                 // Текущее "сырое" чтение с пина
unsigned long lastDebounceTime = 0; // Момент последнего изменения "сырого" сигнала
const unsigned long debounceDelay = 70; // Время стабилизации (увеличено до 70 мс)

// Переменная для режима цвета
int colorMode = 0;

void setup() {
  Serial.begin(9600);
  Rtc.Begin();
  pinMode(buttonPin, INPUT_PULLUP);
  buttonState = digitalRead(buttonPin); // Инициализируем начальное состояние
  Serial.println("Система запущена. Нажимайте кнопку для смены режима.");
}

void loop() {
  // === 1. УЛУЧШЕННОЕ ЧТЕНИЕ И ОБРАБОТКА КНОПКИ ===
  reading = digitalRead(buttonPin);

  // Если "сырое" значение изменилось (возможно, начало нажатия или дребезг)
  if (reading != lastButtonState) {
    lastDebounceTime = millis(); // Сбрасываем таймер стабилизации
  }

  // Проверяем, прошло ли достаточно времени с момента последнего изменения
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Если за время debounceDelay состояние больше не "прыгало",
    // значит, мы получили новое УСТОЙЧИВОЕ состояние кнопки.
    if (reading != buttonState) {
      buttonState = reading;

      // Срабатывание по НАЖАТИЮ (переход с HIGH на LOW)
      if (buttonState == LOW) {
        colorMode++;
        if (colorMode > 2) {
          colorMode = 0;
        }
        Serial.print("[КНОПКА] Режим изменен на: ");
        Serial.println(colorMode);
      }
    }
  }
  // Сохраняем "сырое" состояние для сравнения в следующем цикле
  lastButtonState = reading;

  // === 2. ВЫВОД ВРЕМЕНИ С RTC (реже, чтобы не засорять монитор) ===
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime > 1000) {
    lastPrintTime = millis();
    RtcDateTime now = Rtc.GetDateTime();
    Serial.print("Время: ");
    print2digits(now.Hour());
    Serial.print(":");
    print2digits(now.Minute());
    Serial.print(":");
    print2digits(now.Second());
    Serial.print(" | Режим: ");
    Serial.println(colorMode);
  }

  // Небольшая задержка для стабильности
  delay(10);
}

// Вспомогательная функция для красивого вывода времени (05, а не 5)
void print2digits(int number) {
  if (number < 10) {
    Serial.print('0');
  }
  Serial.print(number);
}
