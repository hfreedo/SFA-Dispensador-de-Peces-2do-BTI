/*
 * ================================================================
 * DISPENSADOR AUTOMÁTICO DE ALIMENTO PARA PECES
 * Proyecto: SFA - Dispensador de Peces 2do. BTI
 * ================================================================
 * Componentes:
 *   - Arduino UNO
 *   - Servomotor SG90 (pin 8)
 *   - Tira WS2812B / NeoPixel de 3 LEDs (pin 6)
 *   - LCD 16x2 I2C (dirección 0x27)
 *   - Módulo Bluetooth HC-06 (pines 2 RX, 3 TX)
 *   - Buzzer Pasivo 5V (pin 12)
 *
 * Comandos disponibles (Serial + Bluetooth):
 *   SET HORA HH:MM:SS     Ajustar la hora actual
 *   SET ANGULO XX         Calibrar ángulo del servo (10-180)
 *   SET VELOCIDAD XX      Velocidad del servo en ms/grado (5-200)
 *   SET MODO AUTO|MANUAL  Alternar entre modo automático/manual
 *   SET TONO X            Tipo de tono del buzzer (1,2,3)
 *   SET ANIMACION X       LEDs: 0 apagada, 1 radar, 2 ola, 3 arcoiris, 4 pulso
 *   ADD HORA HH:MM        Agregar un horario de alimentación
 *   DEL HORA N            Eliminar horario número N (1-10)
 *   CLEAR HORAS           Eliminar todos los horarios
 *   SET INTERVALO H:M:S   HH, HH:MM o HH:MM:SS (min 5s)
 *   DISPENSAR             Dispensar alimento manualmente
 *   ESTADO                Mostrar configuración actual
 *   AYUDA                 Mostrar lista de comandos
 *
 * Mejoras sugeridas:
 *   1. Agregar módulo RTC DS3231 para precisión de reloj
 *   2. Sensor ultrasónico/fotoresistor para detectar nivel de alimento
 *   3. Encoder rotatorio + menú LCD para control sin Bluetooth
 *   4. Conexión WiFi (ESP8266/ESP32) para control remoto vía app
 *   5. Batería de respaldo para mantener hora ante corte de energía
 *   6. Registro en microSD de cada dispensación (data logging)
 *   7. Sensor de humedad para evitar que el alimento se humedezca
 *   8. Watchdog timer para reinicio automático ante fallos
 * ================================================================
 */

#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

// ================================================================
// CONFIGURACIÓN DE PINES
// ================================================================
#define PIN_SERVO     8
#define PIN_LED_STRIP 6
#define PIN_BUZZER    12
#define BT_RX         2
#define BT_TX         3

// ================================================================
// CONSTANTES
// ================================================================
#define MAX_SCHEDULES        10
#define LCD_ADDR             0x27
#define BAUD_RATE            9600
#define MAGIC_NUMBER         0xB2
#define LED_COUNT            3
#define CMD_BUFFER_SIZE      32
#define DISPLAY_NORMAL_MS    15000UL   // 15 seg entre info del proyecto
#define DISPLAY_TITLE_MS     3000UL    // 3 seg mostrando título
#define SAVE_CLOCK_MS        300000UL  // Guardar hora cada 5 min
#define LCD_UPDATE_MS        500UL     // Actualizar LCD cada 500ms
#define DISPENSE_PAUSE_MS    500UL     // Pausa en posición abierta

// Direcciones EEPROM
#define EE_CLOCK_H           100
#define EE_CLOCK_M           101
#define EE_CLOCK_S           102

// ================================================================
// ESTRUCTURA DE CONFIGURACIÓN PERSISTENTE EN EEPROM
// ================================================================
struct Settings {
  uint8_t magic;
  uint8_t servoAngle;
  uint16_t servoSpeed;
  uint8_t mode;             // 0 = AUTO, 1 = MANUAL
  uint8_t toneType;         // 1, 2, 3
  uint8_t ledAnimation;     // 0=apagada, 1=radar, 2=ola, 3=arcoiris, 4=pulso
  uint8_t schedCount;
  uint8_t schedules[MAX_SCHEDULES][2]; // [hora][minuto]
  uint32_t intervalSeconds;
  uint8_t lastFeedH;
  uint8_t lastFeedM;
};

