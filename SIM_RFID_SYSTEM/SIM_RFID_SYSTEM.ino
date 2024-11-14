#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
//_______________________________________________________________________________
#define SS_PIN 10
#define RST_PIN 9
LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
RTC_DS1307 rtc;
unsigned long previousMillis = 0; // ذخیره زمان قبلی
const long interval = 60000; // فاصله زمانی در میلی‌ثانیه (1 دقیقه)
const long backlightTimeout = 10000; // فاصله زمانی برای خاموش شدن بک لایت (10 ثانیه)
unsigned long lastActivityMillis = 0; // زمان آخرین فعالیت
//_______________________________________________________________________________
const byte ROWS = 4; // چهار ردیف
const byte COLS = 3; // سه ستون
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {A0, A1, A2, A3}; // پین‌های ردیف
byte colPins[COLS] = {A4, A5, 2}; // پین‌های ستون
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

struct CardData {
  String uid;
  DateTime entryTime;
  DateTime exitTime;
  bool isEntry;
  bool isFirstEntry;
  TimeSpan totalWorkDuration;
  String name;
};

CardData cards[10]; // فرض می‌کنیم حداکثر 10 کارت داریم
int cardCount = 0;

void setup() {
  lcd.init();
  lcd.clear();         
  lcd.backlight();      // Make sure backlight is on
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  Wire.begin();
  
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("Ready to read from RFID tag...");

  // Set the key (default key for MIFARE cards)
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  lcd.print("      SIM");
  lcd.setCursor(0,1);
  lcd.print("  RFID system");
  delay(4000);
  lcd.clear();
  lcd.print("    WELCOME");
  lcd.setCursor(0,1);
  lcd.print("   loading...");
  delay(3000);
  lcd.clear();
  lastActivityMillis = millis(); // ثبت زمان شروع
}

void loop() {
  DateTime now = rtc.now();
  unsigned long currentMillis = millis();

  // بررسی زمان آخرین فعالیت
  if (currentMillis - lastActivityMillis > backlightTimeout) {
    lcd.noBacklight(); // خاموش کردن بک لایت
  } else {
    lcd.backlight(); // روشن کردن بک لایت
  }

  lcd.setCursor(1,0);
  lcd.print("ready to read ");
  lcd.setCursor(3,1);
  lcd.print("enter tag");

  if (now.hour() == 23 && now.minute() == 59 && now.second() == 59) {
    calculateAndDisplayWorkDurations();
    delay(1000); // Wait for 1 second to avoid multiple calculations within the same minute
  }

  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  lastActivityMillis = millis(); // به‌روزرسانی زمان آخرین فعالیت

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  byte block = 1;
  byte buffer[18];
  byte size = sizeof(buffer);

  MFRC522::StatusCode status;
  status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed: ");
    Serial.println(rfid.GetStatusCodeName(status));
    return;
  }

  status = rfid.MIFARE_Read(block, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Read failed: ");
    Serial.println(rfid.GetStatusCodeName(status));
    return;
  }

  int number = buffer[0];
  String name = "";
  if (number == 1) {
    lcd.backlight();
    name = "Mohiuddin";
  } else if (number == 2) {
    lcd.backlight();
    name = "Seyed";

  } else if (number == 3) {
    lcd.backlight();
    name = "Hojat";

  }

  if (number >= 1 && number <= 3) {
    bool found = false;

    for (int i = 0; i < cardCount; i++) {
      if (cards[i].uid == uid) {
        found = true;
        if (cards[i].isEntry) {
          cards[i].exitTime = now;
          TimeSpan workDuration = cards[i].exitTime - cards[i].entryTime;
          cards[i].totalWorkDuration = cards[i].totalWorkDuration + workDuration;
          cards[i].isEntry = false;
          Serial.print("Card UID: ");
          Serial.println(uid);
          Serial.print("Name: ");
          Serial.println(cards[i].name);
          Serial.print("Exit Time: ");
          Serial.println(cards[i].exitTime.timestamp());
          Serial.print("Work duration: ");
          Serial.print(workDuration.hours());
          Serial.print(" hours, ");
          Serial.print(workDuration.minutes());
          Serial.print(" minutes, ");
          Serial.print(workDuration.seconds());
          Serial.println(" seconds.");
          lcd.clear();
          lcd.setCursor(4,0);
          lcd.print("Goodbye");
          lcd.setCursor(3,1);
          lcd.print(cards[i].name);
          delay(2000);
          lcd.clear();
        } else {
          cards[i].entryTime = now;
          cards[i].isEntry = true;
          Serial.print("Card UID: ");
          Serial.println(uid);
          Serial.print("Name: ");
          Serial.println(cards[i].name);
          Serial.print("Entry Time: ");
          Serial.println(cards[i].entryTime.timestamp());
          lcd.clear();
          lcd.setCursor(4,0);
          if (cards[i].isFirstEntry) {
            lcd.print("Welcome");
            cards[i].isFirstEntry = false;
          } else {
            lcd.print("Welcome back");
          }
          lcd.setCursor(3,1);
          lcd.print(cards[i].name);
          delay(2000);
          lcd.clear();
        }
        break;
      }
    }

    if (!found && cardCount < 10) {
      cards[cardCount].uid = uid;
      cards[cardCount].entryTime = now;
      cards[cardCount].isEntry = true;
      cards[cardCount].isFirstEntry = true;
      cards[cardCount].name = name;
      cardCount++;
      Serial.print("New Card UID: ");
      Serial.println(uid);
      Serial.print("Name: ");
      Serial.println(name);
      Serial.print("Entry Time: ");
      Serial.println(now.timestamp());
      lcd.clear();
      lcd.setCursor(4,0);
      lcd.print("Welcome");
      lcd.setCursor(3,1);
      lcd.print(name);
      delay(2000);
      lcd.clear();
    }
  }

  // Reset the RFID reader to be ready for the next card
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void calculateAndDisplayWorkDurations() {
  for (int i = 0; i < cardCount; i++) {
    Serial.print("Card UID: ");
    Serial.println(cards[i].uid);
    Serial.print("Name: ");
    Serial.println(cards[i].name);
    Serial.print("Total Work Duration: ");
    Serial.print(cards[i].totalWorkDuration.hours());
    Serial.print(" hours, ");
    Serial.print(cards[i].totalWorkDuration.minutes());
    Serial.print(" minutes, ");
    Serial.print(cards[i].totalWorkDuration.seconds());
    Serial.println(" seconds.");
  }
}
