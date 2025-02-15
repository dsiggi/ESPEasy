#include "_Plugin_Helper.h"
#ifdef USES_P117

// #######################################################################################################
// ############ Plugin 117 SCD30 I2C CO2, Humidity and Temperature Sensor ################################
// #######################################################################################################
// development version
// by: V0JT4
// this plugin is based on the Frogmore42 library
// written based code from https://github.com/Frogmore42/Sonoff-Tasmota/tree/development/lib/FrogmoreScd30

// Changelog:
//
// 2023-11-24 tonhuisman: Add Device flag for I2CMax100kHz as this sensor won't work at 400 kHz
// 2022-02-26 tonhuisman: Implement commands for get/set measurement interval, and a setting too. Bugfix.
// 2022-02-26 tonhuisman: Implement commands for auto/manual CO2 calibration, and setting for auto calibration
// 2021-11-20 tonhuisman: Implement multi-instance support (using PluginStruct)
// 2021-09 tonhuisman: Moved from ESPEasyPluginPlayground to main repository

// Commands:
//   SCDGETABC                 - shows automatic calibration period in days, 0 = disable
//   SCDGETALT                 - shows altitude compensation configuration in meters above sea level
//   SCDGETTMP                 - shows temperature offset in degrees C
//   SCDSETCALIBRATION,<0|1>   - Set Automatic calibration off or on
//   SCDSETFRC,<co2_ppm>       - Set the Forced ReCalibration value, range between 400 and 2000 ppm
//                               Turns off AutoCalibration mode!
//   SCDGETINTERVAL            - shows the current measuring interval
//   SCDSETINTERVAL,<interval> - Set the measuring interval, default 2 sec., range 2-1800 seconds.

# define PLUGIN_117
# define PLUGIN_ID_117         117
# define PLUGIN_NAME_117       "Gases - CO2 SCD30"
# define PLUGIN_VALUENAME1_117 "CO2"
# define PLUGIN_VALUENAME2_117 "Humidity"
# define PLUGIN_VALUENAME3_117 "Temperature"
# define PLUGIN_VALUENAME4_117 "CO2raw"

# include "./src/PluginStructs/P117_data_struct.h"