// ================================================================
// VARIABLES GLOBALES
// ================================================================
Servo myservo;
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
SoftwareSerial bluetooth(BT_RX, BT_TX);
Adafruit_NeoPixel ledStrip(LED_COUNT, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

Settings settings;

// Reloj interno
unsigned long startMillis = 0;
uint32_t startClockSecs = 0;  // Segundos desde 00:00 al iniciar
unsigned long lastClockSave = 0;

// Estados
enum DispState { DS_IDLE, DS_OPENING, DS_PAUSING, DS_CLOSING };
DispState dispState = DS_IDLE;
int servoCurrentPos = 0;
int servoTargetPos = 0;
unsigned long lastServoStep = 0;
unsigned long dispensePauseStart = 0;
bool dispensedThisMinute = false;

// Animación no bloqueante de la tira LED
unsigned long lastLedUpdate = 0;
uint8_t ledStep = 0;

// Display
enum LcdState { LCD_NORMAL, LCD_TITLE, LCD_DISPENSING };
LcdState lcdState = LCD_NORMAL;
unsigned long lastLcdSwitch = 0;
unsigned long lastLcdUpdate = 0;

// Comandos
char cmdBuffer[CMD_BUFFER_SIZE];
uint8_t cmdIndex = 0;

// ================================================================
// CONFIGURACIÓN POR DEFECTO
// ================================================================
void loadDefaults() {
  settings.magic = MAGIC_NUMBER;
  settings.servoAngle = 90;
  settings.servoSpeed = 20;
  settings.mode = 0;  // AUTO
  settings.toneType = 1;
  settings.ledAnimation = 1;
  settings.schedCount = 0;
  settings.intervalSeconds = 28800;  // 8 horas por defecto
  settings.lastFeedH = 0;
  settings.lastFeedM = 0;
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    settings.schedules[i][0] = 0;
    settings.schedules[i][1] = 0;
  }
}

// ================================================================
// EEPROM
// ================================================================
void saveSettings() {
  EEPROM.put(0, settings);
}

void loadSettings() {
  Settings temp;
  EEPROM.get(0, temp);
  if (temp.magic == MAGIC_NUMBER) {
    settings = temp;
  } else {
    loadDefaults();
    saveSettings();
  }
}

void saveClock() {
  int h, m, s;
  getCurrentTime(h, m, s);
  if (EEPROM.read(EE_CLOCK_H) != h ||
      EEPROM.read(EE_CLOCK_M) != m ||
      EEPROM.read(EE_CLOCK_S) != s) {
    EEPROM.write(EE_CLOCK_H, h);
    EEPROM.write(EE_CLOCK_M, m);
    EEPROM.write(EE_CLOCK_S, s);
  }
}

void loadClock() {
  int h = EEPROM.read(EE_CLOCK_H);
  int m = EEPROM.read(EE_CLOCK_M);
  int s = EEPROM.read(EE_CLOCK_S);
  if (h < 24 && m < 60 && s < 60) {
    setTime(h, m, s);
  }
}

// ================================================================
// RELOJ INTERNO
// ================================================================
void setTime(int h, int m, int s) {
  startMillis = millis();
  startClockSecs = (uint32_t)h * 3600UL + (uint32_t)m * 60UL + s;
}

void getCurrentTime(int &h, int &m, int &s) {
  unsigned long elapsedSecs = (millis() - startMillis) / 1000UL;
  uint32_t totalSecs = startClockSecs + (uint32_t)elapsedSecs;
  h = (totalSecs / 3600UL) % 24;
  m = (totalSecs / 60UL) % 60;
  s = totalSecs % 60;
}

void fmtTimeStr(int h, int m, int s, char *buf) {
  buf[0] = '0' + h / 10;
  buf[1] = '0' + h % 10;
  buf[2] = ':';
  buf[3] = '0' + m / 10;
  buf[4] = '0' + m % 10;
  buf[5] = ':';
  buf[6] = '0' + s / 10;
  buf[7] = '0' + s % 10;
  buf[8] = '\0';
}

