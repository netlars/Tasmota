/*
  xsns_128_ds2413.ino - DS2413 1-Wire Dual Channel Switch
  Optimiert für Tasmota 15.x - Koexistenz mit DS18B20
*/

#ifdef ESP8266
#ifdef USE_DS2413

#define XSNS_128             128
#define DS2413_CHIPID        0x3A
#define DS2413_ACCESS_READ   0xF5

#ifndef DS2413_MAX_SENSORS
#define DS2413_MAX_SENSORS   3
#endif

// Extern deklarieren, falls xsns_05 nicht geladen wurde
extern uint8_t OneWireSearch(uint8_t *newAddr);

struct {
  uint8_t state;
  uint8_t address[8];
  uint8_t index;
  uint8_t valid;
  int8_t pins_id;
} ds2413_sensor[DS2413_MAX_SENSORS];

struct {
  int8_t pin = 0;
  int8_t pin_out = 0;
  bool dual_mode = false;
} ds2413_gpios[MAX_DSB];

struct {
  char name[17];
  uint8_t sensors;
  uint8_t gpios;
  uint8_t input_mode = 0;
  int8_t pin = 0;
  int8_t pin_out = 0;
  bool dual_mode = false;
} DS2413Data;

/*********************************************************************************************\
 * Low-Level 1-Wire Kommunikation
\*********************************************************************************************/

#define W1_MATCH_ROM         0x55
static uint8_t ds2413_last_state[DS2413_MAX_SENSORS];

uint8_t DS2413_OneWireReset(void) {
  uint8_t retries = 125;
  pinMode(DS2413Data.pin, DS2413Data.input_mode);
  do {
    if (--retries == 0) return 0;
    delayMicroseconds(2);
  } while (!digitalRead(DS2413Data.pin));
  pinMode(DS2413Data.pin, OUTPUT);
  digitalWrite(DS2413Data.pin, LOW);
  delayMicroseconds(480);
  pinMode(DS2413Data.pin, DS2413Data.input_mode);
  delayMicroseconds(70);
  uint8_t r = !digitalRead(DS2413Data.pin);
  delayMicroseconds(410);
  return r;
}

void DS2413_OneWireWriteBit(uint8_t v) {
  v &= 1;
  digitalWrite(DS2413Data.pin, LOW);
  pinMode(DS2413Data.pin, OUTPUT);
  delayMicroseconds(v ? 10 : 65);
  digitalWrite(DS2413Data.pin, HIGH);
  delayMicroseconds(v ? 55 : 5);
}

uint8_t DS2413_OneWireReadBit(void) {
  pinMode(DS2413Data.pin, OUTPUT);
  digitalWrite(DS2413Data.pin, LOW);
  delayMicroseconds(3);
  pinMode(DS2413Data.pin, DS2413Data.input_mode);
  delayMicroseconds(10);
  uint8_t r = digitalRead(DS2413Data.pin);
  delayMicroseconds(53);
  return r;
}

void DS2413_OneWireWrite(uint8_t v) {
  for (uint8_t bit_mask = 0x01; bit_mask; bit_mask <<= 1) {
    DS2413_OneWireWriteBit((bit_mask & v) ? 1 : 0);
  }
}

uint8_t DS2413_OneWireRead(void) {
  uint8_t r = 0;
  for (uint8_t bit_mask = 0x01; bit_mask; bit_mask <<= 1) {
    if (DS2413_OneWireReadBit()) r |= bit_mask;
  }
  return r;
}

/*********************************************************************************************\
 * Hauptfunktionen
\*********************************************************************************************/

bool Ds2413Read(uint8_t sensor) {
  uint8_t index = ds2413_sensor[sensor].index;
  DS2413Data.pin = ds2413_gpios[ds2413_sensor[index].pins_id].pin;

  if (DS2413_OneWireReset()) {
    DS2413_OneWireWrite(W1_MATCH_ROM);
    for (uint8_t i = 0; i < 8; i++) {
      DS2413_OneWireWrite(ds2413_sensor[index].address[i]);
    }
    DS2413_OneWireWrite(DS2413_ACCESS_READ);
    
    uint8_t res = DS2413_OneWireRead();
    // Validierung: Das Byte und sein Komplement müssen passen
    if ((res & 0x0F) == ((~res >> 4) & 0x0F)) {
      ds2413_sensor[index].state = res;
      ds2413_sensor[index].valid = SENSOR_MAX_MISS;
      return true;
    }
  }
  return false;
}

