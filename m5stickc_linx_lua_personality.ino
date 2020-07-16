#include <M5StickC.h>
#include <LinxESP32.h>

#include "esp_ota_ops.h"
#include "esp_lua.h"

const char PERSONALITY_NAME[16] = "lua";
const char PERSONALITY_VERSION[8] = "v1.0.0";

lua_State *L;

LinxESP32 *LinxDevice;

bool restartReq;

const esp_partition_t *nup;
esp_ota_handle_t otaHandle;

int (*subCmds[16])(unsigned char, unsigned char*, unsigned char*, unsigned char*);

void setup()
{
  L = luaL_newstate();
  luaL_openlibs(L);
  lua_register(L, "print", L_print);

  LinxDevice = new LinxESP32();

  Wire.begin(0, 26); // HAT
  //Wire1.begin(32, 33); // Grove

  LinxWifiConnection.AttachCustomCommand(0, getCurrentPersonalityInfoCommand);
  LinxWifiConnection.AttachCustomCommand(1, restartReqCommand);
  LinxWifiConnection.AttachCustomCommand(2, getBaseMACAddressCommand);
  LinxWifiConnection.AttachCustomCommand(3, otaCommand);
  LinxWifiConnection.AttachCustomCommand(4, luaCommand);
  LinxWifiConnection.Start(LinxDevice, 44300);

  //LinxSerialConnection.AttachCustomCommand(0, getCurrentPersonalityInfoCommand);
  //LinxSerialConnection.AttachCustomCommand(1, restartReqCommand);
  //LinxSerialConnection.AttachCustomCommand(2, getBaseMACAddressCommand);
  //LinxSerialConnection.AttachCustomCommand(3, otaCommand);
  //LinxSerialConnection.Start(LinxDevice, 3);
}

void loop()
{
  LinxWifiConnection.CheckForCommands();
  //LinxSerialConnection.CheckForCommands();

  M5.update();
  if (restartReq || M5.BtnB.wasPressed()) {
    if (restartReq) {
      // Wait until receiving the disconnect command
      while (LinxWifiConnection.State != CLOSE) LinxWifiConnection.CheckForCommands();
      //while (LinxSerialConnection.State != CLOSE) LinxSerialConnection.CheckForCommands();

      // Wait a little bit
      delay(500);
    }
    ESP.restart();
  }

  delay(1);
}

int getCurrentPersonalityInfoCommand(unsigned char numInputBytes, unsigned char* input, unsigned char* numResponseBytes, unsigned char* response)
{
  response[0] = 0; // ret_code = SUCCESS
  memcpy(response + 1, PERSONALITY_NAME, 16); // personality_name
  memcpy(response + 17, PERSONALITY_VERSION, 8); // personality_version
  *numResponseBytes = 1 + 16 + 8;
  return 0;
}

int restartReqCommand(unsigned char numInputBytes, unsigned char* input, unsigned char* numResponseBytes, unsigned char* response)
{
  restartReq = true;
  response[0] = 0; // ret_code = SUCCESS
  *numResponseBytes = 1;
  return 0;
}

int getBaseMACAddressCommand(unsigned char numInputBytes, unsigned char* input, unsigned char* numResponseBytes, unsigned char* response)
{
  response[0] = 0; // ret_code = SUCCESS
  esp_efuse_mac_get_default(response + 1);
  *numResponseBytes = 1 + 6;
  return 0;
}

int otaCommand(unsigned char numInputBytes, unsigned char* input, unsigned char* numResponseBytes, unsigned char* response)
{
  if (numInputBytes < 1) {
    response[3] = 0x1; // ret_code = not enough arguments
    return 0;
  }

  esp_err_t ret; // typedef int32_t esp_err_t

  uint8_t subcmd = input[0]; // subcommand

  // API Reference
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html

  if (subcmd == 0x00) { // OTA begin subcommand
    if (numInputBytes < 5) {
      response[3] = 0x1;
      return 0;
    }
    uint32_t imageSize = input[4] | (input[3] << 8) | (input[2] << 16) | (input[1] << 24);
    nup = esp_ota_get_next_update_partition(NULL);
    ret = esp_ota_begin(nup, imageSize, &otaHandle);
  } else if (subcmd == 0x01) { // OTA write subcommand
    // Up to 240 bytes per chunk
    ret = esp_ota_write(otaHandle, input + 1, numInputBytes - 1);
  } else if (subcmd == 0x02) { // OTA end subcommand
    ret = esp_ota_end(otaHandle);
  } else if (subcmd == 0x03) { // OTA set boot partition subcommand
    if (nup == NULL) nup = esp_ota_get_next_update_partition(NULL);
    ret = esp_ota_set_boot_partition(nup);
  } else {
    ret = 0x02; // ret_code = subcommand not found
  }

  esp_error_t_to_ret_code(ret, response); // ret_code
  *numResponseBytes = 4;
  return 0;
}

int luaCommand(unsigned char numInputBytes, unsigned char* input, unsigned char* numResponseBytes, unsigned char* response)
{
  char str[numInputBytes];
  memcpy(str, input, numInputBytes);
  response[0] = luaL_dostring(L, str);
  *numResponseBytes = 1;
  return 0;
}

int subCommand(unsigned char numInputBytes, unsigned char* input, unsigned char* numResponseBytes, unsigned char* response)
{
  // ret_code_cmd : uint8_t
  // ret_code_subcmd ...

  if (numInputBytes < 1) {
    response[0] = 0x1; // ret_code = not enough arguments
    return 0;
  }

  if (subCmds[input[0]] == NULL)
  {
    return 1; // command not supported
  }

  // Call a subcommand
  int ret = subCmds[input[0]](numInputBytes - 1, input + 1, numResponseBytes, response + 1);

  (*numResponseBytes)++;

  return ret;
}

void attachSubCommand(unsigned short subCmdNum, int (*func)(unsigned char, unsigned char*, unsigned char*, unsigned char*))
{
  subCmds[subCmdNum] = func;
}

void esp_error_t_to_ret_code(esp_err_t ret, unsigned char* buf)
{
  // int32_t to uint8_t array
  // Note: LabVIEW stores numbers in big-endian
  buf[0] = (ret >> 24) & 0xFF000000;
  buf[1] = (ret >> 16) & 0x00FF0000;
  buf[2] = (ret >>  8) & 0x0000FF00;
  buf[3] = (ret      ) & 0x000000FF;
}
