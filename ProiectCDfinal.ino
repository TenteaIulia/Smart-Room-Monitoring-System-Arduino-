#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);


// I2C adrese  
uint8_t BH1750_ADDR = 0x23; 
const uint8_t EEPROM24_ADDR = 0x50;   
const uint16_t EXT_ADDR_TSET = 0;     

// Pini 
const int PIN_LED_PWM  = 9;  
const int PIN_HEAT_LED = 8;  
const int PIN_BTN_UP   = 2;  
const int PIN_BTN_DOWN = 3; 
const int PIN_BTN_OK   = 4;
const int PIN_BTN_BACK = 5;
const int PIN_LM35 = A0;


// BH1750 
const uint8_t BH1750_CONT_HRES = 0x10;

enum Screen { SCR_MENU, SCR_TEMP, SCR_LUX };
Screen screen = SCR_MENU;
int menuIndex = 0; // 0=Temp, 1=Lux
bool savedBlink = false;
unsigned long savedMs = 0;


// Control lumina 
float LUX_SET  = 110.0;   
float KpLux    = 0.5;    
float alphaLux = 0.2;    
float luxFilt  = 0.0;
bool luxInit = false;
int pwmOut     = 0;

// Termostat 
float Tset = 24.0;
bool heatOn = false;
const float HYST = 0.5;

// Debounce butoane
struct Btn { int pin; bool lastStable=HIGH; bool lastRead=HIGH; unsigned long lastChange=0; };
Btn bUp, bDown, bOk, bBack;

const unsigned long DEBOUNCE_MS = 30;

bool clicked(Btn &b) {
  bool r = digitalRead(b.pin);
  if (r != b.lastRead) { b.lastRead = r; b.lastChange = millis(); }
  if (millis() - b.lastChange > DEBOUNCE_MS) {
    if (r != b.lastStable) {
      b.lastStable = r;
      if (b.lastStable == LOW) return true; // click la apasare
    }
  }
  return false;
}

// Salvare eeprom extern
void eeprom24_writeBytes(uint16_t memAddr, const uint8_t* data, uint8_t len) {
  Wire.beginTransmission(EEPROM24_ADDR);
  Wire.write((uint8_t)(memAddr >> 8));     // address high
  Wire.write((uint8_t)(memAddr & 0xFF));   // address low
  for (uint8_t i = 0; i < len; i++) Wire.write(data[i]);
  Wire.endTransmission();

  // timp de scriere interna (write cycle) ~5ms tipic
  delay(5);
}

void eeprom24_readBytes(uint16_t memAddr, uint8_t* data, uint8_t len) {
  Wire.beginTransmission(EEPROM24_ADDR);
  Wire.write((uint8_t)(memAddr >> 8));
  Wire.write((uint8_t)(memAddr & 0xFF));
  Wire.endTransmission(false);

  Wire.requestFrom((int)EEPROM24_ADDR, (int)len);
  for (uint8_t i = 0; i < len && Wire.available(); i++) {
    data[i] = Wire.read();
  }
}

void saveTset() {
  uint8_t buf[sizeof(float)];
  memcpy(buf, &Tset, sizeof(float));
  eeprom24_writeBytes(EXT_ADDR_TSET, buf, sizeof(float));
}

void loadTset() {
  uint8_t buf[sizeof(float)] = {0};
  eeprom24_readBytes(EXT_ADDR_TSET, buf, sizeof(float));

  float tmp;
  memcpy(&tmp, buf, sizeof(float));
  if (isnan(tmp) || tmp < 10.0 || tmp > 35.0) tmp = 24.0;

  Tset = tmp;
}


void printLine(uint8_t row, const String &s) {
  lcd.setCursor(0, row);
  String out = s;
  if (out.length() > 16) out = out.substring(0,16);
  while (out.length() < 16) out += " ";
  lcd.print(out);
}


// BH1750 
void bh1750Init() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(BH1750_CONT_HRES);
  Wire.endTransmission();
  delay(180);
}

float bh1750ReadLux() {
  Wire.requestFrom((int)BH1750_ADDR, 2);
  if (Wire.available() < 2) return -1.0;
  uint16_t raw = ((uint16_t)Wire.read() << 8) | Wire.read();
  return raw / 1.2;
}

// LM35
float lm35ReadC() {
  long sum = 0;
  for (int i=0; i<10; i++) { sum += analogRead(PIN_LM35); delay(2); }
  float adc = sum / 10.0;
  float voltage = adc * (5.0 / 1023.0);
  return voltage * 100.0;
}


