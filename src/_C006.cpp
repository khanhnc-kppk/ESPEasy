#include "src/Helpers/_CPlugin_Helper.h"
#ifdef USES_C006

// #######################################################################################################
// ########################### Controller Plugin 006: ThingsBoard MQTT ###################################
// #######################################################################################################

/** Changelog:
 * 2025-02-25 khanhnc: use this place for ThingsBoard MQTT instead PiDome MQTT
 * 2023-08-18 tonhuisman: Clean up source for pull request
 * 2023-03-15 tonhuisman: Handle setting payload to (Dummy) Devices via topic SysName/TaskName/ValueName/set
 * 2023-03 Changelog started
 */
// # include "src/Commands/InternalCommands.h"
# include "src/Commands/ExecuteCommand.h"
# include "src/ESPEasyCore/Controller.h"
# include "src/Globals/Settings.h"
# include "src/Helpers/_CPlugin_Helper_mqtt.h"
# include "src/Helpers/Network.h"
# include "src/Helpers/PeriodicalActions.h"
# include "_Plugin_Helper.h"
# include "src/ESPEasyCore/ESPEasyGPIO.h"
# include "src/ESPEasyCore/ESPEasyRules.h"
# include "src/Helpers/StringParser.h"

# include "src/Helpers/_CPlugin_DomoticzHelper.h"
# include <ArduinoJson.h>

# define CPLUGIN_006
# define CPLUGIN_ID_006         6
# define CPLUGIN_NAME_006       "ThingsBoard MQTT"

String CPlugin_006_pubname;
bool   CPlugin_006_mqtt_retainFlag = false;

bool C006_parse_command(struct EventStruct *event);

bool CPlugin_006(CPlugin::Function function, struct EventStruct *event, String& string)
{
  bool success = false;

  switch (function)
  {
    case CPlugin::Function::CPLUGIN_PROTOCOL_ADD:
    {
      ProtocolStruct& proto = getProtocolStruct(event->idx); //      = CPLUGIN_ID_006;
      proto.usesMQTT     = true;
      proto.usesTemplate = true;
      proto.usesAccount  = true;
      proto.usesPassword = true;
      proto.usesExtCreds = true;
      proto.defaultPort  = 1883;
      proto.usesID       = false;
      #if FEATURE_MQTT_TLS
      proto.usesTLS      = true;
      #endif
      break;
    }

    case CPlugin::Function::CPLUGIN_GET_DEVICENAME:
    {
      string = F(CPLUGIN_NAME_006);
      break;
    }

    case CPlugin::Function::CPLUGIN_INIT:
    {
      success = init_mqtt_delay_queue(event->ControllerIndex, CPlugin_006_pubname, CPlugin_006_mqtt_retainFlag);
      break;
    }

    case CPlugin::Function::CPLUGIN_EXIT:
    {
      exit_mqtt_delay_queue();
      break;
    }

    case CPlugin::Function::CPLUGIN_PROTOCOL_TEMPLATE:
    {
      event->String1 = F("v1/devices/me/rpc/request/+");
      event->String2 = F("v1/devices/me/telemetry");
      event->String3 = F("v1/devices/me/rpc/response/");
      break;
    }

    case CPlugin::Function::CPLUGIN_PROTOCOL_RECV:
    {
      controllerIndex_t ControllerID = findFirstEnabledControllerWithId(CPLUGIN_ID_006);

      if (validControllerIndex(ControllerID)) {
        C006_parse_command(event);
      }
      break;
    }

    case CPlugin::Function::CPLUGIN_PROTOCOL_SEND:
    {
      if (MQTT_queueFull(event->ControllerIndex)) {
        break;
      }

      String json = serializeThingsboardJson(event);
      # ifndef BUILD_NO_DEBUG

      if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
        addLogMove(LOG_LEVEL_DEBUG, concat(F("MQTT : "), json));
      }
      # endif // ifndef BUILD_NO_DEBUG
      String pubname = CPlugin_006_pubname;
      // Publish using move operator, thus pubname and json are empty after this call
      success = MQTTpublish(event->ControllerIndex, event->TaskIndex, std::move(pubname), std::move(json), CPlugin_006_mqtt_retainFlag);
      break;
    }

    case CPlugin::Function::CPLUGIN_FLUSH:
    {
      processMQTTdelayQueue();
      delay(0);
      break;
    }

    default:
      break;
  }
  return success;
}

bool C006_parse_command(struct EventStruct *event) {
  // Topic  : event->String1
  // Message: event->String2
  bool validTopic = MQTT_handle_topic_commands(event); // default handling of /cmd and /set topics

  const int lastindex        = event->String1.lastIndexOf('/');
  const String requestId     = event->String1.substring(lastindex + 1);
  String json = event->String2;
  // need to parse json
  //  {'method':'cmd','params':'GPIO 2 1'}

  uint16_t jsonlength = 512;

  DynamicJsonDocument root(jsonlength);

  deserializeJson(root, json);

  if (root.isNull()) {
    json = "";
    String pubname = "v1/devices/me/rpc/response/" + requestId;
  
    MQTTpublish(event->ControllerIndex, event->TaskIndex, std::move(pubname), std::move(json), CPlugin_006_mqtt_retainFlag);
    return false;
  }

  // Use long here as intermediate object type to prevent ArduinoJSON from adding a new template variant to the code.
  String method = root[F("method")];
  String cmd = root[F("params")];

  MQTT_execute_command(cmd);

  String json2 = "";
  String pubname = "v1/devices/me/rpc/response/" + requestId;

  MQTTpublish(event->ControllerIndex, event->TaskIndex, std::move(pubname), std::move(json2), CPlugin_006_mqtt_retainFlag);
  return validTopic;
}

#endif // ifdef USES_C006
