// Arduino header file
#include <Arduino.h>

// easyvr lib
#include <VoiceRecognitionV3.h>

// gprs lib
#include <gprs.h>

// for atomic access
#include <util/atomic.h>

// debug port
// #define APP_DEBUG_PORT Serial

// sesuaikan pinnya di file sim800.h
// default 7 dan 8
#define GPRS_DEV_ID "car11223344"
#define GPRS_APN "internet"
#define GPRS_USERNAME " "
#define GPRS_PASSWORD " "
#define GPRS_SERVER_URL "http://car-monitor.ex4-tech.id.or.id"
GPRS modem_sim900(19200);

// easyvr obj
// pin 3 dan 4
VR sound_recognizer(3, 4);
uint8_t easy_vr_status = 0, easyvr_result = 255;
uint32_t t_last_sound_recognition = 0;

// pin rf signal
#define RF_COUNT_THRESHOLD 10
#define PIN_RF_SIGNAL 2
// rf data counter
volatile uint16_t rf_data_counter = 0;
uint32_t t_last_rf_count_acquired = 0;

// timer var
uint32_t t_now;

// proto func
void APP_DEBUG_PRINT(String alog);
void init_rf_signal_pin_handler();
void init_easyvr();
void count_rf_signal();
void main_app_setup();
void main_app_loop();
void process_rf_signal();
void process_sound_recognition();

void APP_DEBUG_INIT() {
#ifdef APP_DEBUG_PORT
  APP_DEBUG_PORT.begin(19200);
#endif
}

/**
 * debug printing util
 * @method APP_DEBUG_PRINT
 * @param  alog            [description]
 */

void APP_DEBUG_PRINT(String alog) {
#ifdef APP_DEBUG_PORT
  char dtx[16] = {0};
  snprintf_P(dtx, sizeof(dtx), (const char *)F("%-10u : "), millis());
  APP_DEBUG_PORT.println(String(dtx) + alog);
#endif
}

// setup int0 rf signal
void init_rf_signal_pin_handler() {
  pinMode(PIN_RF_SIGNAL, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_RF_SIGNAL), count_rf_signal,
                  FALLING);
}

// init easyvr
void init_easyvr() {
  sound_recognizer.begin(9600);
  if ((easy_vr_status = sound_recognizer.clear()) == 0) {
    // register sound
    if (sound_recognizer.load((uint8_t)0) == 0) {
      // ok
      APP_DEBUG_PRINT(String("EASYVR LOAD SOUND OK"));
    } else {
      // fail
      easy_vr_status = 255;
      APP_DEBUG_PRINT(String("EASYVR LOAD SOUND FAIL"));
    }
  } else {
    APP_DEBUG_PRINT(String("EASYVR INIT FAIL"));
  }
}

// handler isr int0
void count_rf_signal() { rf_data_counter++; }

/**
 * process rf signal counter
 * @method process_rf_signal
 */
void process_rf_signal() {
  uint16_t cntx = 0;
  int resx = 0;
  char buff_out[64] = {0};

  if (t_now - t_last_rf_count_acquired >= 1000) {
    t_last_rf_count_acquired = t_now;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      cntx = rf_data_counter;
      rf_data_counter = 0;
    }

    // APP_DEBUG_PORT.println(cntx);
    if (cntx >= RF_COUNT_THRESHOLD) {
      // got speech hello?
      if (!((easy_vr_status == 0) && (easyvr_result == 0))) {
        // nope
        APP_DEBUG_PRINT(String("EASYVR NO WORD DETECTED"));
        return;
      }

      // open gprs connection
      if ((resx = modem_sim900.gprsOpenConnection(GPRS_APN, GPRS_USERNAME,
                                                  GPRS_PASSWORD)) == 0) {
        // gprs opened
        // build data
        String urlx = String(GPRS_SERVER_URL) + String("/api.php");
        String alarm_data =
            String("&id=") + String(GPRS_DEV_ID) + String("&c=1");

        APP_DEBUG_PRINT(String("GPRS SENDING DATA"));
        if ((resx = modem_sim900.gprsSendData(
                 HTTP_MODEM_GET, urlx.c_str(), alarm_data.c_str(),
                 sizeof(buff_out), buff_out)) == 0) {
          // send ok
          APP_DEBUG_PRINT(String("GPRS SEND OK"));
        } else {
          // send fail
          APP_DEBUG_PRINT(String("GPRS SEND FAIL = ") + String(resx));
        }
      } else {
        // fail opening gprs
        APP_DEBUG_PRINT(String("OPEN GPRS FAIL = ") + String(resx));
      }
    } else {
      APP_DEBUG_PRINT(String("RF SIGNAL BELOW THRESHOLD"));
    }
  }
}

/**
 * process sound recognition
 * @method process_sound_recognition
 */
void process_sound_recognition() {
  uint8_t bufx[64] = {0};

  if (t_now - t_last_sound_recognition >= 1000) {
    t_last_sound_recognition = t_now;
    if (sound_recognizer.recognize(bufx, 50) > 0) {
      // success
      easyvr_result = bufx[1];
      APP_DEBUG_PRINT(String("EASYVR SOUND RECOGNITION OK = ") +
                      String(easyvr_result));
    } else {
      // fail
      easyvr_result = 255;
      APP_DEBUG_PRINT(String("EASYVR SOUND RECOGNITION FAIL"));
    }
  }
}

// main app setup
void main_app_setup() {
  // debug serial start
  APP_DEBUG_INIT();

  APP_DEBUG_PRINT(String("INITIALIZING...."));

  // init easyvr
  init_easyvr();

  // setup int0 pin
  init_rf_signal_pin_handler();

  // init millis
  t_now = t_last_rf_count_acquired = t_last_sound_recognition = millis();

  APP_DEBUG_PRINT(String("INIT DONE"));
}

// main app loop
void main_app_loop() {
  // current millis
  t_now = millis();

  // process sound recognition
  process_sound_recognition();

  // process rf signal counter
  process_rf_signal();
}