void setup() {
  pinMode(PIN_LED_PWM, OUTPUT);
  pinMode(PIN_HEAT_LED, OUTPUT);

  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_OK, INPUT_PULLUP);
  pinMode(PIN_BTN_BACK, INPUT_PULLUP);

  Wire.begin();
  bUp.pin = PIN_BTN_UP;     bUp.lastStable = HIGH; bUp.lastRead = HIGH; bUp.lastChange = 0;
  bDown.pin = PIN_BTN_DOWN; bDown.lastStable = HIGH; bDown.lastRead = HIGH; bDown.lastChange = 0;
  bOk.pin = PIN_BTN_OK;     
  bBack.pin = PIN_BTN_BACK; 

  lcd.init();
  lcd.backlight();
  lcd.clear();

  bh1750Init();
  loadTset();

  lcd.setCursor(0,0);
  lcd.print("Smart Room");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Citiri senzori
  float lux = bh1750ReadLux();
  float T = lm35ReadC();

// Control lumina (mapare + prag FULL) 
if (lux >= 0) {
  if (!luxInit) { luxFilt = lux; luxInit = true; }
  luxFilt = alphaLux * luxFilt + (1.0 - alphaLux) * lux;

  const float LUX_FULL = 25.0;   // sub 25 lx -> LED MAX (deget)
  const float LUX_MAX  = 160.0;  // peste 160 lx -> LED stins
  const float gamma    = 2.2;

  float x = luxFilt;
  if (x < 0) x = 0;
  if (x > LUX_MAX) x = LUX_MAX;

  if (luxFilt <= LUX_FULL) {
    pwmOut = 255;
  } else {
    float t = x / LUX_MAX;          // 0..1
    float y = pow(t, gamma);        // curba perceptuala
    pwmOut = (int)round(255.0 * (1.0 - y));
    pwmOut = constrain(pwmOut, 0, 255);
  }

  // LED-ul Active-Low 
  analogWrite(PIN_LED_PWM,255 - pwmOut);

}

  // Termostat cu histerezis 
  if (!isnan(T)) {
    if (!heatOn && T < Tset) heatOn = true;
    else if (heatOn && T > (Tset + HYST)) heatOn = false;
  }
  digitalWrite(PIN_HEAT_LED, heatOn ? HIGH : LOW);
  
  // Citire butoane 
  bool up    = clicked(bUp);
  bool down  = clicked(bDown);
  bool ok    = clicked(bOk);
  bool back  = clicked(bBack);

  // UI state machine 
  if (screen == SCR_MENU) {
    if (up)   menuIndex = (menuIndex + 1) % 2;   // 2 optiuni
    if (down) menuIndex = (menuIndex + 1) % 2;   // 2 optiuni

    if (ok) {
      screen = (menuIndex == 0) ? SCR_TEMP : SCR_LUX;
      lcd.clear();
    }

  // Afisare LCD 
    printLine(0, String(menuIndex==0 ? ">" : " ") + "Control Temperatura");
    printLine(1, String(menuIndex==1 ? ">" : " ") + "Control Luminozitate");
  }

  else if (screen == SCR_TEMP) {
    if (back) { screen = SCR_MENU; lcd.clear(); }

    // modifici setpoint cu sus/jos
    if (up)   { Tset += 0.5; if (Tset > 35.0) Tset = 35.0; }
    if (down) { Tset -= 0.5; if (Tset < 10.0) Tset = 10.0; }
    
    // salvez la OK
    if (ok) {
      saveTset();
      savedBlink = true;
      savedMs = millis();
    }
    
      // Afisare Tamb / Tset
    String l0 = "Tamb: ";
    if (!isnan(T)) { l0 += String(T,1); l0 += (char)223; }
    else l0 += "--.-" + String((char)223);
    
    String l1 = "Tset: " + String(Tset,1) + String((char)223);

    // Mic feedback dupa save "OK" pentru ~800ms
    if (savedBlink && millis() - savedMs < 800) {
      // pune un indicator la final
      if (l1.length() < 15) l1 += " ";
      l1 += "OK";
    } else {
      savedBlink = false;
    }

    printLine(0, l0);
    printLine(1, l1);
   }
   else if (screen == SCR_LUX) {
      if (back) { screen = SCR_MENU; lcd.clear(); }

    // Afisez Lux 
    String l0 = "Lux: ";
    if (lux >= 0) l0 += String((int)luxFilt) + " lx";
    else l0 += "--- lx";
    
    String l1 = "PWM:" + String(pwmOut) + "   BACK";


    printLine(0, l0);
    printLine(1, l1);
  }

  delay(120);
}
