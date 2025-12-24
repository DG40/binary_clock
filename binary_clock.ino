#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

// === МАТРИЦА ===
int pinCS = 9;
Max72xxPanel matrix = Max72xxPanel(pinCS, 4, 1);

// === RTC ===
ThreeWire myWire(6, 7, 5);
RtcDS1302<ThreeWire> Rtc(myWire);

// === КНОПКА ===
const int buttonPin = 4;
int buttonState;
int lastButtonState = HIGH;
int reading;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 70;

// === РЕЖИМЫ ===
int displayMode = 0;
int editHours = 0;
int editMinutes = 0;
int editSeconds = 0;
int editStep = 0;
unsigned long lastClickTime = 0;
bool blinkState = true;
unsigned long lastBlinkTime = 0;
bool isHoldingInBinaryMode = false;

// === ПОВОРОТ ЭКРАНА ===
bool isScreenRotated = false;
bool actionProcessed = false;  // ФЛАГ: была ли обработана акция при этом нажатии

// === ЗМЕЙКА (СОКРАЩЕНО ДО 64 СЕГМЕНТОВ) ===
int snakeX[64];
int snakeY[64];
int snakeLen = 3;
int foodX = 16;
int foodY = 4;
unsigned long lastMoveTime = 0;
int dirX = 1;
int dirY = 0;
int nextDirX = 1;
int nextDirY = 0;
int moveCounter = 0;
unsigned long crashTime = 0;
bool isCrashing = false;
bool snakeInitialized = false;

// === БЕГУЩАЯ СТРОКА ===
unsigned long lastScrollTime = 0;
int scrollPos = 0;
const char scrollText[] = "TrueConf";
const int textLength = 8;

// === АБСТРАКТНЫЕ ПИКСЕЛИ ===
byte abstractGrid[8][32];
unsigned long lastAbstractUpdateTime = 0;

// === ШРИФТ ЦИФР В FLASH ===
const byte smallDigits[10][7] PROGMEM = {
    {0b0110, 0b1001, 0b1001, 0b1001, 0b1001, 0b1001, 0b0110},
    {0b0010, 0b0110, 0b0010, 0b0010, 0b0010, 0b0010, 0b0111},
    {0b0110, 0b1001, 0b0001, 0b0010, 0b0100, 0b1000, 0b1111},
    {0b0110, 0b1001, 0b0001, 0b0110, 0b0001, 0b1001, 0b0110},
    {0b1000, 0b1100, 0b1010, 0b1001, 0b1111, 0b0001, 0b0001},
    {0b1111, 0b1000, 0b1000, 0b1110, 0b0001, 0b1001, 0b0110},
    {0b0110, 0b1001, 0b1000, 0b1110, 0b1001, 0b1001, 0b0110},
    {0b1111, 0b0001, 0b0010, 0b0100, 0b1000, 0b1000, 0b1000},
    {0b0110, 0b1001, 0b1001, 0b0110, 0b1001, 0b1001, 0b0110},
    {0b0110, 0b1001, 0b1001, 0b0111, 0b0001, 0b1001, 0b0110}
};

// === ШРИФТ БЕГУЩЕЙ СТРОКИ "TrueConf" ===
const byte scrollingFont[8][7] PROGMEM = {
    {0b00000, 0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},
    {0b00000, 0b00000, 0b11110, 0b10001, 0b10000, 0b10000, 0b10000},
    {0b00000, 0b00000, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},
    {0b00000, 0b00000, 0b01110, 0b10001, 0b11111, 0b10000, 0b01111},
    {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110},
    {0b00000, 0b00000, 0b01110, 0b10001, 0b10001, 0b10001, 0b01110},
    {0b00000, 0b00000, 0b11110, 0b10001, 0b10001, 0b10001, 0b10001},
    {0b00110, 0b01000, 0b11100, 0b01000, 0b01000, 0b01000, 0b01000}
};

// === ПРОТОТИПЫ ФУНКЦИЙ ===
void drawClockDisplay();
void drawBinaryClockDisplay();
void drawEditDisplay();
void drawAbstractPattern();
void drawScrollingText();
void drawScrollingChar(int charIdx, int xStart);
void drawSmallNumber(int value, int xStart, int yStart, bool fullBrightness = true);
void drawSmallDigit(int digit, int xStart, int yStart, bool dimmed = false);
void drawColon(int xStart, int yStart);
void drawBinaryRow(int value, int rowY, int maxBits);
void drawAbstractPixels();
void initializeAbstractPixels();
void updateAbstractPixels();
void drawPixelRotated(int x, int y);
void enterEditMode();
void incrementEditValue();
void confirmAndNextStep();
void print2digits(int number);
void resetSnakeGame();
void generateFood();