void Ds2413Show(bool json) {
  for (uint32_t i = 0; i < DS2413Data.sensors; i++) {
    if (ds2413_sensor[i].valid) {
      uint8_t s = ds2413_sensor[i].state;
      int pioA = (s & 0x01) ? 1 : 0;
      int pioB = (s & 0x04) ? 1 : 0;

      if (json) {
        char address[17];
        for (uint32_t j = 0; j < 6; j++) sprintf(address+2*j, "%02X", ds2413_sensor[i].address[6-j]);
        ResponseAppend_P(PSTR(",\"DS2413-%d\":{\"" D_JSON_ID "\":\"%s\",\"A\":%d,\"B\":%d}"), i + 1, address, pioA, pioB);
      } else {
#ifdef USE_WEBSERVER
        WSContentSend_P(PSTR("<tr><th>DS2413-%d A/B</th><td>%d / %d</td></tr>"), i + 1, pioA, pioB);
#endif
      }
    }
  }
}

/*********************************************************************************************\
 * Tasmota Sensor Interface
\*********************************************************************************************/

bool Xsns128(uint32_t function) {
  bool result = false;
  if (PinUsed(GPIO_DSB, GPIO_ANY)) {
    switch (function) {
      case FUNC_INIT:
        DS2413Data.sensors = 0;
        break;

      case FUNC_EVERY_SECOND:
        // SCHRITT 1: Verzögerte Suche (nach 12 Sek.), wenn noch nichts gefunden wurde
        if (TasmotaGlobal.uptime == 12 && DS2413Data.sensors == 0) {
          DS2413Data.input_mode = Settings->flag3.ds18x20_internal_pullup ? INPUT_PULLUP : INPUT;
          DS2413Data.gpios = 0;
          
          for (uint32_t pins = 0; pins < MAX_DSB; pins++) {
            if (PinUsed(GPIO_DSB, pins)) {
              ds2413_gpios[pins].pin = Pin(GPIO_DSB, pins);
              DS2413Data.gpios++;
            }
          }

          for (uint32_t pins = 0; pins < DS2413Data.gpios; pins++) {
            DS2413Data.pin = ds2413_gpios[pins].pin;
            uint8_t addr[8];
            // Suche mit der Core-Funktion
            while (DS2413Data.sensors < DS2413_MAX_SENSORS) {
              if (!OneWireSearch(addr)) break;
              if (addr[0] == DS2413_CHIPID) {
                memcpy(ds2413_sensor[DS2413Data.sensors].address, addr, 8);
                ds2413_sensor[DS2413Data.sensors].index = DS2413Data.sensors;
                ds2413_sensor[DS2413Data.sensors].pins_id = pins;
                ds2413_sensor[DS2413Data.sensors].valid = SENSOR_MAX_MISS;
                DS2413Data.sensors++;
              }
            }
          }
          if (DS2413Data.sensors > 0) {
            AddLog(LOG_LEVEL_INFO, PSTR("DS2413: Sensoren nach Wartezeit registriert"));
          }
        }

        // SCHRITT 2: Normales Auslesen (nur jede 2. Sekunde zur Bus-Schonung)
        if (TasmotaGlobal.uptime > 15 && TasmotaGlobal.uptime % 2 == 0) {
          for (uint32_t i = 0; i < DS2413Data.sensors; i++) {
            Ds2413Read(i);
          }
        }
        break;

      case FUNC_JSON_APPEND:
        Ds2413Show(1);
        break;

#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        Ds2413Show(0);
        break;
#endif
    }
  }
  return result;
}

#endif // USE_DS2413
#endif // ESP8266