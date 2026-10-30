#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <ESP32Encoder.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Wire.h>
#include <si5351.h>
#include <EEPROM.h>

// ==================== PINES ====================
#define TFT_MOSI   23
#define TFT_SCLK   18
#define TFT_DC      4

#define TFT_CS1     5
#define TFT_RST1    2
#define TFT_CS2    17
#define TFT_RST2   13
#define TFT_CS3    15
#define TFT_RST3   14

#define ENC_CLK    34
#define ENC_DT     35
#define ENC_SW     33

#define PTT_PIN    26
#define I2C_SDA    22
#define I2C_SCL    21
#define S_METER    36
#define VU_METER   32
#define TX_ENABLE_PIN 25

// ==================== CONSTANTES ====================
#define FI_455      455000
#define EEPROM_SIZE 64
#define RX_MIN      3500000
#define RX_MAX      8000000
#define TX_MIN      7000000
#define TX_MAX      7300000
#define SERIAL_BAUD 115200
#define CMD_BUFFER_SIZE 32

#define EE_MEM_BASE      0
#define EE_FRECUENCIA   16
#define EE_CLARIFICADOR 20
#define EE_BFO          24
#define EE_PASO         25
#define EE_EN_CLAR      26
#define EE_MAGIC        27
#define EE_CAL          32
#define EE_MAGIC_VAL    0xA5

#define CLAR_PASO    25
#define CLAR_MAX   1000

#define BFO_HOLD_MS        1000
#define MEM_HOLD_MS        3000
#define CAL_HOLD_MS        5000
#define RESET_HOLD_MS     10000
#define GUARDADO_HOLD_MS   3000
#define CLICK_MAX_MS        800
#define CONFIG_DEBOUNCE_MS 3000

#define MODO_AM     0
#define MODO_BFO    1
#define MODO_MEM    2
#define MODO_CAL    3
#define MODO_RESET  4

#define TAG_NONE    0
#define TAG_BFO     1
#define TAG_CLAR    2
#define TAG_MEM     3
#define TAG_CAL     4
#define TAG_RESET   5
#define TAG_NOTX    6

#define CELESTE_CLARO 0x6D5F
#define NARANJA       0xFD20
#define BLANCO_SUAVE  0xC618
#define AZUL_S_METER   0x001F
#define VERDE_MOD      0x07C0
#define AMARILLO_MOD   0xFFA0
#define ROJO_MOD       0xF800
#define AZUL_INDICADOR 0x8410

// ---- Ajuste del S-Meter ----
float calibracionS = 2.0f;    // Pendiente (qué tan separados están los dB)
float offsetS = 50.0f;        // dB en el fondo de la escala (S1)
float calibracionMOD = 1.09;

// ---- Velocidad de las agujas ----
// Más grande = más rápido (1.0 es instantáneo)
const float factorSubidaS = 0.45f; // Sube rápido
const float factorBajadaS = 0.12f; // Baja lento

#define BASE_Y 127
#define IZQ_X 25
#define DER_X 103
#define AJUSTE_Y 12
#define NIVEL_MIN 15
#define NIVEL_MAX 115
#define NIVEL_CURSOR_MIN (NIVEL_MIN + 2)
#define NIVEL_CURSOR_MAX (NIVEL_MAX - 2)
#define NIVEL_S1 25
#define NIVEL_S3 43
#define NIVEL_S6 60
#define NIVEL_S9 77
#define NIVEL_20 96
#define AGUJA_UP   2
#define AGUJA_DOWN 1

// ==================== OBJETOS ====================
Adafruit_ST7735 tft1 = Adafruit_ST7735(TFT_CS1, TFT_DC, TFT_RST1);
Adafruit_ST7735 tft2 = Adafruit_ST7735(TFT_CS2, TFT_DC, TFT_RST2);
Adafruit_ST7735 tft3 = Adafruit_ST7735(TFT_CS3, TFT_DC, TFT_RST3);

ESP32Encoder encoder;
Si5351 si5351;

// ==================== ESTADO ====================
long frecuenciaRX   = 7100000;
long frecuenciaBase = 7100000;
long clarificador   = 0;
int  paso           = 1000;
bool enClar         = false;
int  pasoAlEntrarBFO = 1000;
int  pasoAntesCal   = 1000;
bool rxMode         = true;

bool memActivo  = false;
int  memoriaSeleccionada = 0;
bool memGuardadoRealizado = false;

bool bfoActivo = false;
bool calActivo = false;
bool calGuardadoRealizado = false;
long calFrecuenciaBase = 0;
long calCorreccion = 0;
bool resetModo = false;
bool resetGuardadoRealizado = false;

int  modoAlPresionar = MODO_AM;
bool botonPresionado = false;
bool transicionEnPulsacion = false;
int  nivelAlcanzado = 0;
unsigned long pulsacionStartTime = 0;
const unsigned long debounceDelay = 50;

bool modoInvalido = false;
bool txEnableState = false;
unsigned long txEnableTime = 0;

int sMeterNivel = 0;
int vuMeterNivel = 0;
float sMeterFiltrado = 0;
float vuMeterFiltrado = 0;
bool display3Inicializado = false;
int ultimoNivelS = -1;
int ultimoNivelVU = -1;

char cmdBuffer[CMD_BUFFER_SIZE];
int cmdIndex = 0;

long memorias[4] = {7100000, 0, 0, 0};
bool memoriaGrabada[4] = {true, false, false, false};

bool configSucia = false;
unsigned long configCambioMs = 0;

struct ClkCache { bool on; uint64_t freq; };
ClkCache clkCache[3] = {{false, 0}, {false, 0}, {false, 0}};

int      uiDig[7]    = {-1, -1, -1, -1, -1, -1, -1};
uint16_t uiDigCol[7] = {0, 0, 0, 0, 0, 0, 0};
int      uiMemVal[4] = {-1, -1, -1, -1};
int      uiMemSel    = -2;
int      uiTagModo   = -1;
long     uiClarVal   = 0;
int      uiTagBand   = -1;
int      uiTagRXTX   = -1;
bool     uiForzar    = true;