// ================================================================
// BUZZER
// ================================================================
void beep(int type) {
  switch (type) {
    case 1: // Un pitido
      tone(PIN_BUZZER, 1000, 150);
      break;
    case 2: // Doble pitido
      tone(PIN_BUZZER, 1000, 100);
      delay(100);
      tone(PIN_BUZZER, 1000, 100);
      break;
    case 3: // Tono ascendente
      for (int f = 500; f <= 1500; f += 100) {
        tone(PIN_BUZZER, f, 50);
        delay(60);
      }
      break;
  }
}

void alertCommandOK() {
  tone(PIN_BUZZER, 1200, 100);
}

void alertDispense() {
  beep(settings.toneType);
}

void alertError() {
  tone(PIN_BUZZER, 400, 300);
}

// ================================================================
// TIRA LED WS2812B - 3 PIXELES, ANIMACIONES NO BLOQUEANTES
// ================================================================
uint32_t colorWheel(byte position) {
  position = 255 - position;
  if (position < 85) {
    return ledStrip.Color(255 - position * 3, 0, position * 3);
  }
  if (position < 170) {
    position -= 85;
    return ledStrip.Color(0, position * 3, 255 - position * 3);
  }
  position -= 170;
  return ledStrip.Color(position * 3, 255 - position * 3, 0);
}

void clearLedStrip() {
  ledStrip.clear();
  ledStrip.show();
}

void updateLedAnimation() {
  unsigned long now = millis();
  uint16_t frameMs = settings.ledAnimation == 4 ? 45 : 110;
  if (now - lastLedUpdate < frameMs) return;
  lastLedUpdate = now;

  switch (settings.ledAnimation) {
    case 0: // Apagada
      ledStrip.clear();
      break;

    case 1: { // Radar: punto verde que recorre y regresa
      const uint8_t radarPath[4] = {0, 1, 2, 1};
      ledStrip.clear();
      uint8_t head = radarPath[ledStep % 4];
      ledStrip.setPixelColor(head, ledStrip.Color(0, 255, 90));
      if (head > 0) ledStrip.setPixelColor(head - 1, ledStrip.Color(0, 24, 8));
      if (head + 1 < LED_COUNT) ledStrip.setPixelColor(head + 1, ledStrip.Color(0, 24, 8));
      break;
    }

    case 2: { // Ola marina: azules desfasados
      const uint8_t levels[6] = {20, 55, 120, 220, 120, 55};
      for (uint8_t i = 0; i < LED_COUNT; i++) {
        uint8_t level = levels[(ledStep + i * 2) % 6];
        ledStrip.setPixelColor(i, ledStrip.Color(0, level / 2, level));
      }
      break;
    }

    case 3: // Arcoíris
      for (uint8_t i = 0; i < LED_COUNT; i++) {
        ledStrip.setPixelColor(i, colorWheel((ledStep * 8 + i * 85) & 255));
      }
      break;

    case 4: { // Pulso coral
      uint8_t phase = ledStep % 32;
      uint8_t level = phase < 16 ? phase * 12 : (31 - phase) * 12;
      for (uint8_t i = 0; i < LED_COUNT; i++) {
        ledStrip.setPixelColor(i, ledStrip.Color(level, level / 5, level / 12));
      }
      break;
    }

    default:
      settings.ledAnimation = 1;
      return;
  }

  ledStrip.show();
  ledStep++;
}

// ================================================================
// SERVOMOTOR - Movimiento no bloqueante
// ================================================================
void startServoMove(int target) {
  if (target < 0) target = 0;
  if (target > 180) target = 180;
  servoTargetPos = target;
  lastServoStep = millis();
}

