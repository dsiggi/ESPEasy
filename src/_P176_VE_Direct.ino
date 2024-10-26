#include "_Plugin_Helper.h"
#ifdef USES_P176

// #######################################################################################################
// ############################# Plugin 176: Communication - Victron VE.Direct ###########################
// #######################################################################################################

/**
 * 2024-10-26 tonhuisman: Add setting for RX buffer size and optional Debug logging. Add Quit log to suppress most logging
 *                        Store original received data names, to show in Device configuration, add default decimals to conversion factors
 *                        Improved serial processing reading per line instead of per data-block, moved to 50/sec
 * 2024-10-22 tonhuisman: Add checksum handling,
 *                        Add conversion factors to get more usable values like V, A, Ah, %. Based on VE.Direct protocol manual v. 3.3
 * 2024-10-20 tonhuisman: Start plugin for Victron VE.Direct protocol reader, based on Victron documentation
 **/

# define PLUGIN_176
# define PLUGIN_ID_176          176
# define PLUGIN_NAME_176        "Communication - Victron VE.Direct"
# define PLUGIN_VALUENAME1_176  "V"   // battery_voltage
# define PLUGIN_VALUENAME2_176  "I"   // battery_current
# define PLUGIN_VALUENAME3_176  "P"   // instantaneous_power
# define PLUGIN_VALUENAME4_176  "ERR" // error_code

# include "./src/PluginStructs/P176_data_struct.h"

# include <GPIO_Direct_Access.h>