// ==================== PROTOTIPOS ====================
void actualizarPantallas();
void inicializarDisplayAnalogico();
void actualizarIndicadores();
void actualizarModoRxTx();
void cargarMemoria(int idx);
void guardarMemoria(int idx, long freq);
void cargarMemoriasEEPROM();
void guardarCorreccionSI5351(long correccion);
void cargarCorreccionSI5351();
void verificarRangoTX();
void leerMedidores();
int  mapearNivel(float raw, int minNivel, int maxNivel, float calibracion);
int  mapearNivelLog(float raw, int minNivel, int maxNivel, float slope, float offset);
void enviarEstado();
void procesarComandos();
void marcarConfigSucia();
void guardarConfig();
void cargarConfig();
void recalcularFrecuencia();
void limitarFrecuencia();
void handleEncoder();
void handlePTT();
void limpiarModos();
void entrarBFO();
void salirBFO();
void entrarMEM();
void entrarCAL();
void entrarRESET();
void confirmarReset();
void ciclarPaso();
int  modoActual();
int  getModoTag();
uint8_t indicePaso(int p);
int  pasoDesdeIndice(uint8_t i);
void drawRowS(int y);
void drawRowVU(int y);

uint8_t indicePaso(int p) {
  if (p == 100000) return 2;
  if (p == 10000)  return 1;
  return 0;
}
int pasoDesdeIndice(uint8_t i) {
  if (i == 2) return 100000;
  if (i == 1) return 10000;
  return 1000;
}
int modoActual() {
  if (resetModo) return MODO_RESET;
  if (calActivo) return MODO_CAL;
  if (memActivo) return MODO_MEM;
  if (bfoActivo) return MODO_BFO;
  return MODO_AM;
}
int getModoTag() {
  if (!rxMode && modoInvalido) return TAG_NOTX;
  if (resetModo) return TAG_RESET;
  if (calActivo) return TAG_CAL;
  if (memActivo) return TAG_MEM;
  if (bfoActivo && enClar) return TAG_CLAR;
  if (bfoActivo) return TAG_BFO;
  return TAG_NONE;
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(SERIAL_BAUD);

  EEPROM.begin(EEPROM_SIZE);
  cargarMemoriasEEPROM();
  cargarCorreccionSI5351();
  cargarConfig();

  pinMode(TFT_CS1, OUTPUT);
  pinMode(TFT_CS2, OUTPUT);
  pinMode(TFT_CS3, OUTPUT);
  pinMode(TFT_DC, OUTPUT);

  digitalWrite(TFT_CS1, HIGH);
  digitalWrite(TFT_CS2, HIGH);
  digitalWrite(TFT_CS3, HIGH);

  digitalWrite(TFT_CS1, LOW);
  tft1.initR(INITR_144GREENTAB);
  tft1.setSPISpeed(4000000UL);
  tft1.setRotation(0);
  tft1.fillScreen(ST77XX_BLACK);
  digitalWrite(TFT_CS1, HIGH);

  digitalWrite(TFT_CS2, LOW);
  tft2.initR(INITR_144GREENTAB);
  tft2.setSPISpeed(4000000UL);
  tft2.setRotation(0);
  tft2.fillScreen(ST77XX_BLACK);
  digitalWrite(TFT_CS2, HIGH);

  digitalWrite(TFT_CS3, LOW);
  tft3.initR(INITR_144GREENTAB);
  tft3.setSPISpeed(4000000UL);
  tft3.setRotation(0);
  tft3.fillScreen(ST77XX_BLACK);
  digitalWrite(TFT_CS3, HIGH);

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(ENC_DT, ENC_CLK);
  encoder.setFilter(1023);
  encoder.setCount(0);

  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(PTT_PIN, INPUT_PULLUP);
  pinMode(TX_ENABLE_PIN, OUTPUT);
  digitalWrite(TX_ENABLE_PIN, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, calCorreccion);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
  si5351.output_enable(SI5351_CLK0, false);
  si5351.output_enable(SI5351_CLK1, false);
  si5351.output_enable(SI5351_CLK2, false);
  for (int i = 0; i < 3; i++) { clkCache[i].on = false; clkCache[i].freq = 0; }

  if (bfoActivo) pasoAlEntrarBFO = paso;

  recalcularFrecuencia();
  actualizarModoRxTx();

  inicializarDisplayAnalogico();
  uiForzar = true;
  actualizarPantallas();
}

// ==================== ADC ====================
int mapearNivel(float raw, int minNivel, int maxNivel, float calibracion) {
  float voltaje = (raw / 4095.0f) * 3.3f * calibracion;
  if (voltaje > 3.3f) voltaje = 3.3f;
  if (voltaje < 0)    voltaje = 0;
  int nivel = minNivel + (voltaje / 3.3f) * (maxNivel - minNivel);
  if (nivel < minNivel) nivel = minNivel;
  if (nivel > maxNivel) nivel = maxNivel;
  return nivel;
}

int mapearNivelLog(float raw, int minNivel, int maxNivel, float slope, float offset) {
  if (raw < 1.0f) raw = 1.0f; 
  // Pasamos el ADC a dB (0 a -infinito)
  float db = 20.0f * log10(raw / 4095.0f); 
  
  // Mapeo lineal en dB
  float nivel = minNivel + ((db + offset) * slope);
  
  if (nivel < minNivel) nivel = minNivel;
  if (nivel > maxNivel) nivel = maxNivel;
  return nivel;
}