void updateServoMove() {
  if (servoCurrentPos == servoTargetPos) return;

  unsigned long now = millis();
  if (now - lastServoStep < settings.servoSpeed) return;
  lastServoStep = now;

  if (servoCurrentPos < servoTargetPos) {
    servoCurrentPos++;
  } else {
    servoCurrentPos--;
  }
  myservo.write(servoCurrentPos);

  if (servoCurrentPos == servoTargetPos) {
    if (dispState == DS_OPENING) {
      dispState = DS_PAUSING;
      dispensePauseStart = millis();
    } else if (dispState == DS_CLOSING) {
      dispState = DS_IDLE;
      lcdState = LCD_NORMAL;
      lastLcdSwitch = millis();
    }
  }
}

void triggerDispense() {
  if (dispState != DS_IDLE) return;  // Ya está dispensando

  servoCurrentPos = 0;
  myservo.write(0);
  startServoMove(settings.servoAngle);
  dispState = DS_OPENING;
  lcdState = LCD_DISPENSING;
  dispensedThisMinute = true;

  // Guardar hora de última alimentación
  int h, m, s;
  getCurrentTime(h, m, s);
  settings.lastFeedH = h;
  settings.lastFeedM = m;
  saveSettings();

  alertDispense();

  Serial.print(F("\n[Dispensando] "));
  char buf[9];
  fmtTimeStr(h, m, s, buf);
  Serial.println(buf);
}

// ================================================================
// LÓGICA DE HORARIOS
// ================================================================
bool matchesSchedule(int h, int m) {
  for (int i = 0; i < settings.schedCount; i++) {
    if (settings.schedules[i][0] == h &&
        settings.schedules[i][1] == m) {
      return true;
    }
  }
  return false;
}

int calcTimeRemainingSecs() {
  int hNow, mNow, sNow;
  getCurrentTime(hNow, mNow, sNow);
  uint32_t nowSecs = (uint32_t)hNow * 3600UL + (uint32_t)mNow * 60UL + sNow;

  if (settings.schedCount > 0) {
    // Buscar el próximo horario
    uint32_t bestSecs = 0xFFFFFFFFUL;
    bool found = false;

    for (int i = 0; i < settings.schedCount; i++) {
      uint32_t schedSecs = (uint32_t)settings.schedules[i][0] * 3600UL
                         + (uint32_t)settings.schedules[i][1] * 60UL;
      int32_t diff = (int32_t)(schedSecs - nowSecs);
      if (diff > 0 && (uint32_t)diff < bestSecs) {
        bestSecs = (uint32_t)diff;
        found = true;
      }
    }

    if (!found) {
      // Todos los horarios ya pasaron hoy: tomar el primero de mañana
      uint32_t firstSecs = (uint32_t)settings.schedules[0][0] * 3600UL
                         + (uint32_t)settings.schedules[0][1] * 60UL;
      bestSecs = firstSecs + 86400UL - nowSecs;
    }
    return (int)bestSecs;
  }

  // Modo intervalo
  uint32_t lastSecs = (uint32_t)settings.lastFeedH * 3600UL
                    + (uint32_t)settings.lastFeedM * 60UL;
  uint32_t intervalSecs = settings.intervalSeconds;

  uint32_t elapsed;
  if (nowSecs >= lastSecs) {
    elapsed = nowSecs - lastSecs;
  } else {
    elapsed = nowSecs + 86400UL - lastSecs;
  }

  uint32_t remaining = intervalSecs - (elapsed % intervalSecs);
  if (remaining == 0) remaining = intervalSecs;
  return (int)remaining;
}

void fmtTimeRemaining(char *buf) {
  int secs = calcTimeRemainingSecs();
  if (secs < 0) secs = 0;
  int h = secs / 3600;
  int m = (secs % 3600) / 60;
  int s = secs % 60;

  buf[0] = '0' + h / 10;
  buf[1] = '0' + h % 10;
  buf[2] = ':';
  buf[3] = '0' + m / 10;
  buf[4] = '0' + m % 10;
  buf[5] = ':';
  buf[6] = '0' + s / 10;
  buf[7] = '0' + s % 10;
  buf[8] = '\0';
}

