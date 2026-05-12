#include <SoftwareSerial.h>

SoftwareSerial Bluetooth(10, 11);

// ===================== الإعدادات الأساسية =====================
int tonePin = 2;
int toneFreq = 1000;
int ledPin = 13;
int ledNotPin = 12;
int touchPin = 8;
int debounceDelay = 30;

// ===================== أزرار وليدات المستويات =====================
int btnLevel[4] = {3, 4, 5, 6};
int ledLevel[4] = {7, 9, A0, A1};

// ===================== إعدادات المورس =====================
int dotLength = 240;
int dotSpace = dotLength;
int dashLength = dotLength * 3;
int letterSpace = dotLength * 3;
int wordSpace = dotLength * 7;
float wpm = 1200. / dotLength;

int t1, t2, onTime, gap;
bool newLetter, newWord, letterFound, keyboardText;
int lineLength = 0;
int maxLineLength = 20;

char* letters[] = {".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--.."};
char* numbers[] = {"-----",".----","..---","...--","....-",".....","-....","--...","---..","----."};

String dashSeq = "";
char keyLetter, ch;
int i;

// ===================== نظام المستويات =====================
int currentLevel = 0;
int tapCount = 0;
unsigned long lastTapTime = 0;
unsigned long tapWindowMs = 1000;
bool tapPending = false;
bool lastTouchState = LOW;
unsigned long lastDebounceTime = 0;

// ===================== الرسائل (4 مستويات × 4 رسائل) =====================
const char* messages[4][4] = {
  // 🔴 Level 1 — Critical
  {
    "SOS IMMEDIATE DANGER",
    "NEED MEDICAL ASSISTANCE NOW",
    "ACTIVE FIRE AT MY LOCATION",
    "I AM TRAPPED CALL FOR HELP"
  },
  // 🟠 Level 2 — Urgent
  {
    "I AM LOST SEND GPS LOCATION",
    "VEHICLE BREAKDOWN NEED HELP",
    "LOST CONNECTION NEED GUIDANCE",
    "MINOR INJURY NEED SUPPORT"
  },
  // 🟡 Level 3 — Alert
  {
    "ARRIVED SAFELY AT LOCATION",
    "MOVING NOW FOLLOW MY TRACK",
    "PLAN CHANGED AWAIT UPDATE",
    "LOW BATTERY GOING OFFLINE SOON"
  },
  // 🟢 Level 4 — Info
  {
    "STATUS STABLE NO ISSUES",
    "I AM ON MY WAY",
    "MISSION COMPLETED SUCCESSFULLY",
    "WILL CALL YOU BACK LATER"
  }
};

// ===================== دوال مساعدة =====================
void setLeds(int state) {
  digitalWrite(ledPin, state);
  digitalWrite(ledNotPin, !state);
}

void setLevelLed(int level) {
  for (int i = 0; i < 4; i++) digitalWrite(ledLevel[i], LOW);
  if (level > 0) digitalWrite(ledLevel[level - 1], HIGH);
}

void flashDot() {
  setLeds(HIGH); tone(tonePin, toneFreq);
  delay(dotLength);
  setLeds(LOW); noTone(tonePin);
  delay(dotSpace);
}

void flashDash() {
  setLeds(HIGH); tone(tonePin, toneFreq);
  delay(dashLength);
  setLeds(LOW); noTone(tonePin);
  delay(dotSpace);
}

void flashSequence(char* sequence) {
  int i = 0;
  while (sequence[i] != '\0') {
    if (sequence[i] == '.') flashDot();
    else if (sequence[i] == '-') flashDash();
    i++;
  }
}

void sendMessage(const char* msg) {
  Serial.print(">> Sending: "); Serial.println(msg);
  Bluetooth.print("MSG: "); Bluetooth.println(msg);

  int j = 0;
  while (msg[j] != '\0') {
    char c = msg[j];
    if (c >= 'a' && c <= 'z') c -= 32;

    if (c >= 'A' && c <= 'Z') {
      flashSequence(letters[c - 'A']);
      delay(letterSpace);
    } else if (c >= '0' && c <= '9') {
      flashSequence(numbers[c - '0']);
      delay(letterSpace);
    } else if (c == ' ') {
      delay(wordSpace);
    }
    j++;
  }
}

// ===================== الإعداد =====================
void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(ledNotPin, OUTPUT);
  pinMode(tonePin, OUTPUT);
  pinMode(touchPin, INPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(btnLevel[i], INPUT_PULLUP);
    pinMode(ledLevel[i], OUTPUT);
    digitalWrite(ledLevel[i], LOW);
  }

  Serial.begin(9600);
  Bluetooth.begin(9600);

  tone(tonePin, toneFreq);
  setLeds(HIGH);
  delay(1000);
  setLeds(LOW);
  noTone(tonePin);

  Serial.println("System Ready");
  Bluetooth.println("Bluetooth Ready!");

  newLetter = false;
  newWord = false;
  keyboardText = false;
}