void leerMedidores() {
  // --- S-METER ---
  if (rxMode) {
    int rawS = analogRead(S_METER);
    if (rawS > sMeterFiltrado) {
      // Sube más rápido
      sMeterFiltrado = sMeterFiltrado * (1.0f - factorSubidaS) + rawS * factorSubidaS;
    } else {
      // Baja más lento
      sMeterFiltrado = sMeterFiltrado * (1.0f - factorBajadaS) + rawS * factorBajadaS;
    }
  } else {
    sMeterFiltrado = sMeterFiltrado * (1.0f - factorBajadaS) + 0 * factorBajadaS;
  }
  sMeterNivel = mapearNivelLog(sMeterFiltrado, NIVEL_CURSOR_MIN, NIVEL_CURSOR_MAX, calibracionS, offsetS);

  // --- VU-METER ---
  if (!rxMode) {
    int rawVU = analogRead(VU_METER);
    if (rawVU > vuMeterFiltrado) {
      vuMeterFiltrado = vuMeterFiltrado * 0.7f + rawVU * 0.3f; // Sube rápido
    } else {
      vuMeterFiltrado = vuMeterFiltrado * 0.9f + rawVU * 0.1f; // Baja lento
    }
  } else {
    vuMeterFiltrado = vuMeterFiltrado * 0.9f + 0 * 0.1f;
  }
  vuMeterNivel = mapearNivel(vuMeterFiltrado, NIVEL_CURSOR_MIN, NIVEL_CURSOR_MAX, calibracionMOD);
}

// ==================== DISPLAY 3 (AGUJAS) ====================
void inicializarDisplayAnalogico() {
  digitalWrite(TFT_CS3, LOW);
  tft3.fillScreen(ST77XX_BLACK);

  tft3.setTextColor(ST77XX_CYAN);
  tft3.setTextSize(1);
  tft3.setCursor(10, 5);  tft3.print("S-METER");
  tft3.setCursor(78, 5);  tft3.print("MODUL");

  tft3.drawLine(IZQ_X, BASE_Y - NIVEL_MIN + AJUSTE_Y, IZQ_X, BASE_Y - NIVEL_MAX + AJUSTE_Y, ST77XX_WHITE);
  for (int nivel = NIVEL_MIN; nivel <= NIVEL_MAX; nivel += 2) {
    int x = IZQ_X + 8 + (nivel - NIVEL_MIN) / 6;
    int y = BASE_Y - nivel + AJUSTE_Y;
    tft3.drawPixel(x, y, ST77XX_WHITE);
  }
  for (int nivel = NIVEL_MIN; nivel <= NIVEL_MAX; nivel++) {
    int xFin = IZQ_X + 8 + (nivel - NIVEL_MIN) / 6;
    int y = BASE_Y - nivel + AJUSTE_Y;
    tft3.drawLine(IZQ_X, y, xFin, y, AZUL_S_METER);
  }
  tft3.setTextColor(ST77XX_WHITE);
  tft3.setCursor(5, BASE_Y - NIVEL_MAX + AJUSTE_Y - 3); tft3.print("+40");
  tft3.setCursor(5, BASE_Y - NIVEL_20  + AJUSTE_Y - 3); tft3.print("+20");
  tft3.setCursor(9, BASE_Y - NIVEL_S9  + AJUSTE_Y - 3); tft3.print("S9");
  tft3.setCursor(9, BASE_Y - NIVEL_S6  + AJUSTE_Y - 3); tft3.print("S6");
  tft3.setCursor(9, BASE_Y - NIVEL_S3  + AJUSTE_Y - 3); tft3.print("S3");
  tft3.setCursor(9, BASE_Y - NIVEL_S1  + AJUSTE_Y - 3); tft3.print("S1");

  tft3.drawLine(DER_X, BASE_Y - NIVEL_MIN + AJUSTE_Y, DER_X, BASE_Y - NIVEL_MAX + AJUSTE_Y, ST77XX_WHITE);
  for (int nivel = NIVEL_MIN; nivel <= NIVEL_MAX; nivel += 2) {
    int x = DER_X - 8 - (nivel - NIVEL_MIN) / 6;
    int y = BASE_Y - nivel + AJUSTE_Y;
    tft3.drawPixel(x, y, ST77XX_WHITE);
  }
  for (int nivel = NIVEL_MIN; nivel <= NIVEL_MAX; nivel++) {
    int xIni = DER_X - 8 - (nivel - NIVEL_MIN) / 6;
    int y = BASE_Y - nivel + AJUSTE_Y;
    float pr = (float)(nivel - NIVEL_MIN) / (NIVEL_MAX - NIVEL_MIN);
    int pct = pr * 100;
    uint16_t c = (pct <= 70) ? VERDE_MOD : (pct <= 90) ? AMARILLO_MOD : ROJO_MOD;
    tft3.drawLine(xIni, y, DER_X, y, c);
  }
  tft3.setTextColor(ST77XX_WHITE);
  tft3.setCursor(108, BASE_Y - NIVEL_MAX + AJUSTE_Y - 3); tft3.print("100");
  int n75 = NIVEL_MIN + (NIVEL_MAX - NIVEL_MIN) * 0.75;
  int n50 = NIVEL_MIN + (NIVEL_MAX - NIVEL_MIN) * 0.50;
  int n25 = NIVEL_MIN + (NIVEL_MAX - NIVEL_MIN) * 0.25;
  tft3.setCursor(110, BASE_Y - n75 + AJUSTE_Y - 3); tft3.print("75");
  tft3.setCursor(110, BASE_Y - n50 + AJUSTE_Y - 3); tft3.print("50");
  tft3.setCursor(110, BASE_Y - n25 + AJUSTE_Y - 3); tft3.print("25");
  tft3.setCursor(110, BASE_Y - NIVEL_MIN + AJUSTE_Y - 3); tft3.print("0");

  digitalWrite(TFT_CS3, HIGH);
  display3Inicializado = true;
  ultimoNivelS = -1;
  ultimoNivelVU = -1;
}

static inline int yDeNivel(int n) { return BASE_Y - n + AJUSTE_Y; }
static inline int xFinS(int n)    { return IZQ_X + 8 + (n - NIVEL_MIN) / 6; }
static inline int xIniVU(int n)   { return DER_X - 8 - (n - NIVEL_MIN) / 6; }