// ================================================================
// DISPLAY LCD
// ================================================================
void updateLCD() {
  unsigned long now = millis();
  if (now - lastLcdUpdate < LCD_UPDATE_MS) return;
  lastLcdUpdate = now;

  // Máquina de estados del display
  if (lcdState == LCD_DISPENSING) {
    // No cambiar mientras se dispensa
  } else if (lcdState == LCD_NORMAL && now - lastLcdSwitch >= DISPLAY_NORMAL_MS) {
    lcdState = LCD_TITLE;
    lastLcdSwitch = now;
  } else if (lcdState == LCD_TITLE && now - lastLcdSwitch >= DISPLAY_TITLE_MS) {
    lcdState = LCD_NORMAL;
    lastLcdSwitch = now;
  }

  lcd.clear();

  switch (lcdState) {
    case LCD_NORMAL: {
      int h, m, s;
      getCurrentTime(h, m, s);
      char buf[17];

      // Fila 1: Hora actual + indicador de modo
      fmtTimeStr(h, m, s, buf);
      lcd.setCursor(0, 0);
      lcd.print(buf);
      lcd.print(' ');
      if (settings.schedCount > 0) {
        lcd.print(settings.mode == 0 ? F("HorAuto") : F("HorMan "));
      } else {
        lcd.print(settings.mode == 0 ? F("IntAuto") : F("IntMan "));
      }

      // Fila 2: Tiempo restante
      lcd.setCursor(0, 1);
      lcd.print(F("Prox: "));
      fmtTimeRemaining(buf);
      lcd.print(buf);
      break;
    }

    case LCD_TITLE: {
      lcd.setCursor(0, 0);
      lcd.print(F("Dispensador de"));
      lcd.setCursor(0, 1);
      lcd.print(F("Alimentos 2BTI"));
      break;
    }

    case LCD_DISPENSING: {
      lcd.setCursor(0, 0);
      lcd.print(F("  Dispensando"));
      lcd.setCursor(0, 1);
      lcd.print(F("  alimento... "));
      break;
    }
  }
}

// ================================================================
// PARSEO DE COMANDOS
// ================================================================
void printHelp() {
  Serial.println(F("\n======== COMANDOS DISPONIBLES ========"));
  Serial.println(F("SET HORA HH:MM:SS    Ajustar hora"));
  Serial.println(F("SET ANGULO XX        Angulo servo (10-180)"));
  Serial.println(F("SET VELOCIDAD XX     Velocidad ms/grado (5-200)"));
  Serial.println(F("SET MODO AUTO|MANUAL Modo operacion"));
  Serial.println(F("SET TONO X           Tono buzzer (1-3)"));
  Serial.println(F("SET ANIMACION X      LED 0-4 (ver lista)"));
  Serial.println(F("ADD HORA HH:MM       Agregar horario"));
  Serial.println(F("DEL HORA N           Eliminar horario N"));
  Serial.println(F("CLEAR HORAS          Borrar todos"));
  Serial.println(F("SET INTERVALO H:M:S  Intervalo (8, 0:30, 0:0:15)"));
  Serial.println(F("DISPENSAR            Dispensar ahora"));
  Serial.println(F("ESTADO               Ver configuracion"));
  Serial.println(F("AYUDA                Mostrar esta lista"));
  Serial.println(F("========================================"));
}

