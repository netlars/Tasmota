#ifndef _USER_CONFIG_OVERRIDE_H_
#define _USER_CONFIG_OVERRIDE_H_

// 1-Wire Treiber aktivieren
#ifndef USE_DS2413
  #define USE_DS2413             // Treiber für DS2413 Fensterkontakte
#endif

#ifndef USE_DS18X20
  #define USE_DS18X20            // Treiber für DS18B20 Temperatur
#endif

#undef USE_ADC_VCC              // Deaktiviert VCC Messung
#undef USE_I2C                  // Deaktiviert den kompletten I2C-Support (spart viel Platz!)
#undef USE_SPI                  // Deaktiviert SPI-Support
#undef USE_MHZ19                // Deaktiviert CO2 Sensor
#undef USE_SENSEAIR             // Deaktiviert SenseAir
#undef USE_CCS811               // Deaktiviert CCS811
#undef USE_BME280               // Deaktiviert BME280/BMP280
#undef USE_DOMOTICZ             // Deaktiviert Domoticz Unterstützung
#undef USE_HOME_ASSISTANT       // Deaktiviert Home Assistant Discovery
#undef USE_KNX                  // Deaktiviert KNX (sehr groß!)
#undef USE_DISCOVERY            // Deaktiviert mDNS Discovery
#undef USE_BTC_TLS              // Deaktiviert TLS für MQTT/HTTP
#undef USE_WEBSERVER_SSL        // Deaktiviert SSL für den Webserver
#undef USE_RULES                // Keine Rules
#endif