void setup() {
    Serial.begin(9600);
    Rtc.Begin();
    pinMode(buttonPin, INPUT_PULLUP);
    buttonState = digitalRead(buttonPin);
    
    matrix.setIntensity(7);
    for (int i = 0; i < 4; i++) {
        matrix.setRotation(i, 1);
    }
    matrix.fillScreen(LOW);
    
    resetSnakeGame();
    initializeAbstractPixels();
    scrollPos = 0;
    lastScrollTime = millis();
    
    Serial.println("Часы с абстрактными пикселями и поворотом из режима бегущей строки.");
}

void loop() {
    // === ОБРАБОТКА КНОПКИ ===
    reading = digitalRead(buttonPin);
    
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            
            if (buttonState == LOW) {
                lastClickTime = millis();
                isHoldingInBinaryMode = false;
                actionProcessed = false;
            } else {
                unsigned long pressDuration = millis() - lastClickTime;
                
                // ТОЛЬКО ОДИН РАЗ обрабатываем действие при отпускании!
                if (!actionProcessed) {
                    actionProcessed = true;
                    
                    if (displayMode != 2) {
                        if (pressDuration > 500) {
                            if (displayMode == 0) {
                                enterEditMode();
                            } else if (displayMode == 4) {
                                isScreenRotated = !isScreenRotated;
                            }
                        } else if (pressDuration > debounceDelay) {
                            // Быстрое нажатие - переключение режима
                            displayMode++;
                            if (displayMode > 5) displayMode = 0;
                            if (displayMode == 2) displayMode = 3;
                        }
                    } else {
                        if (pressDuration > 500) {
                            confirmAndNextStep();
                        } else if (pressDuration > debounceDelay) {
                            incrementEditValue();
                        }
                    }
                }
            }
        }
    }
    lastButtonState = reading;
    
    // === ОТРИСОВКА МАТРИЦЫ ===
    matrix.fillScreen(LOW);
    
    if (millis() - lastBlinkTime > 300) {
        lastBlinkTime = millis();
        blinkState = !blinkState;
    }
    
    switch(displayMode) {
        case 0: drawClockDisplay(); break;
        case 1: drawAbstractPattern(); break;
        case 2: drawEditDisplay(); break;
        case 3: drawBinaryClockDisplay(); break;
        case 4: drawScrollingText(); break;
        case 5: drawAbstractPixels(); break;
    }
    
    matrix.write();
    delay(50);
}


// === ФУНКЦИЯ ПОВОРОТА ПИКСЕЛЯ ===
void drawPixelRotated(int x, int y) {
    if (isScreenRotated) {
        matrix.drawPixel(31 - x, 7 - y, HIGH);
    } else {
        matrix.drawPixel(x, y, HIGH);
    }
}

void enterEditMode() {
    displayMode = 2;
    RtcDateTime now = Rtc.GetDateTime();
    editHours = now.Hour();
    editMinutes = now.Minute();
    editSeconds = now.Second();
    editStep = 0;
}

void incrementEditValue() {
    if (editStep == 0) {
        editHours = (editHours + 1) % 24;
    } else if (editStep == 1) {
        editMinutes = (editMinutes + 1) % 60;
    } else {
        editSeconds = (editSeconds + 1) % 60;
    }
}

void confirmAndNextStep() {
    if (editStep < 2) {
        editStep++;
    } else {
        RtcDateTime dt(2025, 12, 21, editHours, editMinutes, editSeconds);
        Rtc.SetDateTime(dt);
        displayMode = 0;
    }
}

void drawClockDisplay() {
    RtcDateTime now = Rtc.GetDateTime();
    drawSmallNumber(now.Hour(), 0, 0);
    drawColon(8, 0);
    drawSmallNumber(now.Minute(), 12, 0);
    drawColon(20, 0);
    drawSmallNumber(now.Second(), 24, 0);
    
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime > 1000) {
        lastPrintTime = millis();
        Serial.print("Время: ");
        print2digits(now.Hour());
        Serial.print(":");
        print2digits(now.Minute());
        Serial.print(":");
        print2digits(now.Second());
        Serial.println();
    }
}