void printStatus() {
  int h, m, s;
  getCurrentTime(h, m, s);

  Serial.println(F("\n============= ESTADO ============="));
  Serial.print(F("Hora actual: "));
  char buf[9];
  fmtTimeStr(h, m, s, buf);
  Serial.println(buf);

  Serial.print(F("Modo: "));
  Serial.println(settings.mode == 0 ? F("AUTOMATICO") : F("MANUAL"));

  if (settings.schedCount > 0) {
    Serial.println(F("Horarios programados:"));
    for (int i = 0; i < settings.schedCount; i++) {
      Serial.print(F("  "));
      Serial.print(i + 1);
      Serial.print(F(") "));
      if (settings.schedules[i][0] < 10) Serial.print('0');
      Serial.print(settings.schedules[i][0]);
      Serial.print(':');
      if (settings.schedules[i][1] < 10) Serial.print('0');
      Serial.println(settings.schedules[i][1]);
    }
  } else {
    uint32_t iv = settings.intervalSeconds;
    Serial.print(F("Intervalo: "));
    if (iv >= 3600) { Serial.print(iv / 3600); Serial.print(F("h ")); }
    if ((iv % 3600) >= 60 || iv < 60) { Serial.print((iv % 3600) / 60); Serial.print(F("m ")); }
    Serial.print(iv % 60);
    Serial.println(F("s"));
  }

  Serial.print(F("Angulo servo: "));
  Serial.print(settings.servoAngle);
  Serial.println(F(" grados"));

  Serial.print(F("Velocidad servo: "));
  Serial.print(settings.servoSpeed);
  Serial.println(F(" ms/grado"));

  Serial.print(F("Tipo de tono: "));
  Serial.println(settings.toneType);

  Serial.print(F("Animacion LED: "));
  Serial.println(settings.ledAnimation);

  Serial.print(F("Proxima alimentacion en: "));
  fmtTimeRemaining(buf);
  Serial.println(buf);
  Serial.println(F("===================================="));
}