void drawRowS(int y) {
  int n = BASE_Y - y + AJUSTE_Y;
  int nClamped = constrain(n, NIVEL_MIN, NIVEL_MAX);
  int xF = xFinS(nClamped);

  tft3.fillRect(IZQ_X, y, 35, 1, ST77XX_BLACK);
  tft3.fillRect(IZQ_X, y, xF - IZQ_X + 1, 1, AZUL_S_METER);
}

void drawRowVU(int y) {
  int n = BASE_Y - y + AJUSTE_Y;
  int nClamped = constrain(n, NIVEL_MIN, NIVEL_MAX);
  int xI = xIniVU(nClamped);
  float pr = (float)(nClamped - NIVEL_MIN) / (NIVEL_MAX - NIVEL_MIN);
  int pct = pr * 100;
  uint16_t c = (pct <= 70) ? VERDE_MOD : (pct <= 90) ? AMARILLO_MOD : ROJO_MOD;

  tft3.fillRect(70, y, 40, 1, ST77XX_BLACK);
  tft3.fillRect(xI, y, DER_X - xI + 1, 1, c);
}

void actualizarIndicadores() {
  if (!display3Inicializado) return;

  if (sMeterNivel != ultimoNivelS) {
    digitalWrite(TFT_CS3, LOW);
    
    int yB_old = yDeNivel(ultimoNivelS);
    int yB_new = yDeNivel(sMeterNivel);
    
    int minY = min(yB_old, yB_new) - AGUJA_UP;
    int maxY = max(yB_old, yB_new) + AGUJA_DOWN;
    
    for (int y = minY; y <= maxY; y++) {
      drawRowS(y);
    }
    
    int xF_new = xFinS(sMeterNivel) - 1;
    for (int o = -AGUJA_UP; o <= AGUJA_DOWN; o++) {
      tft3.drawLine(IZQ_X, yB_new + o, xF_new, yB_new + o, ST77XX_WHITE);
    }
    
    digitalWrite(TFT_CS3, HIGH);
    ultimoNivelS = sMeterNivel;
  }

  if (vuMeterNivel != ultimoNivelVU) {
    digitalWrite(TFT_CS3, LOW);
    
    int yB_old = yDeNivel(ultimoNivelVU);
    int yB_new = yDeNivel(vuMeterNivel);
    
    int minY = min(yB_old, yB_new) - AGUJA_UP;
    int maxY = max(yB_old, yB_new) + AGUJA_DOWN;
    
    for (int y = minY; y <= maxY; y++) {
      drawRowVU(y);
    }
    
    int xI_new = xIniVU(vuMeterNivel) + 1;
    for (int o = -AGUJA_UP; o <= AGUJA_DOWN; o++) {
      tft3.drawLine(xI_new, yB_new + o, DER_X, yB_new + o, AZUL_INDICADOR);
    }
    
    digitalWrite(TFT_CS3, HIGH);
    ultimoNivelVU = vuMeterNivel;
  }
}

// ==================== PERSISTENCIA ====================
void marcarConfigSucia() {
  configSucia = true;
  configCambioMs = millis();
}

void guardarConfig() {
  EEPROM.put(EE_FRECUENCIA, frecuenciaBase);
  EEPROM.put(EE_CLARIFICADOR, clarificador);
  EEPROM.write(EE_BFO, bfoActivo ? 1 : 0);
  EEPROM.write(EE_PASO, indicePaso(paso));
  EEPROM.write(EE_EN_CLAR, enClar ? 1 : 0);
  EEPROM.write(EE_MAGIC, EE_MAGIC_VAL);
  EEPROM.commit();
  configSucia = false;
}

void cargarConfig() {
  if (EEPROM.read(EE_MAGIC) != EE_MAGIC_VAL) return;

  long f = 0; EEPROM.get(EE_FRECUENCIA, f);
  if (f >= RX_MIN && f <= RX_MAX) frecuenciaBase = f;

  long c = 0; EEPROM.get(EE_CLARIFICADOR, c);
  if (c >= -CLAR_MAX && c <= CLAR_MAX) clarificador = c;

  bfoActivo = (EEPROM.read(EE_BFO) != 0);
  paso      = pasoDesdeIndice(EEPROM.read(EE_PASO));
  enClar    = (EEPROM.read(EE_EN_CLAR) != 0) && bfoActivo;

  if (!bfoActivo) { enClar = false; clarificador = 0; }
}

// ==================== FRECUENCIA ====================
void recalcularFrecuencia() {
  frecuenciaRX = frecuenciaBase + ((rxMode && bfoActivo) ? clarificador : 0);
}

void limitarFrecuencia() {
  if (rxMode) {
    if (frecuenciaBase < RX_MIN) frecuenciaBase = RX_MIN;
    if (frecuenciaBase > RX_MAX) frecuenciaBase = RX_MAX;
  } else {
    if (frecuenciaBase < TX_MIN) frecuenciaBase = TX_MIN;
    if (frecuenciaBase > TX_MAX) frecuenciaBase = TX_MAX;
  }
}

// ==================== SI5351 ====================
static void clkAplicar(uint8_t n, bool on, uint64_t f) {
  si5351_clock ck = (n == 0) ? SI5351_CLK0 : (n == 1) ? SI5351_CLK1 : SI5351_CLK2;
  if (on) {
    if (f != clkCache[n].freq) {
      uint8_t r = si5351.set_freq(f, ck);
      if (r == 0) clkCache[n].freq = f;
    }
    if (!clkCache[n].on) { si5351.output_enable(ck, true); clkCache[n].on = true; }
  } else {
    if (clkCache[n].on)  { si5351.output_enable(ck, false); clkCache[n].on = false; }
  }
}