void drawBinaryClockDisplay() {
    RtcDateTime now = Rtc.GetDateTime();
    
    unsigned long currentHoldTime = millis() - lastClickTime;
    if (buttonState == LOW && currentHoldTime > 500) {
        isHoldingInBinaryMode = true;
        drawSmallNumber(now.Hour(), 0, 0);
        drawColon(8, 0);
        drawSmallNumber(now.Minute(), 12, 0);
        drawColon(20, 0);
        drawSmallNumber(now.Second(), 24, 0);
    } else {
        isHoldingInBinaryMode = false;

        // === ЛЕВЫЙ ТРЕУГОЛЬНИК ◀ ===
        int startX = 5;
        drawPixelRotated(startX, 0);
        drawPixelRotated(startX + 1, 1);
        drawPixelRotated(startX, 2);
        drawPixelRotated(startX, 5);
        drawPixelRotated(startX + 1, 6);
        drawPixelRotated(startX, 7);

        drawBinaryRow(now.Hour(), 0, 6);
        drawBinaryRow(now.Minute(), 3, 6);
        drawBinaryRow(now.Second(), 6, 6);
    }
}

void drawBinaryRow(int value, int rowY, int maxBits) {
    int startX = 10;
    for (int bit = maxBits - 1; bit >= 0; bit--) {
        int xPos = startX + (maxBits - 1 - bit) * 3;
        if (value & (1 << bit)) {
            for (int x = 0; x < 2; x++) {
                if (rowY + 1 < 8) {
                    drawPixelRotated(xPos + x, rowY);
                    drawPixelRotated(xPos + x, rowY + 1);
                }
            }
        }
    }
}

void drawEditDisplay() {
    if (editStep == 0 && blinkState) {
        drawSmallNumber(editHours, 0, 0, false);
    } else {
        drawSmallNumber(editHours, 0, 0);
    }
    drawColon(8, 0);
    
    if (editStep == 1 && blinkState) {
        drawSmallNumber(editMinutes, 12, 0, false);
    } else {
        drawSmallNumber(editMinutes, 12, 0);
    }
    drawColon(20, 0);
    
    if (editStep == 2 && blinkState) {
        drawSmallNumber(editSeconds, 24, 0, false);
    } else {
        drawSmallNumber(editSeconds, 24, 0);
    }
}

// === БЕГУЩАЯ СТРОКА ===
void drawScrollingText() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastScrollTime > 40) {
        lastScrollTime = currentTime;
        scrollPos++;
        if (scrollPos > 32 + 32 + 5 * textLength) {
            scrollPos = 0;
        }
    }
    
    for (int charIdx = 0; charIdx < textLength; charIdx++) {
        int charX = 32 + charIdx * 6 - scrollPos;
        
        if (charX > -6 && charX < 32) {
            drawScrollingChar(charIdx, charX);
        }
    }
}

void drawScrollingChar(int charIdx, int xStart) {
    for (int row = 0; row < 7; row++) {
        byte charData = pgm_read_byte(&scrollingFont[charIdx][row]);
        for (int col = 0; col < 5; col++) {
            if (charData & (1 << (4 - col))) {
                int pixelX = xStart + col;
                int pixelY = row;
                if (pixelX >= 0 && pixelX < 32 && pixelY >= 0 && pixelY < 8) {
                    drawPixelRotated(pixelX, pixelY);
                }
            }
        }
    }
}

// === АБСТРАКТНЫЕ ПИКСЕЛИ ===
void initializeAbstractPixels() {
    // Инициализируем сетку пикселей (50% включены)
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 32; x++) {
            abstractGrid[y][x] = random(100) < 50 ? 1 : 0;
        }
    }
    lastAbstractUpdateTime = millis();
}

// === ГЛОБАЛЬНАЯ ПЕРЕМЕННАЯ ===
int abstractMode = 0;  // 0=нормальный, 1=бурный, 2=хаос, 3=лед
bool longPressDetected = false;  // ФЛАГ: был ли долгий пресс

void drawAbstractPixels() {
    unsigned long currentTime = millis();
    
    unsigned long currentHoldTime = millis() - lastClickTime;
    bool isButtonHeld = (buttonState == LOW);
    
    // === ОТМЕЧАЕМ ДОЛГИЙ ПРЕСС ===
    if (isButtonHeld && currentHoldTime > 500 && !longPressDetected) {
        longPressDetected = true;  // Первый раз >500мс - отмечаем
    }
    
    // === ОТПУСКАНИЕ ПОСЛЕ ДОЛГОГО ПРЕССА ===
    static bool wasLongPressReleased = false;
    if (!isButtonHeld && longPressDetected && !wasLongPressReleased) {
        // ПЕРЕКЛЮЧЕНИЕ РЕЖИМА!
        abstractMode = (abstractMode + 1) % 5;
        longPressDetected = false;
        wasLongPressReleased = true;
    }
    
    // СБРОС при новом нажатии
    if (isButtonHeld && currentHoldTime < 50) {
        longPressDetected = false;
        wasLongPressReleased = false;
    }
    
    // ОБНОВЛЯЕМ СОГЛАСНО РЕЖИМУ
    if (currentTime - lastAbstractUpdateTime > 20) {
        lastAbstractUpdateTime = currentTime;
        updateAbstractPixelsByMode(abstractMode);
    }
    
    // РИСУЕМ ПИКСЕЛИ
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 32; x++) {
            if (abstractGrid[y][x]) {
                drawPixelRotated(x, y);
            }
        }
    }
}