void parseCommand(const char *cmd) {
  if (strlen(cmd) == 0) return;

  alertCommandOK();

  if (strncmp(cmd, "SET HORA ", 9) == 0) {
    int h, m, s;
    if (sscanf(cmd + 9, "%d:%d:%d", &h, &m, &s) == 3) {
      if (h < 24 && m < 60 && s < 60) {
        setTime(h, m, s);
        saveClock();
        Serial.print(F("Hora ajustada: "));
        char buf[9];
        fmtTimeStr(h, m, s, buf);
        Serial.println(buf);
      } else { Serial.println(F("Error: hora invalida")); alertError(); }
    } else { Serial.println(F("Formato: SET HORA HH:MM:SS")); alertError(); }
    return;
  }

  if (strncmp(cmd, "SET ANGULO ", 11) == 0) {
    int a = atoi(cmd + 11);
    if (a >= 10 && a <= 180) {
      settings.servoAngle = a;
      saveSettings();
      Serial.print(F("Angulo: "));
      Serial.println(a);
    } else { Serial.println(F("Error: 10-180 grados")); alertError(); }
    return;
  }

  if (strncmp(cmd, "SET VELOCIDAD ", 14) == 0) {
    int v = atoi(cmd + 14);
    if (v >= 5 && v <= 200) {
      settings.servoSpeed = v;
      saveSettings();
      Serial.print(F("Velocidad: "));
      Serial.print(v);
      Serial.println(F(" ms/grado"));
    } else { Serial.println(F("Error: 5-200 ms/grado")); alertError(); }
    return;
  }

  if (strncmp(cmd, "SET MODO ", 9) == 0) {
    if (strstr(cmd + 9, "AUTO")) {
      settings.mode = 0;
      saveSettings();
      Serial.println(F("Modo: AUTOMATICO"));
    } else if (strstr(cmd + 9, "MANUAL")) {
      settings.mode = 1;
      saveSettings();
      Serial.println(F("Modo: MANUAL"));
    } else { Serial.println(F("Use: AUTO o MANUAL")); alertError(); }
    return;
  }

  if (strncmp(cmd, "SET TONO ", 9) == 0) {
    int t = atoi(cmd + 9);
    if (t >= 1 && t <= 3) {
      settings.toneType = t;
      saveSettings();
      Serial.print(F("Tono: "));
      Serial.println(t);
      beep(t);
    } else { Serial.println(F("Error: 1-3")); alertError(); }
    return;
  }

  if (strncmp(cmd, "SET ANIMACION ", 14) == 0) {
    int animation = atoi(cmd + 14);
    if (animation >= 0 && animation <= 4) {
      settings.ledAnimation = animation;
      ledStep = 0;
      saveSettings();
      Serial.print(F("Animacion LED: "));
      Serial.println(animation);
      if (animation == 0) clearLedStrip();
    } else {
      Serial.println(F("Error: animacion LED 0-4"));
      alertError();
    }
    return;
  }

  if (strncmp(cmd, "ADD HORA ", 9) == 0) {
    int h, m;
    if (sscanf(cmd + 9, "%d:%d", &h, &m) == 2) {
      if (h < 24 && m < 60 && settings.schedCount < MAX_SCHEDULES) {
        settings.schedules[settings.schedCount][0] = h;
        settings.schedules[settings.schedCount][1] = m;
        settings.schedCount++;
        saveSettings();
        Serial.print(F("Horario agregado: "));
        if (h < 10) Serial.print('0');
        Serial.print(h);
        Serial.print(':');
        if (m < 10) Serial.print('0');
        Serial.println(m);
      } else {
        Serial.println(F("Error: hora invalida o maximo alcanzado"));
        alertError();
      }
    } else { Serial.println(F("Formato: ADD HORA HH:MM")); alertError(); }
    return;
  }

  if (strncmp(cmd, "DEL HORA ", 9) == 0) {
    int n = atoi(cmd + 9);
    if (n >= 1 && n <= settings.schedCount) {
      n--; // Índice base 0
      for (int i = n; i < settings.schedCount - 1; i++) {
        settings.schedules[i][0] = settings.schedules[i + 1][0];
        settings.schedules[i][1] = settings.schedules[i + 1][1];
      }
      settings.schedCount--;
      saveSettings();
      Serial.print(F("Horario "));
      Serial.print(n + 1);
      Serial.println(F(" eliminado"));
    } else { Serial.println(F("Error: numero invalido")); alertError(); }
    return;
  }

  if (strcmp(cmd, "CLEAR HORAS") == 0) {
    settings.schedCount = 0;
    saveSettings();
    Serial.println(F("Horarios eliminados"));
    return;
  }

  if (strncmp(cmd, "SET INTERVALO ", 14) == 0) {
    int h = 0, m = 0, s = 0;
    int n = sscanf(cmd + 14, "%d:%d:%d", &h, &m, &s);
    uint32_t totalSecs = 0;

    if (n == 1) {
      totalSecs = (uint32_t)h * 3600UL;           // Solo horas
    } else if (n == 2) {
      totalSecs = (uint32_t)h * 60UL + m;          // Min:Seg interpretado como MM:SS
    } else if (n == 3) {
      totalSecs = (uint32_t)h * 3600UL + (uint32_t)m * 60UL + s; // HH:MM:SS
    } else {
      Serial.println(F("Formato: SET INTERVALO HH, HH:MM o HH:MM:SS"));
      alertError();
      return;
    }

    if (totalSecs < 5) {
      Serial.println(F("Error: minimo 5 segundos"));
      alertError();
      return;
    }

    settings.intervalSeconds = totalSecs;
    saveSettings();
    Serial.print(F("Intervalo: "));
    if (totalSecs >= 3600) {
      Serial.print(totalSecs / 3600);
      Serial.print(F("h "));
    }
    if ((totalSecs % 3600) >= 60 || totalSecs < 60) {
      Serial.print((totalSecs % 3600) / 60);
      Serial.print(F("m "));
    }
    Serial.print(totalSecs % 60);
    Serial.println(F("s"));
    return;
  }

  if (strcmp(cmd, "DISPENSAR") == 0) {
    triggerDispense();
    return;
  }

  if (strcmp(cmd, "ESTADO") == 0) {
    printStatus();
    return;
  }

  if (strcmp(cmd, "AYUDA") == 0) {
    printHelp();
    return;
  }

  Serial.println(F("Comando no reconocido. Use AYUDA"));
  alertError();
}

// ================================================================
// PROCESAMIENTO DE ENTRADA (Serial + Bluetooth)
// ================================================================
void checkSerialInput(Stream &stream, const char *source) {
  while (stream.available()) {
    char c = stream.read();
    if (c == '\n' || c == '\r') {
      if (cmdIndex > 0) {
        cmdBuffer[cmdIndex] = '\0';
        // Eco solo para Serial
        if (&stream == &Serial) {
          Serial.print(F("\n[Comando recibido] "));
          Serial.println(cmdBuffer);
        } else {
          Serial.print(F("[BT] "));
          Serial.println(cmdBuffer);
        }
        parseCommand(cmdBuffer);
        cmdIndex = 0;
      }
    } else if (cmdIndex < CMD_BUFFER_SIZE - 1) {
      cmdBuffer[cmdIndex++] = c;
    }
  }
}