void actualizarModoRxTx() {
  if (calActivo) {
    clkAplicar(0, true,  (uint64_t)frecuenciaBase * 100ULL);
    clkAplicar(1, true,  (uint64_t)(frecuenciaBase + FI_455) * 100ULL);
    clkAplicar(2, false, 0);
  }
  else if (rxMode) {
    clkAplicar(0, false, 0);
    clkAplicar(1, true,  (uint64_t)(frecuenciaBase + FI_455) * 100ULL);
    if (bfoActivo) {
      clkAplicar(2, true, (uint64_t)(FI_455 + clarificador) * 100ULL);
    } else {
      clkAplicar(2, false, 0);
    }
  }
  else {
    clkAplicar(1, false, 0);
    clkAplicar(2, false, 0);
    if (!modoInvalido) clkAplicar(0, true, (uint64_t)frecuenciaBase * 100ULL);
    else               clkAplicar(0, false, 0);
  }
}

// ==================== DISPLAYS 1 y 2 ====================
void actualizarPantallas() {
  int d[7];
  d[0] = frecuenciaRX / 1000000;
  d[1] = (frecuenciaRX / 100000) % 10;
  d[2] = (frecuenciaRX / 10000) % 10;
  d[3] = (frecuenciaRX / 1000) % 10;
  d[4] = (frecuenciaRX / 100) % 10;
  d[5] = (frecuenciaRX / 10) % 10;
  d[6] = frecuenciaRX % 10;

  bool enPaso = (!enClar && rxMode);
  bool naranja = (rxMode && bfoActivo && clarificador != 0);

  uint16_t col[7];
  col[0] = ST77XX_WHITE;
  col[1] = (paso == 100000 && enPaso) ? CELESTE_CLARO : ST77XX_WHITE;
  col[2] = (paso == 10000  && enPaso) ? CELESTE_CLARO : ST77XX_WHITE;
  col[3] = (paso == 1000   && enPaso) ? CELESTE_CLARO : ST77XX_WHITE;
  
  col[4] = naranja ? NARANJA : (enClar ? CELESTE_CLARO : ST77XX_WHITE);
  col[5] = naranja ? NARANJA : (enClar ? CELESTE_CLARO : ST77XX_WHITE);
  col[6] = naranja ? NARANJA : (enClar ? CELESTE_CLARO : ST77XX_WHITE);

  int sel = memActivo ? memoriaSeleccionada : -1;
  bool selCambio = (uiForzar || sel != uiMemSel);

  digitalWrite(TFT_CS1, LOW);
  if (uiForzar) {
    tft1.fillScreen(ST77XX_BLACK);
    tft1.setFont(&FreeMono9pt7b);
    tft1.setTextColor(CELESTE_CLARO);
    tft1.setCursor(0, 15);
    tft1.print("LW3DLP");
  }
  for (int i = 0; i < 4; i++) {
    if (uiForzar || d[i] != uiDig[i] || col[i] != uiDigCol[i]) {
      tft1.setFont(&FreeMonoBold24pt7b);
      tft1.fillRect(16 + i * 28, 20, 28, 45, ST77XX_BLACK); 
      tft1.setTextColor(col[i]);
      tft1.setCursor(16 + i * 28, 60);
      tft1.print(d[i]);
      uiDig[i] = d[i];
      uiDigCol[i] = col[i];
    }
  }
  if (selCambio) {
    tft1.setFont(&FreeMonoBold9pt7b);
    tft1.setTextColor(sel == 0 ? CELESTE_CLARO : BLANCO_SUAVE, ST77XX_BLACK);
    tft1.setCursor(20, 97); tft1.print("M1");
    tft1.setTextColor(sel == 1 ? CELESTE_CLARO : BLANCO_SUAVE, ST77XX_BLACK);
    tft1.setCursor(90, 97); tft1.print("M2");
  }
  for (int i = 0; i < 2; i++) {
    int v = memoriaGrabada[i] ? (int)(memorias[i] / 1000) : -1;
    if (uiForzar || v != uiMemVal[i]) {
      tft1.setFont(&FreeMono9pt7b);
      tft1.setTextColor(BLANCO_SUAVE, ST77XX_BLACK);
      tft1.setCursor((i == 0) ? 9 : 79, 117);
      if (v >= 0) tft1.print(v); else tft1.print("    ");
      uiMemVal[i] = v;
    }
  }
  digitalWrite(TFT_CS1, HIGH);

  digitalWrite(TFT_CS2, LOW);
  if (uiForzar) tft2.fillScreen(ST77XX_BLACK);

  int tag = getModoTag();
  bool redrawTag = uiForzar || (tag != uiTagModo) || (tag == TAG_CLAR && clarificador != uiClarVal);
  if (redrawTag) {
    char buf[16];
    const char* txt;
    uint16_t tcol;
    switch (tag) {
      case TAG_BFO:   txt = "BFO"; tcol = NARANJA; break;
      case TAG_CLAR:
        snprintf(buf, sizeof(buf), "CLR%+d", (int)clarificador);
        txt = buf; tcol = NARANJA; break;
      case TAG_MEM:   txt = "MEM"; tcol = CELESTE_CLARO; break;
      case TAG_CAL:   txt = "CAL"; tcol = ST77XX_RED; break;
      case TAG_RESET: txt = "RESET"; tcol = ST77XX_RED; break;
      case TAG_NOTX:  txt = "noTX"; tcol = ST77XX_RED; break;
      default:        txt = " "; tcol = CELESTE_CLARO; break;
    }
    
    tft2.fillRect(0, 0, 87, 18, ST77XX_BLACK); 
    
    tft2.setFont(&FreeMono9pt7b);
    tft2.setTextColor(tcol, ST77XX_BLACK);
    tft2.setCursor(0, 15);
    tft2.print(txt);
    uiTagModo = tag;
    uiClarVal = clarificador;
  }

  int band = bfoActivo ? 1 : 0;
  if (uiForzar || band != uiTagBand) {
    tft2.fillRect(85, 12, 43, 16, ST77XX_BLACK);
    tft2.setFont(&FreeMono9pt7b);
    if (band) {
      tft2.setTextColor(NARANJA, ST77XX_BLACK);
      tft2.setCursor(90, 25); tft2.print("BLU");
    } else {
      tft2.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
      tft2.setCursor(90, 25); tft2.print("AM ");
    }
    uiTagBand = band;
  }

  for (int i = 4; i < 7; i++) {
    if (uiForzar || d[i] != uiDig[i] || col[i] != uiDigCol[i]) {
      tft2.setFont(&FreeMonoBold24pt7b);
      tft2.fillRect((i - 4) * 28, 20, 28, 45, ST77XX_BLACK); 
      tft2.setTextColor(col[i]);
      tft2.setCursor((i - 4) * 28, 60);
      tft2.print(d[i]);
      uiDig[i] = d[i];
      uiDigCol[i] = col[i];
    }
  }

  if (selCambio) {
    tft2.setFont(&FreeMonoBold9pt7b);
    tft2.setTextColor(sel == 2 ? CELESTE_CLARO : BLANCO_SUAVE, ST77XX_BLACK);
    tft2.setCursor(20, 97); tft2.print("M3");
    tft2.setTextColor(sel == 3 ? CELESTE_CLARO : BLANCO_SUAVE, ST77XX_BLACK);
    tft2.setCursor(90, 97); tft2.print("M4");
  }

  for (int i = 2; i < 4; i++) {
    int v = memoriaGrabada[i] ? (int)(memorias[i] / 1000) : -1;
    if (uiForzar || v != uiMemVal[i]) {
      tft2.setFont(&FreeMono9pt7b);
      tft2.setTextColor(BLANCO_SUAVE, ST77XX_BLACK);
      tft2.setCursor((i == 2) ? 9 : 79, 117);
      if (v >= 0) tft2.print(v); else tft2.print("    ");
      uiMemVal[i] = v;
    }
  }

  int rxtx = rxMode ? 0 : (modoInvalido ? 2 : 1);
  if (uiForzar || rxtx != uiTagRXTX) {
    tft2.fillRect(85, 32, 43, 16, ST77XX_BLACK);
    tft2.setFont(&FreeMono9pt7b);
    if (rxtx == 0) {
      tft2.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
      tft2.setCursor(90, 45); tft2.print("RX ");
    } else if (rxtx == 1) {
      tft2.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft2.setCursor(90, 45); tft2.print("TX ");
    } else {
      tft2.setTextColor(ST77XX_RED, ST77XX_BLACK);
      tft2.setCursor(90, 45); tft2.print("-- ");
    }
    uiTagRXTX = rxtx;
  }
  digitalWrite(TFT_CS2, HIGH);

  uiMemSel = sel;
  uiForzar = false;
}