// ===================== الحلقة الرئيسية =====================
void loop() {

  // --- فحص أزرار المستويات ---
  for (int i = 0; i < 4; i++) {
    if (digitalRead(btnLevel[i]) == LOW) {
      delay(debounceDelay);
      if (digitalRead(btnLevel[i]) == LOW) {

        // لو نفس الزرار اتضغط تاني → اخرج من المستوى
        if (currentLevel == i + 1) {
          currentLevel = 0;
          tapCount = 0;
          tapPending = false;
          setLevelLed(0);
          Serial.println("Exited Level — Manual Mode");
          Bluetooth.println("Exited Level — Manual Mode");
        } 
        // لو زرار مختلف → ادخل المستوى الجديد
        else {
          currentLevel = i + 1;
          tapCount = 0;
          tapPending = false;
          setLevelLed(currentLevel);
          Serial.print("Level: "); Serial.println(currentLevel);
          Bluetooth.print("Level: "); Bluetooth.println(currentLevel);
        }

        while (digitalRead(btnLevel[i]) == LOW);
      }
    }
  }

  // --- استقبال البلوتوث والسيريال ---
  if (Serial.available() > 0 || Bluetooth.available() > 0) {
    if (Bluetooth.available() > 0) ch = Bluetooth.read();
    else ch = Serial.read();

    if (ch >= 'a' && ch <= 'z') ch -= 32;

    if (ch >= 'A' && ch <= 'Z') {
      Serial.print(ch); Bluetooth.print(ch);
      flashSequence(letters[ch - 'A']);
      delay(letterSpace);
    } else if (ch >= '0' && ch <= '9') {
      Serial.print(ch); Bluetooth.print(ch);
      flashSequence(numbers[ch - '0']);
      delay(letterSpace);
    } else if (ch == ' ') {
      Serial.print("_"); Bluetooth.print("_");
      delay(wordSpace);
    }
  }

  // --- منطق التاتش سينسور ---
  bool touchState = digitalRead(touchPin);

  if (currentLevel > 0) {
    // ======= وضع المستوى: عد الضغطات وبعت الرسالة =======
    if (touchState == HIGH && lastTouchState == LOW) {
      unsigned long now = millis();
      if (now - lastDebounceTime > debounceDelay) {
        tapCount++;
        lastTapTime = now;
        tapPending = true;
        lastDebounceTime = now;
        Serial.print("Tap #"); Serial.println(tapCount);
        Bluetooth.print("Tap #"); Bluetooth.println(tapCount);
      }
    }

    // لو فاتت ثانية من آخر ضغطة → ابعت الرسالة
    if (tapPending && (millis() - lastTapTime >= tapWindowMs)) {
      tapPending = false;
      if (tapCount >= 1 && tapCount <= 4) {
        sendMessage(messages[currentLevel - 1][tapCount - 1]);
      }
      tapCount = 0;
    }

  } else {
    // ======= الوضع العادي: مورس يدوي =======
    if (touchState == HIGH) {
      newLetter = true;
      newWord = true;
      t1 = millis();
      setLeds(HIGH);
      tone(tonePin, toneFreq);
      while (digitalRead(touchPin) == HIGH);
      t2 = millis();
      setLeds(LOW);
      noTone(tonePin);
      onTime = t2 - t1;
      if (onTime > debounceDelay) {
        if (onTime < dotLength * 2) dashSeq += ".";
        else dashSeq += "-";
      }
    }

    gap = millis() - t2;

    if (newLetter && gap > letterSpace && !keyboardText) {
      newLetter = false;
      letterFound = false;
      for (i = 0; i < 26; i++) {
        if (dashSeq == letters[i]) {
          keyLetter = 'A' + i;
          Serial.print(keyLetter);
          Bluetooth.print(keyLetter);
          letterFound = true;
          lineLength++;
          break;
        }
      }
      if (!letterFound) {
        for (i = 0; i < 10; i++) {
          if (dashSeq == numbers[i]) {
            Serial.print(i);
            Bluetooth.print(i);
            letterFound = true;
            lineLength++;
            break;
          }
        }
      }
      if (!letterFound && dashSeq.length() > 0) {
        Serial.print("?");
        Bluetooth.print("?");
      }
      dashSeq = "";
      if (lineLength >= maxLineLength) {
        Serial.println();
        Bluetooth.println();
        lineLength = 0;
      }
    }

    if (newWord && gap > wordSpace && !keyboardText) {
      newWord = false;
      Serial.print(" ");
      Bluetooth.print(" ");
      lineLength++;
    }
  }

  lastTouchState = touchState;
}