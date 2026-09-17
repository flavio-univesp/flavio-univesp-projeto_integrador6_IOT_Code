#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>

// RC522 via SPI (VSPI padrao do ESP32)
#define SS_PIN 5
#define RST_PIN 21
MFRC522 rfid(SS_PIN, RST_PIN);

// LCD 1602 em modo paralelo 4 bits (mesma pinagem logica do projeto original em Arduino Uno)
LiquidCrystal lcd(32, 33, 25, 26, 27, 14);

void showWaitingMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aproxime a TAG");
  lcd.setCursor(0, 1);
  lcd.print("no leitor RFID");
}

void showTag(const String &uid) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TAG identificada");
  lcd.setCursor(0, 1);
  lcd.print(uid);
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  // Diagnostico: confirma se o modulo responde via SPI antes de tentar ler cartoes.
  // 0x91 ou 0x92 = comunicacao OK. 0x00 ou 0xFF = problema de fiacao/alimentacao.
  Serial.print("Versao do firmware do MFRC522: ");
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.println(version, HEX);
  rfid.PCD_DumpVersionToSerial();

  lcd.begin(16, 2);
  showWaitingMessage();
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidString += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uidString += String(rfid.uid.uidByte[i], HEX);
    if (i != rfid.uid.size - 1) uidString += " ";
  }
  uidString.toUpperCase();

  Serial.print("UID da tag: ");
  Serial.println(uidString);
  showTag(uidString);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(3000);
  showWaitingMessage();
}