// ==================== ENTRADA / SALIDA ====================
void limpiarModos() {
  if (calActivo) paso = pasoAntesCal;
  resetModo = false;
  calActivo = false;
  memActivo = false;
  bfoActivo = false;
  enClar = false;
  clarificador = 0;
}

void entrarBFO() {
  limpiarModos();
  bfoActivo = true;
  pasoAlEntrarBFO = paso;
  recalcularFrecuencia();
  marcarConfigSucia();
  actualizarModoRxTx();
  actualizarPantallas();
}

void salirBFO() {
  bfoActivo = false;
  enClar = false;
  clarificador = 0;
  paso = pasoAlEntrarBFO;
  recalcularFrecuencia();
  marcarConfigSucia();
  actualizarModoRxTx();
  actualizarPantallas();
}

void entrarMEM() {
  limpiarModos();
  memActivo = true;
  memoriaSeleccionada = 0;
  memGuardadoRealizado = false;
  recalcularFrecuencia();
  actualizarModoRxTx();
  actualizarPantallas();
}

void entrarCAL() {
  limpiarModos();
  calActivo = true;
  calFrecuenciaBase = frecuenciaBase;
  pasoAntesCal = paso;
  paso = 1;
  calGuardadoRealizado = false;
  recalcularFrecuencia();
  actualizarModoRxTx();
  actualizarPantallas();
}

void entrarRESET() {
  limpiarModos();
  resetModo = true;
  resetGuardadoRealizado = false;
  recalcularFrecuencia();
  actualizarModoRxTx();
  actualizarPantallas();
}

void confirmarReset() {
  calCorreccion = 0;
  guardarCorreccionSI5351(calCorreccion);
  resetModo = false;

  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, calCorreccion);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
  for (int i = 0; i < 3; i++) { clkCache[i].on = false; clkCache[i].freq = 0; }
  si5351.output_enable(SI5351_CLK0, false);
  si5351.output_enable(SI5351_CLK1, false);
  si5351.output_enable(SI5351_CLK2, false);

  recalcularFrecuencia();
  marcarConfigSucia();
  actualizarModoRxTx();
  actualizarPantallas();
}

void ciclarPaso() {
  if (!bfoActivo) {
    if (paso == 100000) paso = 10000;
    else if (paso == 10000) paso = 1000;
    else paso = 100000;
  } else {
    if (enClar) {
      enClar = false;
      paso = 100000;
    } else if (paso == 100000) {
      paso = 10000;
    } else if (paso == 10000) {
      paso = 1000;
    } else if (paso == 1000) {
      enClar = true;
    } else {
      paso = 100000;
    }
  }
  marcarConfigSucia();
  actualizarPantallas();
}

// ==================== SERIE ====================
void enviarEstado() {
  Serial.print("F,");
  Serial.print(frecuenciaRX);
  Serial.print(",");
  Serial.print(rxMode ? "RX" : "TX");
  Serial.print(",");
  Serial.print(paso);
  Serial.print(",");
  Serial.print(sMeterNivel);
  Serial.print(",");
  Serial.print(vuMeterNivel);
  Serial.print(",");
  Serial.print(bfoActivo ? "1" : "0");
  Serial.print(",");
  Serial.print(memActivo ? "1" : "0");
  Serial.print(",");
  Serial.println(clarificador);
}