boolean Plugin_176(uint8_t function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number       = PLUGIN_ID_176;
      Device[deviceCount].Type           = DEVICE_TYPE_SERIAL;
      Device[deviceCount].VType          = Sensor_VType::SENSOR_TYPE_QUAD;
      Device[deviceCount].Ports          = 0;
      Device[deviceCount].FormulaOption  = true;
      Device[deviceCount].ValueCount     = 4;
      Device[deviceCount].SendDataOption = true;
      Device[deviceCount].TimerOption    = true;
      Device[deviceCount].TimerOptional  = true;
      Device[deviceCount].PluginStats    = true;

      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_176);

      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_176));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_176));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_176));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_176));

      break;
    }

    case PLUGIN_SET_DEFAULTS:
    {
      CONFIG_PORT = static_cast<int>(ESPEasySerialPort::serial1); // Serial1 port
      int rxPin                    = 0;
      int txPin                    = 0;
      const ESPEasySerialPort port = static_cast<ESPEasySerialPort>(CONFIG_PORT);

      ESPeasySerialType::getSerialTypePins(port, rxPin, txPin);
      CONFIG_PIN1          = rxPin;
      CONFIG_PIN2          = txPin;
      P176_SERIAL_BAUDRATE = P176_DEFAULT_BAUDRATE;
      P176_SERIAL_BUFFER   = P176_DEFAULT_BUFFER;
      P176_RX_WAIT         = P176_DEFAULT_RX_WAIT;
      P176_SET_FAIL_CHECKSUM(P176_DEFAULT_FAIL_CHECKSUM);
      P176_SET_LED_PIN(-1);
      P176_SET_QUIET_LOG(true);
    }

    case PLUGIN_WEBFORM_SHOW_CONFIG:
    {
      string += serialHelper_getSerialTypeLabel(event);
      success = true;
      break;
    }

    case PLUGIN_GET_DEVICEGPIONAMES:
    {
      serialHelper_getGpioNames(event);
      break;
    }

    case PLUGIN_WEBFORM_SHOW_GPIO_DESCR:
    {
      string = strformat(F("LED: %s"), formatGpioLabel(P176_GET_LED_PIN, false).c_str());

      if (validGpio(P176_GET_LED_PIN) && P176_GET_LED_INVERTED) {
        string += F(" (inv)");
      }
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      addFormNumericBox(F("Baud Rate"), F("pbaud"), P176_SERIAL_BAUDRATE, 0);
      uint8_t serialConfChoice = serialHelper_convertOldSerialConfig(P176_SERIAL_CONFIG);
      serialHelper_serialconfig_webformLoad(event, serialConfChoice);

      addFormNumericBox(F("RX Receive Timeout"), F("prxwait"), P176_RX_WAIT, 0, 200);
      addUnit(F("mSec"));

      addFormNumericBox(F("RX buffersize"), F("pbuffer"), P176_SERIAL_BUFFER, 128, 2048);
      addUnit(F("128..2048"));

      # if P176_FAIL_CHECKSUM
      addFormCheckBox(F("Ignore data on checksum error"), F("pchksm"), P176_GET_FAIL_CHECKSUM);
      # endif // if P176_FAIL_CHECKSUM

      { // Led settings
        addFormSubHeader(F("Led"));

        addFormPinSelect(PinSelectPurpose::Generic_output, F("Led pin"), F("pledpin"), P176_GET_LED_PIN);
        addFormCheckBox(F("Led inverted"), F("pledinv"), P176_GET_LED_INVERTED);
      }

      {
        P176_data_struct *P176_data = static_cast<P176_data_struct *>(getPluginTaskData(event->TaskIndex));

        if ((nullptr != P176_data) && (P176_data->plugin_size_current_data() > 0)) {
          addFormSubHeader(F("Current data"));
          addRowLabel(F("Recently received data"));
          P176_data->plugin_show_current_data();
        }
      }
      # if P176_DEBUG
      addFormSubHeader(F("Logging"));
      addFormCheckBox(F("Enable logging (for debug)"),  F("pdebug"), P176_GET_DEBUG_LOG);
      # endif // if P176_DEBUG
      addFormCheckBox(F("Quiet logging (minimal log)"), F("pquiet"), P176_GET_QUIET_LOG);
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      P176_SERIAL_BAUDRATE = getFormItemInt(F("pbaud"));
      P176_SERIAL_CONFIG   = serialHelper_serialconfig_webformSave();
      P176_RX_WAIT         = getFormItemInt(F("prxwait"));
      P176_SERIAL_BUFFER   = getFormItemInt(F("pbuffer"));
      P176_SET_LED_PIN(getFormItemInt(F("pledpin")));
      P176_SET_LED_INVERTED(isFormItemChecked(F("pledinv")));
      # if P176_FAIL_CHECKSUM
      P176_SET_FAIL_CHECKSUM(isFormItemChecked(F("pchksm")));
      # endif // if P176_FAIL_CHECKSUM
      # if P176_DEBUG
      P176_SET_DEBUG_LOG(isFormItemChecked(F("pdebug")));
      # endif // if P176_DEBUG
      P176_SET_QUIET_LOG(isFormItemChecked(F("pquiet")));

      success = true;
      break;
    }

    case PLUGIN_INIT:
    {
      initPluginTaskData(event->TaskIndex, new (std::nothrow) P176_data_struct(event));
      P176_data_struct *P176_data = static_cast<P176_data_struct *>(getPluginTaskData(event->TaskIndex));

      success = (nullptr != P176_data) && P176_data->init();
      int8_t _ledPin = P176_GET_LED_PIN;

      if (validGpio(_ledPin)) {
        DIRECT_PINMODE_OUTPUT(_ledPin);
        DIRECT_pinWrite(_ledPin, P176_GET_LED_INVERTED ? 1 : 0); // Led off
      }

      break;
    }

    case PLUGIN_READ:
    {
      P176_data_struct *P176_data = static_cast<P176_data_struct *>(getPluginTaskData(event->TaskIndex));

      success = (nullptr != P176_data) && P176_data->plugin_read(event);

      break;
    }

    case PLUGIN_FIFTY_PER_SECOND:
    {
      P176_data_struct *P176_data = static_cast<P176_data_struct *>(getPluginTaskData(event->TaskIndex));

      success = (nullptr != P176_data) && P176_data->plugin_fifty_per_second(event);

      break;
    }

    case PLUGIN_GET_CONFIG_VALUE:
    {
      P176_data_struct *P176_data = static_cast<P176_data_struct *>(getPluginTaskData(event->TaskIndex));

      success = (nullptr != P176_data) && P176_data->plugin_get_config_value(event, string);

      break;
    }
  }
  return success;
}

#endif // USES_P176