boolean Plugin_117(uint8_t function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      auto& dev = Device[++deviceCount];
      dev.Number         = PLUGIN_ID_117;
      dev.Type           = DEVICE_TYPE_I2C;
      dev.VType          = Sensor_VType::SENSOR_TYPE_QUAD;
      dev.FormulaOption  = true;
      dev.ValueCount     = 4;
      dev.SendDataOption = true;
      dev.TimerOption    = true;
      dev.PluginStats    = true;
      dev.I2CMax100kHz   = true; // Max 100 kHz allowed/supported
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_117);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_117));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_117));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_117));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_117));
      break;
    }

    case PLUGIN_I2C_HAS_ADDRESS:
    {
      success = event->Par1 == 0x61;
      break;
    }

    # if FEATURE_I2C_GET_ADDRESS
    case PLUGIN_I2C_GET_ADDRESS:
    {
      event->Par1 = 0x61;
      success     = true;
      break;
    }
    # endif // if FEATURE_I2C_GET_ADDRESS

    case PLUGIN_SET_DEFAULTS:
    {
      P117_MEASURE_INTERVAL                        = 2; // Default Measurement Interval
      ExtraTaskSettings.TaskDeviceValueDecimals[0] = 0; // CO2 values are integers
      ExtraTaskSettings.TaskDeviceValueDecimals[3] = 0;
      break;
    }

    case PLUGIN_WEBFORM_SHOW_I2C_PARAMS:
    {
      html_TR_TD();
      html_TD();
      addHtml(F("<span style=\"color:red\">Tools->Advanced->I2C ClockStretchLimit should be set in range 20 to 150 msec.</span>"));
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      addFormNumericBox(F("Altitude"), F("alt"), P117_SENSOR_ALTITUDE, 0, 2000);
      addUnit(F("0..2000 m"));

      addFormTextBox(F("Temp offset"), F("tmp"), toString(P117_TEMPERATURE_OFFSET, 2), 5);
      addUnit(F("&deg;C"));

      addFormNumericBox(F("Measurement Interval"), F("pinterval"), P117_MEASURE_INTERVAL, 2, 1800);
      addUnit(F("2..1800 sec."));

      addFormCheckBox(F("Automatic Self Calibration"), F("abc"), P117_AUTO_CALIBRATION == 1);
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      uint16_t alt = getFormItemInt(F("alt"));

      if (alt > 2000) { alt = 2000; }
      P117_SENSOR_ALTITUDE    = alt;
      P117_TEMPERATURE_OFFSET = getFormItemFloat(F("tmp"));
      P117_AUTO_CALIBRATION   = isFormItemChecked(F("abc")) ? 1 : 0;
      const uint16_t interval = getFormItemInt(F("pinterval"));
      P117_MEASURE_INTERVAL = constrain(interval, 2, 1800);
      success               = true;
      break;
    }
    case PLUGIN_INIT:
    {
      const uint16_t interval = P117_MEASURE_INTERVAL;
      P117_MEASURE_INTERVAL = constrain(interval, 2, 1800);
      initPluginTaskData(event->TaskIndex,
                         new (std::nothrow) P117_data_struct(P117_SENSOR_ALTITUDE, P117_TEMPERATURE_OFFSET, P117_AUTO_CALIBRATION == 1,
                                                             P117_MEASURE_INTERVAL));
      P117_data_struct *P117_data = static_cast<P117_data_struct *>(getPluginTaskData(event->TaskIndex));

      success = (nullptr != P117_data);

      break;
    }
    case PLUGIN_READ:
    {
      P117_data_struct *P117_data = static_cast<P117_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P117_data) {
        return success;
      }

      uint16_t scd30_CO2{};
      uint16_t scd30_CO2EAvg{};
      float    scd30_Humid{};
      float    scd30_Temp{};

      switch (P117_data->read_sensor(&scd30_CO2, &scd30_CO2EAvg, &scd30_Temp, &scd30_Humid))
      {
        case ERROR_SCD30_NO_ERROR:
          UserVar.setFloat(event->TaskIndex, 0, scd30_CO2EAvg);
          UserVar.setFloat(event->TaskIndex, 1, scd30_Humid);
          UserVar.setFloat(event->TaskIndex, 2, scd30_Temp);
          UserVar.setFloat(event->TaskIndex, 3, scd30_CO2);

          if (scd30_CO2EAvg > 5000) {
            addLog(LOG_LEVEL_INFO, F("SCD30: Sensor saturated! > 5000 ppm"));
          }
          break;
        case ERROR_SCD30_NO_DATA:
        case ERROR_SCD30_CRC_ERROR:
        case ERROR_SCD30_CO2_ZERO:
          break;
        default:
        {
          P117_data->softReset();
          break;
        }
      }
      success = true;
      break;
    }
    case PLUGIN_WRITE:
    {
      P117_data_struct *P117_data = static_cast<P117_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P117_data) {
        return success;
      }
      const String command = parseString(string, 1);
      uint16_t     value   = 0;
      String log;
      float  temp;

      if (equals(command, F("scdgetabc"))) {
        P117_data->getCalibrationType(&value);
        log    += concat(F("ABC: "), value);
        success = true;
      } else if (equals(command, F("scdgetalt"))) {
        P117_data->getAltitudeCompensation(&value);
        log    += concat(F("Altitude: "), value);
        success = true;
      } else if (equals(command, F("scdgettmp"))) {
        P117_data->getTemperatureOffset(&temp);
        log    += concat(F("Temp offset: "), toString(temp, 2));
        success = true;
      } else if (equals(command, F("scdsetcalibration")) && (event->Par1 >= 0) && (event->Par1 <= 1)) {
        P117_data->setCalibrationMode(event->Par1 == 1);
        P117_AUTO_CALIBRATION = event->Par1; // Update device configuration
        log                  += concat(F("Calibration: "), event->Par1 == 1 ? F("auto") : F("manual"));
        success               = true;
      } else if (equals(command, F("scdsetfrc")) && (event->Par1 >= 400) && (event->Par1 <= 2000)) {
        const int res = P117_data->setForcedRecalibrationFactor(event->Par1);
        log    += strformat(F("SCD30 Forced calibration: %d, result: %d"), event->Par1, res);
        success = true;
      } else if (equals(command, F("scdgetinterval"))) {
        P117_data->getMeasurementInterval(&value);
        log    += concat(F("Interval: "), value);
        success = true;
      } else if (equals(command, F("scdsetinterval")) && (event->Par1 >= 2) && (event->Par1 <= 1800)) {
        const int res = P117_data->setMeasurementInterval(event->Par1);
        P117_MEASURE_INTERVAL = event->Par1; // Update device configuration
        log                  += strformat(F("SCD30 Measurement Interval: %d, result: %d"),
                                          event->Par1, res);
        success = true;
      }

      if (success) {
        SendStatus(event, log);
      } else {
        addLog(LOG_LEVEL_ERROR, F("SCD30: Unknown command."));
      }
      break;
    }
  }
  return success;
}

#endif // ifdef USES_P117