void procesarComandos() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || cmdIndex >= CMD_BUFFER_SIZE - 1) {
      cmdBuffer[cmdIndex] = '\0';
      cmdIndex = 0;
      if (cmdBuffer[0] == 'R') { enviarEstado(); }
      else if (cmdBuffer[0] == 'F' && cmdBuffer[1] == ',') {
        long nf = atol(cmdBuffer + 2);
        if (nf >= RX_MIN && nf <= RX_MAX) {
          frecuenciaBase = nf; clarificador = 0; enClar = false;
          recalcularFrecuencia(); marcarConfigSucia();
          actualizarModoRxTx(); actualizarPantallas(); enviarEstado();
        }
      }
      else if (cmdBuffer[0] == 'P' && cmdBuffer[1] == ',') {
        int np = atoi(cmdBuffer + 2);
        if (np == 1000 || np == 10000 || np == 100000) {
          paso = np; enClar = false;
          marcarConfigSucia(); actualizarPantallas(); enviarEstado();
        }
      }
      else if (cmdBuffer[0] == 'M' && cmdBuffer[1] == ',') {
        int m = atoi(cmdBuffer + 2);
        if (m == 0 && !rxMode) { rxMode = true;  actualizarModoRxTx(); actualizarPantallas(); enviarEstado(); }
        if (m == 1 &&  rxMode) { rxMode = false; actualizarModoRxTx(); actualizarPantallas(); enviarEstado(); }
      }
      else if (cmdBuffer[0] == 'B' && cmdBuffer[1] == ',') {
        int e = atoi(cmdBuffer + 2);
        if (e == 1 && !bfoActivo) entrarBFO();
        if (e == 0 &&  bfoActivo) salirBFO();
      }
      else if (cmdBuffer[0] == 'G' && cmdBuffer[1] == ',') {
        int idx = atoi(cmdBuffer + 2);
        if (idx >= 0 && idx < 4) {
          Serial.print("M,"); Serial.print(idx); Serial.print(",");
          Serial.println(memorias[idx]);
        }
      }
      else if (cmdBuffer[0] == 'L' && cmdBuffer[1] == ',') {
        char* resto = cmdBuffer + 2;
        char* idxStr = strtok(resto, ",");
        char* fStr = strtok(NULL, ",");
        if (idxStr && fStr) {
          int idx = atoi(idxStr);
          if (idx >= 0 && idx < 4 && memoriaGrabada[idx]) {
            cargarMemoria(idx);
            enviarEstado();
          }
        }
      }
    } else {
      cmdBuffer[cmdIndex++] = c;
    }
  }
}

// ==================== ENCODER ====================
void handleEncoder() {
  long pos = encoder.getCount();
  if (pos == 0) return;

  if (memActivo) {
    memoriaSeleccionada = (memoriaSeleccionada + (int)pos) % 4;
    if (memoriaSeleccionada < 0) memoriaSeleccionada += 4;
    encoder.setCount(0);
    actualizarPantallas();
    return;
  }
  if (calActivo) {
    frecuenciaBase += pos;
    limitarFrecuencia();
    encoder.setCount(0);
    recalcularFrecuencia();
    actualizarModoRxTx();
    actualizarPantallas();
    return;
  }
  if (resetModo) { encoder.setCount(0); return; }
  if (bfoActivo && enClar && rxMode) {
    clarificador += pos * CLAR_PASO;
    if (clarificador >  CLAR_MAX) clarificador =  CLAR_MAX;
    if (clarificador < -CLAR_MAX) clarificador = -CLAR_MAX;
    encoder.setCount(0);
    marcarConfigSucia();
    recalcularFrecuencia();
    actualizarModoRxTx();
    actualizarPantallas();
    return;
  }

  long cambio;
  long a = labs(pos);
  if (a >= 5) cambio = (pos > 0) ? 100000 : -100000;
  else if (a >= 2) cambio = (pos > 0) ? (long)paso * 10 : -(long)paso * 10;
  else cambio = (pos > 0) ? paso : -paso;

  frecuenciaBase += cambio;
  limitarFrecuencia();
  encoder.setCount(0);
  marcarConfigSucia();
  recalcularFrecuencia();
  actualizarModoRxTx();
  actualizarPantallas();
}

// ==================== PTT ====================
void handlePTT() {
  static int lastPTTState = HIGH;
  int pttReading = digitalRead(PTT_PIN);

  if (pttReading != lastPTTState) {
    delay(50);
    pttReading = digitalRead(PTT_PIN);
    if (pttReading != lastPTTState) {
      bool newRx = (pttReading == HIGH);
      if (newRx != rxMode) {
        rxMode = newRx;
        if (!rxMode) {
          if (calActivo) {
            calActivo = false;
            frecuenciaBase = calFrecuenciaBase;
            paso = pasoAntesCal;
          }
          memActivo = false;
          resetModo = false;
          botonPresionado = false;
          verificarRangoTX();
        }
        recalcularFrecuencia();
        actualizarModoRxTx();
        actualizarPantallas();
      }
      lastPTTState = pttReading;
    }
  }
}

