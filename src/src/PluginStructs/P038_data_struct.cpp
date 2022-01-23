#include "../PluginStructs/P038_data_struct.h"

#ifdef USES_P038

// **************************************************************************/
// Constructor
// **************************************************************************/
P038_data_struct::P038_data_struct(int8_t gpioPin, uint16_t ledCount, uint8_t stripType)
  : _gpioPin(gpioPin), _maxPixels(ledCount), _stripType(stripType) {}

// **************************************************************************/
// Destructor
// **************************************************************************/
P038_data_struct::~P038_data_struct() {
  if (isInitialized()) {
    delete Plugin_038_pixels;
    Plugin_038_pixels = nullptr;
  }
}

bool P038_data_struct::plugin_init(struct EventStruct *event) {
  bool success = false;

  if (!isInitialized()) {
    Plugin_038_pixels = new (std::nothrow) Adafruit_NeoPixel(_maxPixels,
                                                             _gpioPin,
                                                             (_stripType == P038_STRIP_TYPE_RGBW ? NEO_GRBW : NEO_GRB) + NEO_KHZ800);

    if (Plugin_038_pixels != nullptr) {
      Plugin_038_pixels->begin(); // This initializes the NeoPixel library.
      success = true;
    }
  }

  return success;
}

bool P038_data_struct::plugin_exit(struct EventStruct *event) {
  if (isInitialized()) {
    delete Plugin_038_pixels;
    Plugin_038_pixels = nullptr;
  }
  return true;
}

bool P038_data_struct::plugin_write(struct EventStruct *event, const String& string) {
  bool success = false;

  if (isInitialized()) {
    String log;

    if (loglevelActiveFor(LOG_LEVEL_INFO) &&
        log.reserve(64)) {
      log  = F("P038 : ");
      log += string;
    }

    String cmd = parseString(string, 1);

    if (cmd.equals(F("neopixel"))) { // NeoPixel
      Plugin_038_pixels->setPixelColor(event->Par1 - 1, Plugin_038_pixels->Color(event->Par2, event->Par3, event->Par4, event->Par5));
      Plugin_038_pixels->show();     // This sends the updated pixel color to the hardware.
      success = true;
    }

    // extra function to receive HSV values (i.e. homie controler)
    if (cmd.equals(F("neopixelhsv"))) { // NeoPixelHSV
      int rgbw[4];
      rgbw[3] = 0;

      log += HSV2RGBWorRGBandLog(event->Par2, event->Par3, event->Par4, rgbw);

      Plugin_038_pixels->setPixelColor(event->Par1 - 1, Plugin_038_pixels->Color(rgbw[0], rgbw[1], rgbw[2], rgbw[3]));
      Plugin_038_pixels->show(); // This sends the updated pixel color to the hardware.
      success = true;
    }

    if (cmd.equals(F("neopixelall"))) { // NeoPixelAll
      for (int i = 0; i < _maxPixels; i++) {
        Plugin_038_pixels->setPixelColor(i, Plugin_038_pixels->Color(event->Par1, event->Par2, event->Par3, event->Par4));
      }
      Plugin_038_pixels->show();
      success = true;
    }

    if (cmd.equals(F("neopixelallhsv"))) { // NeoPixelAllHSV
      int rgbw[4];
      rgbw[3] = 0;

      log += HSV2RGBWorRGBandLog(event->Par1, event->Par2, event->Par3, rgbw);

      for (int i = 0; i < _maxPixels; i++) {
        Plugin_038_pixels->setPixelColor(i, Plugin_038_pixels->Color(rgbw[0], rgbw[1], rgbw[2], rgbw[3]));
      }
      Plugin_038_pixels->show();
      success = true;
    }

    if (cmd.equals(F("neopixelline"))) {                      // NeoPixelLine
      int brightness;
      validIntFromString(parseString(string, 7), brightness); // Get 7th argument aka Par6

      for (int i = event->Par1 - 1; i < event->Par2; i++) {
        Plugin_038_pixels->setPixelColor(i, Plugin_038_pixels->Color(event->Par3, event->Par4, event->Par5, brightness));
      }
      Plugin_038_pixels->show();
      success = true;
    }

    if (cmd.equals(F("neopixellinehsv"))) { // NeoPixelLineHSV
      int rgbw[4];
      rgbw[3] = 0;

      log += HSV2RGBWorRGBandLog(event->Par3, event->Par4, event->Par5, rgbw);

      for (int i = event->Par1 - 1; i < event->Par2; i++) {
        Plugin_038_pixels->setPixelColor(i, Plugin_038_pixels->Color(rgbw[0], rgbw[1], rgbw[2], rgbw[3]));
      }
      Plugin_038_pixels->show();
      success = true;
    }
  }
  return success;
}

String P038_data_struct::HSV2RGBWorRGBandLog(float H, float S, float V, int rgbw[4]) {
  if (_stripType == P038_STRIP_TYPE_RGBW) { // RGBW
    HSV2RGBW(H, S, V, rgbw);
  } else {                                  // RGB
    HSV2RGB(H, S, V, rgbw);
  }
  String log;

  if (loglevelActiveFor(LOG_LEVEL_INFO)) {
    log += F(" HSV converted to RGB(W):");
    log += rgbw[0];
    log += ',';
    log += rgbw[1];
    log += ',';
    log += rgbw[2];
    log += ',';
    log += rgbw[3];
    addLog(LOG_LEVEL_INFO, log);
  }
  return log;
}

#endif // ifdef USES_P038