// === ФУНКЦИЯ ДЛЯ РЕЖИМОВ ===
void updateAbstractPixelsByMode(int mode) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 32; x++) {
            int chance = random(100);
            
            switch(mode) {
                case 0:
                    if (abstractGrid[y][x] == 1) {
                        if (chance < 95) abstractGrid[y][x] = 0;
                    } else {
                        if (chance < 5) abstractGrid[y][x] = 1;
                    }
                    break;
                    
                case 1:
                    if (abstractGrid[y][x] == 1) {
                        if (chance < 90) abstractGrid[y][x] = 0;
                    } else {
                        if (chance < 10) abstractGrid[y][x] = 1;
                    }
                    break;
                    
                case 2:
                    if (abstractGrid[y][x] == 1) {
                        if (chance < 80) abstractGrid[y][x] = 0;
                    } else {
                        if (chance < 20) abstractGrid[y][x] = 1;
                    }
                    break;
                    
                case 3:
                    if (abstractGrid[y][x] == 1) {
                        if (chance < 60) abstractGrid[y][x] = 0;
                    } else {
                        if (chance < 40) abstractGrid[y][x] = 1;
                    }
                    break;
                case 4:
                    if (abstractGrid[y][x] == 1) {
                        if (chance < 50) abstractGrid[y][x] = 0;
                    } else {
                        if (chance < 50) abstractGrid[y][x] = 1;
                    }
                    break;
            }
        }
    }
}

// === ЗМЕЙКА ===
void resetSnakeGame() {
    snakeLen = 3;
    snakeX[0] = 16;
    snakeY[0] = 4;
    snakeX[1] = 15;
    snakeY[1] = 4;
    snakeX[2] = 14;
    snakeY[2] = 4;
    
    moveCounter = 0;
    dirX = 1;
    dirY = 0;
    nextDirX = 1;
    nextDirY = 0;
    isCrashing = false;
    snakeInitialized = true;
    
    foodX = 8;
    foodY = 2;
    lastMoveTime = millis();
}

void generateFood() {
    unsigned long currentTime = millis();
    int attempts = 0;
    const int MAX_ATTEMPTS = 100;
    
    do {
        foodX = (currentTime / 100 + moveCounter * 7 + attempts) % 32;
        foodY = (currentTime / 200 + moveCounter * 13 + attempts) % 8;
        
        boolean onSnake = false;
        for (int i = 0; i < snakeLen; i++) {
            if (snakeX[i] == foodX && snakeY[i] == foodY) {
                onSnake = true;
                break;
            }
        }
        
        if (!onSnake) {
            break;
        }
        
        attempts++;
    } while (attempts < MAX_ATTEMPTS);
    
    if (attempts >= MAX_ATTEMPTS) {
        foodX = 8;
        foodY = 2;
    }
}