// ==================== LOOP ====================
void loop() {
  static unsigned long ultimaLectura = 0;
  if (millis() - ultimaLectura > 40) {
    leerMedidores();
    ultimaLectura = millis();
  }
  actualizarIndicadores();

  if (configSucia && (millis() - configCambioMs > CONFIG_DEBOUNCE_MS)) {
    guardarConfig();
  }

  if (calActivo || resetModo || rxMode) {
    digitalWrite(TX_ENABLE_PIN, LOW);
    txEnableState = false;
    modoInvalido = false;
  } else {
    verificarRangoTX();
    if (!modoInvalido) {
      if (!txEnableState) { txEnableTime = millis(); txEnableState = true; }
      if (millis() - txEnableTime >= 100) digitalWrite(TX_ENABLE_PIN, HIGH);
    } else {
      digitalWrite(TX_ENABLE_PIN, LOW);
      txEnableState = false;
    }
  }

  if (botonPresionado && rxMode && !transicionEnPulsacion) {
    unsigned long t = millis() - pulsacionStartTime;

    if (modoAlPresionar == MODO_AM) {
      if      (t >= RESET_HOLD_MS && nivelAlcanzado < 4) { nivelAlcanzado = 4; entrarRESET(); }
      else if (t >= CAL_HOLD_MS   && nivelAlcanzado < 3) { nivelAlcanzado = 3; entrarCAL();   }
      else if (t >= MEM_HOLD_MS   && nivelAlcanzado < 2) { nivelAlcanzado = 2; entrarMEM();   }
      else if (t >= BFO_HOLD_MS   && nivelAlcanzado < 1) { nivelAlcanzado = 1; entrarBFO();   }
    }
    else {
      switch (modoAlPresionar) {
        case MODO_BFO:
          if (t >= BFO_HOLD_MS) { transicionEnPulsacion = true; salirBFO(); }
          break;
        case MODO_MEM:
          if (t >= GUARDADO_HOLD_MS && !memGuardadoRealizado) {
            memGuardadoRealizado = true;
            transicionEnPulsacion = true;
            guardarMemoria(memoriaSeleccionada, frecuenciaBase);
            memActivo = false;
            actualizarPantallas();
          }
          break;
        case MODO_CAL:
          if (t >= GUARDADO_HOLD_MS && !calGuardadoRealizado) {
            calGuardadoRealizado = true;
            transicionEnPulsacion = true;

            double delta = ((double)calFrecuenciaBase / (double)frecuenciaBase - 1.0) * 1e9;
            calCorreccion += (long)delta;
            if (calCorreccion >  1000000L) calCorreccion =  1000000L;
            if (calCorreccion < -1000000L) calCorreccion = -1000000L;
            guardarCorreccionSI5351(calCorreccion);

            calActivo = false;
            frecuenciaBase = calFrecuenciaBase;
            paso = pasoAntesCal;

            si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, calCorreccion);
            si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
            si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
            si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
            for (int i = 0; i < 3; i++) { clkCache[i].on = false; clkCache[i].freq = 0; }
            si5351.output_enable(SI5351_CLK0, false);
            si5351.output_enable(SI5351_CLK1, false);
            si5351.output_enable(SI5351_CLK2, false);

            recalcularFrecuencia();
            marcarConfigSucia();
            actualizarModoRxTx();
            actualizarPantallas();
          }
          break;
        case MODO_RESET:
          if (t >= GUARDADO_HOLD_MS && !resetGuardadoRealizado) {
            resetGuardadoRealizado = true;
            transicionEnPulsacion = true;
            confirmarReset();
          }
          break;
      }
    }
  }

  static int lastButtonState = HIGH;
  int buttonState = digitalRead(ENC_SW);

  if (buttonState != lastButtonState) {
    delay(debounceDelay);
    buttonState = digitalRead(ENC_SW);
    if (buttonState != lastButtonState) {
      if (buttonState == LOW) {
        botonPresionado = true;
        pulsacionStartTime = millis();
        modoAlPresionar = modoActual();
        transicionEnPulsacion = false;
        nivelAlcanzado = 0;
        if (modoAlPresionar == MODO_MEM)   memGuardadoRealizado = false;
        if (modoAlPresionar == MODO_CAL)   calGuardadoRealizado = false;
        if (modoAlPresionar == MODO_RESET) resetGuardadoRealizado = false;
      } else {
        if (botonPresionado && !transicionEnPulsacion && nivelAlcanzado == 0) {
          unsigned long t = millis() - pulsacionStartTime;
          if (t < CLICK_MAX_MS) {
            switch (modoAlPresionar) {
              case MODO_AM:
              case MODO_BFO:
                ciclarPaso();
                break;
              case MODO_MEM:
                if (memActivo) {
                  cargarMemoria(memoriaSeleccionada);
                  memActivo = false;
                  actualizarPantallas();
                }
                break;
              case MODO_CAL:
                if (calActivo) {
                  calActivo = false;
                  frecuenciaBase = calFrecuenciaBase;
                  paso = pasoAntesCal;
                  recalcularFrecuencia();
                  marcarConfigSucia();
                  actualizarModoRxTx();
                  actualizarPantallas();
                }
                break;
              case MODO_RESET:
                if (resetModo) {
                  resetModo = false;
                  actualizarModoRxTx();
                  actualizarPantallas();
                }
                break;
            }
          }
        }
        botonPresionado = false;
      }
      lastButtonState = buttonState;
    }
  }

  handleEncoder();
  handlePTT();
  procesarComandos();
  delay(10);
}

// ==================== RESTANTES ====================
void verificarRangoTX() {
  modoInvalido = (frecuenciaBase < TX_MIN || frecuenciaBase > TX_MAX);
}

void cargarMemoria(int idx) {
  if (!memoriaGrabada[idx]) return;
  frecuenciaBase = memorias[idx];
  clarificador = 0;
  enClar = false;
  bfoActivo = false;
  recalcularFrecuencia();
  marcarConfigSucia();
  actualizarModoRxTx();
  actualizarPantallas();
}

void guardarMemoria(int idx, long freq) {
  memorias[idx] = freq;
  memoriaGrabada[idx] = true;
  EEPROM.put(EE_MEM_BASE + idx * sizeof(long), freq);
  EEPROM.commit();
}

void cargarMemoriasEEPROM() {
  for (int i = 0; i < 4; i++) {
    EEPROM.get(EE_MEM_BASE + i * sizeof(long), memorias[i]);
    if (memorias[i] >= RX_MIN && memorias[i] <= RX_MAX) {
      memoriaGrabada[i] = true;
    } else {
      memoriaGrabada[i] = false;
      memorias[i] = 0;
    }
  }
}

void guardarCorreccionSI5351(long correccion) {
  EEPROM.put(EE_CAL, correccion);
  EEPROM.commit();
}

void cargarCorreccionSI5351() {
  EEPROM.get(EE_CAL, calCorreccion);
  if (calCorreccion > 1000000L || calCorreccion < -1000000L) calCorreccion = 0;
}