// ================================================================
// VERIFICACIÓN DE DISPENSACIÓN AUTOMÁTICA
// ================================================================
void checkAutoDispense() {
  if (settings.mode != 0) return;  // Modo manual
  if (dispState != DS_IDLE) return; // Ya dispensando

  int h, m, s;
  getCurrentTime(h, m, s);

  // Resetear flag cada minuto
  if (s == 0) {
    dispensedThisMinute = false;
  }

  if (dispensedThisMinute) return;

  if (settings.schedCount > 0) {
    if (matchesSchedule(h, m) && !dispensedThisMinute) {
      triggerDispense();
    }
  } else {
    // Modo intervalo
    uint32_t nowSecs = (uint32_t)h * 3600UL + (uint32_t)m * 60UL + s;
    uint32_t lastSecs = (uint32_t)settings.lastFeedH * 3600UL
                      + (uint32_t)settings.lastFeedM * 60UL;
    uint32_t intervalSecs = settings.intervalSeconds;

    uint32_t elapsed;
    if (nowSecs >= lastSecs) {
      elapsed = nowSecs - lastSecs;
    } else {
      elapsed = nowSecs + 86400UL - lastSecs;  // Cruce de medianoche
    }

    if (elapsed >= intervalSecs) {
      triggerDispense();
    }
  }
}

// ================================================================
// CONFIGURACIÓN INICIAL
// ================================================================
void setup() {
  // Inicialización de pines
  pinMode(PIN_BUZZER, OUTPUT);

  ledStrip.begin();
  // 120/255: brillo visible con margen de consumo para 3 NeoPixels.
  ledStrip.setBrightness(120);
  clearLedStrip();

  // Inicializar comunicaciones
  Serial.begin(BAUD_RATE);
  bluetooth.begin(BAUD_RATE);

  // Inicializar LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F(" Iniciando..."));
  lcd.setCursor(0, 1);
  lcd.print(F("Dispensador 2BTI"));
  delay(1500);

  // Cargar configuración de EEPROM
  loadSettings();

  // Cargar hora guardada
  loadClock();

  // Inicializar servo
  myservo.attach(PIN_SERVO);
  myservo.write(0);
  servoCurrentPos = 0;
  servoTargetPos = 0;

  // Mensaje de bienvenida
  Serial.println(F("\n=============================="));
  Serial.println(F("SFA DISPENSADOR DE PECES 2BTI"));
  Serial.println(F("=============================="));
  Serial.println(F("Use AYUDA para ver comandos"));
  Serial.println(F("=============================="));

  beep(2); // Doble pitido de inicio

  lastLcdSwitch = millis();
  lastLcdUpdate = 0;
  lastClockSave = millis();
  lastServoStep = millis();
}

// ================================================================
// BUCLE PRINCIPAL
// ================================================================
void loop() {
  // Leer comandos de Serial y Bluetooth
  checkSerialInput(Serial, "SER");
  checkSerialInput(bluetooth, "BT");

  // Actualizar servo (no bloqueante)
  if (dispState != DS_IDLE) {
    updateServoMove();

    // Manejar pausa en posición abierta
    if (dispState == DS_PAUSING &&
        millis() - dispensePauseStart >= DISPENSE_PAUSE_MS) {
      startServoMove(0);
      dispState = DS_CLOSING;
    }
  }

  // Verificar dispensación automática
  checkAutoDispense();

  // Mantener activa la animación LED elegida sin bloquear el programa
  updateLedAnimation();

  // Actualizar LCD
  updateLCD();

  // Guardar hora periódicamente
  if (millis() - lastClockSave >= SAVE_CLOCK_MS) {
    saveClock();
    lastClockSave = millis();
  }
}