void drawAbstractPattern() {
    unsigned long currentTime = millis();
    
    if (!snakeInitialized) {
        resetSnakeGame();
    }
    
    unsigned long currentHoldTime = millis() - lastClickTime;
    bool isButtonHeld = (buttonState == LOW && currentHoldTime > 500);
    
    if (isButtonHeld) {
        // === РАМКА ПРИ ЗАЖАТИИ КНОПКИ ===
        drawSnakeFrame(currentTime);
        
        // Рисуем змейку поверх рамки
        for (int i = 0; i < snakeLen; i++) {
            drawPixelRotated(snakeX[i], snakeY[i]);
        }
        drawPixelRotated(foodX, foodY);
        return;
    }
    
    if (isCrashing) {
        if (currentTime - crashTime > 800) {
            resetSnakeGame();
        } else {
            if (((currentTime - crashTime) / 100) % 2 == 0) {
                for (int i = 0; i < snakeLen; i++) {
                    drawPixelRotated(snakeX[i], snakeY[i]);
                }
                drawPixelRotated(foodX, foodY);
            }
            return;
        }
    }
    
    // === ОСНОВНАЯ ЛОГИКА ЗМЕЙКИ ===
    if (currentTime - lastMoveTime > 200) {
        lastMoveTime = currentTime;
        
        if (snakeX[0] < foodX && dirX == 0) {
            nextDirX = 1;
            nextDirY = 0;
        } else if (snakeX[0] > foodX && dirX == 0) {
            nextDirX = -1;
            nextDirY = 0;
        } else if (snakeY[0] < foodY && dirY == 0) {
            nextDirX = 0;
            nextDirY = 1;
        } else if (snakeY[0] > foodY && dirY == 0) {
            nextDirX = 0;
            nextDirY = -1;
        }
        
        if (snakeLen > 1) {
            if (snakeX[1] != snakeX[0] + nextDirX || snakeY[1] != snakeY[0] + nextDirY) {
                dirX = nextDirX;
                dirY = nextDirY;
            }
        } else {
            dirX = nextDirX;
            dirY = nextDirY;
        }
        
        int newX = snakeX[0] + dirX;
        int newY = snakeY[0] + dirY;
        
        if (newX < 0) newX = 31;
        if (newX >= 32) newX = 0;
        if (newY < 0) newY = 7;
        if (newY >= 8) newY = 0;
        
        boolean hitSelf = false;
        for (int i = 0; i < snakeLen; i++) {
            if (snakeX[i] == newX && snakeY[i] == newY) {
                hitSelf = true;
                break;
            }
        }
        
        if (hitSelf) {
            isCrashing = true;
            crashTime = currentTime;
            return;
        }
        
        for (int i = snakeLen; i > 0; i--) {
            snakeX[i] = snakeX[i - 1];
            snakeY[i] = snakeY[i - 1];
        }
        snakeX[0] = newX;
        snakeY[0] = newY;
        
        if (snakeX[0] == foodX && snakeY[0] == foodY) {
            snakeLen++;
            moveCounter++;
            
            if (snakeLen >= 64) {
                isCrashing = true;
                crashTime = currentTime;
                return;
            }
            
            generateFood();
        }
    }
    
    for (int i = 0; i < snakeLen; i++) {
        drawPixelRotated(snakeX[i], snakeY[i]);
    }
    
    if ((currentTime / 200) % 2 == 0) {
        drawPixelRotated(foodX, foodY);
    }
}

// === НОВАЯ ФУНКЦИЯ РАМКИ ===
void drawSnakeFrame(unsigned long currentTime) {
    // Пиксель через пиксель (анимация бегущей рамки)
    bool frameOn = (currentTime / 100) % 2;  // Мигание каждые 100мс
    
    // ВЕРХНЯЯ ГРАНИЦА (y=0)
    for (int x = 0; x < 32; x++) {
        if ((x % 2 == 0) == frameOn) {
            drawPixelRotated(x, 0);
        }
    }
    
    // НИЖНЯЯ ГРАНИЦА (y=7)
    for (int x = 0; x < 32; x++) {
        if ((x % 2 == 0) == frameOn) {
            drawPixelRotated(x, 7);
        }
    }
    
    // ЛЕВАЯ ГРАНИЦА (x=0)
    for (int y = 1; y < 7; y++) {
        if ((y % 2 == 0) == frameOn) {
            drawPixelRotated(0, y);
        }
    }
    
    // ПРАВАЯ ГРАНИЦА (x=31)
    for (int y = 1; y < 7; y++) {
        if ((y % 2 == 0) == frameOn) {
            drawPixelRotated(31, y);
        }
    }
}


void drawSmallNumber(int value, int xStart, int yStart, bool fullBrightness) {
    drawSmallDigit(value / 10, xStart, yStart, !fullBrightness);
    drawSmallDigit(value % 10, xStart + 4, yStart, !fullBrightness);
}

void drawSmallDigit(int digit, int xStart, int yStart, bool dimmed) {
    for (int row = 0; row < 7; row++) {
        byte rowData = pgm_read_byte(&smallDigits[digit][row]);
        for (int col = 0; col < 4; col++) {
            if (rowData & (1 << (3 - col))) {
                if (!dimmed || (row + col) % 2 == 0) {
                    drawPixelRotated(xStart + col, yStart + row);
                }
            }
        }
    }
}

void drawColon(int xStart, int yStart) {
    drawPixelRotated(xStart + 1, yStart + 2);
    drawPixelRotated(xStart + 1, yStart + 5);
}

void print2digits(int number) {
    if (number < 10) Serial.print('0');
    Serial.print(number);
}
