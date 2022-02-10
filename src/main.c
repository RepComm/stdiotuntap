
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "./b64.c/b64.h"
#include "./b64.c/buffer.c"
#include "./b64.c/decode.c"
#include "./b64.c/encode.c"
#include "./jsonwrite.c"
#include "./libtuntap/tuntap.h"
#include "./tiny-json/tiny-json.c"

#define dev struct device *
#define constr char const *

#define MSGTYPE_LOG 0
#define MSGTYPE_DATA 1

__fortify_function int writeout(const char *__restrict __fmt, ...) {
  int result = __printf_chk(__USE_FORTIFY_LEVEL - 1, __fmt, __va_arg_pack());

  fflush(stdout);
  return result;
}

// TODO - won't handle escaped { or } correctly!
int scan_json_string(char *buffer, int bufferSize) {
  int openBracketCount = 0;
  int closeBracketCount = 0;

  bool isEscaped = false;

  int ch = 0;
  int readSize = 0;

  for (int i = 0; i < bufferSize; i++) {
    ch = getchar();
    if (i == 0 && ch != '{') return 0;
    buffer[i] = ch;
    readSize++;

    if (isEscaped) {
    } else {
      if (ch == '{') {
        openBracketCount++;
      } else if (ch == '}') {
        closeBracketCount++;
      }
    }

    if (openBracketCount == closeBracketCount) return readSize;
  }
  return 0;
}

int jsonBufferAllocSize = 2048;
char *jsonBuffer;
char *jsonBufferTrimmed;

#define jsonPoolMaxFields 128
json_t jsonPool[jsonPoolMaxFields];
const json_t *json;

jw_info_p info;

#define devPoolMaxSize 10
dev devPool[devPoolMaxSize];
int nextUnusedDeviceIndex = 0;
bool devPoolStarted[devPoolMaxSize];

#define devReadAllocSize 4096
unsigned char *devReadData;

bool doLoop;

void _tuntap_log(int level, const char *msg) {
  jw_info_start(info);
  jw_begin(info);
  jw_key(info, "isAck");
  jw_value_boolean(info, false);
  jw_key(info, "cmd");
  jw_value_str(info, "log");
  jw_key(info, "log");
  jw_begin(info);
  jw_key(info, "data");
  jw_value_str(info, (char *)msg);
  jw_end(info);
  jw_end(info);
  jw_info_stop(info);
  writeout(info->outputJsonString);
}

int getDevicePoolIndex(dev d) {
  for (int i = 0; i < nextUnusedDeviceIndex; i++) {
    if (devPool[i] == d) return i;
  }
  return -1;
}

bool canCreateDevice() { return nextUnusedDeviceIndex < devPoolMaxSize - 1; }

dev createDevice() {
  if (canCreateDevice()) {
    dev tunnel = tuntap_init();
    tuntap_log_set_cb(_tuntap_log);

    devPool[nextUnusedDeviceIndex] = tunnel;
    nextUnusedDeviceIndex++;
    return tunnel;
  }
  return NULL;
}

void destroyDevice(dev d) {
  int idx = getDevicePoolIndex(d);
  if (idx > -1) {
    devPool[idx] = NULL;
    devPoolStarted[idx] = false;
  }
  tuntap_destroy(d);
}

dev getDeviceByFD(int deviceFileDescriptor) {
  dev d = NULL;
  for (int i = 0; i < nextUnusedDeviceIndex; i++) {
    d = devPool[i];
    if (tuntap_get_fd(d) == deviceFileDescriptor) {
      break;
    }
  }
  return d;
}

bool isDeviceStarted(dev d) {
  int idx = getDevicePoolIndex(d);
  return (idx > 1 && devPoolStarted[idx] == true);
}

void setDeviceUpState(dev d, bool upState) {
  if (upState) {
    tuntap_up(d);
  } else {
    tuntap_down(d);
  }
  int idx = getDevicePoolIndex(d);

  if (idx > -1) devPoolStarted[idx] = upState;
}

void init() {
  tuntap_log_set_cb(_tuntap_log);

  info = jw_info_create();

  jsonBuffer = (char *)malloc(jsonBufferAllocSize * sizeof(char));
  jsonBufferTrimmed = (char *)malloc(jsonBufferAllocSize * sizeof(char));

  devReadData =
      (unsigned char *)malloc(devReadAllocSize * sizeof(unsigned char));

  for (int i = 0; i < devPoolMaxSize; i++) {
    devPoolStarted[i] = false;
  }

  doLoop = true;
}
void cleanup() {
  for (int i = 0; i < nextUnusedDeviceIndex; i++) {
    dev d = devPool[i];
    devPoolStarted[i] = false;
    tuntap_down(d);
    tuntap_destroy(d);
  }
  free(jsonBuffer);
  free(jsonBufferTrimmed);
  free(devReadData);
  jw_info_destroy(info);
}

constr json_getPropString(const json_t *json, char *propname) {
  const json_t *prop = json_getProperty(json, propname);
  if (prop == NULL || json_getType(prop) != JSON_TEXT) return NULL;
  return json_getValue(prop);
}

/**
 * returns 0xDEADBEEF if prop didn't exist
 */
int64_t json_getPropInt(const json_t *json, char *propname) {
  json_t const *prop = json_getProperty(json, propname);
  if (prop == NULL) return 0xDEADBEEF;
  if (json_getType(prop) != JSON_INTEGER) return 0xDEADBEEF;

  return json_getInteger(prop);
}

bool json_propIs(json_t *json, char *propname, char *value) {
  char const *vProp = json_getPropString(json, propname);
  if (vProp == NULL) return false;
  return strcmp(vProp, value) == 0;
}

bool streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

void handleJsonInput() {
  // see if we even need to do anything
  int n;
  if (ioctl(0, FIONREAD, &n) != 0 || n < 1) return;
  int jsonBufferReadSize = scan_json_string(jsonBuffer, jsonBufferAllocSize);

  // do nothing if there is no data
  if (jsonBufferReadSize == 0) {
    // printf("no valid input, skipping\n");
    return;
  }

  // copy only relevant data by erasing previous and populating with subset of
  // array
  memset(jsonBufferTrimmed, 0x00, jsonBufferAllocSize);
  memcpy(jsonBufferTrimmed, jsonBuffer, jsonBufferReadSize);

  // printf("You entered %.*s\n", jsonBufferReadSize, jsonBufferTrimmed);

  // parse json
  json = json_create(jsonBufferTrimmed, jsonPool, jsonPoolMaxFields);
  if (json == NULL) return;

  constr vJsonCmd = json_getPropString(json, "cmd");
  if (vJsonCmd == NULL) return;

  if (streq(vJsonCmd, "die")) {
    doLoop = false;
    jw_info_start(info);

    jw_begin(info);
    jw_key(info, "isAck");
    jw_value_boolean(info, true);
    jw_key(info, "cmd");
    jw_value_str(info, (char *)vJsonCmd);
    jw_key(info, (char *)vJsonCmd);

    jw_end(info);

    jw_info_stop(info);

    writeout(info->outputJsonString);
    return;
  }
  json_t const *pSub = json_getProperty(json, vJsonCmd);
  if (pSub == NULL) return;
  if (json_getType(pSub) != JSON_OBJ) return;

  // may be 0xDEADBEEF (aka null, but null is 0)!
  // int64_t id = json_getPropInt(json, "id");

  if (streq(vJsonCmd, "dev")) {
    constr vDevCmd = json_getPropString(pSub, "cmd");
    if (vDevCmd == NULL) return;
    // may be 0xDEADBEEF (aka null, but null is 0)!
    int64_t devId = json_getPropInt(pSub, "id");
    dev tunnel = NULL;
    if (devId != 0xDEADBEEF) tunnel = getDeviceByFD(devId);

    if (tunnel == NULL) {
      if (streq(vDevCmd, "create")) {
        // writeout("attempting to create device");

        tunnel = createDevice();
        if (tunnel == NULL) {
          // respond with pool too big exception
          writeout("did not create device");
          return;
        }

        tuntap_start(tunnel, TUNTAP_MODE_TUNNEL, TUNTAP_ID_ANY);

        char *ifname = tuntap_get_ifname(tunnel);

        // TODO - grab ipaddr from json if it is present
        char *ipaddr = "10.0.0.1";
        tuntap_set_ip(tunnel, ipaddr, 24);

        int fd = tuntap_get_fd(tunnel);

        jw_info_start(info);

        jw_begin(info);
        jw_key(info, "isAck");
        jw_value_boolean(info, true);
        jw_key(info, "cmd");
        jw_value_str(info, (char *)vJsonCmd);
        jw_key(info, (char *)vJsonCmd);
        jw_begin(info);
        jw_key(info, "cmd");
        jw_value_str(info, (char *)vDevCmd);
        jw_key(info, "id");
        jw_value_int(info, fd);
        jw_key(info, "ifname");
        jw_value_str(info, ifname);
        jw_key(info, "ipv4");
        jw_value_str(info, ipaddr);
        jw_end(info);
        jw_end(info);

        jw_info_stop(info);

        writeout(info->outputJsonString);

        return;
      }

      return;
    }

    if (streq(vDevCmd, "up")) {
      // tuntap_up(tunnel);
      setDeviceUpState(tunnel, true);

      jw_info_start(info);
      jw_begin(info);
      jw_key(info, "isAck");
      jw_value_boolean(info, true);
      jw_key(info, "cmd");
      jw_value_str(info, (char *)vJsonCmd);
      jw_key(info, (char *)vJsonCmd);
      jw_begin(info);
      jw_key(info, "cmd");
      jw_value_str(info, (char *)vDevCmd);
      jw_key(info, "id");
      jw_value_int(info, (int)devId);
      jw_end(info);
      jw_end(info);

      jw_info_stop(info);

      writeout(info->outputJsonString);
      return;
    } else if (streq(vDevCmd, "down")) {
      setDeviceUpState(tunnel, false);

      jw_info_start(info);

      jw_begin(info);
      jw_key(info, "isAck");
      jw_value_boolean(info, true);
      jw_key(info, "cmd");
      jw_value_str(info, (char *)vJsonCmd);
      jw_key(info, (char *)vJsonCmd);
      jw_begin(info);
      jw_key(info, "cmd");
      jw_value_str(info, (char *)vDevCmd);
      jw_key(info, "id");
      jw_value_int(info, (int)devId);
      jw_end(info);
      jw_end(info);

      jw_info_stop(info);

      writeout(info->outputJsonString);
      return;
    } else if (streq(vDevCmd, "destroy")) {
      tuntap_destroy(tunnel);

      jw_info_start(info);

      jw_begin(info);
      jw_key(info, "isAck");
      jw_value_boolean(info, true);
      jw_key(info, "cmd");
      jw_value_str(info, (char *)vJsonCmd);
      jw_key(info, (char *)vJsonCmd);
      jw_begin(info);
      jw_key(info, "cmd");
      jw_value_str(info, (char *)vDevCmd);
      jw_key(info, "id");
      jw_value_int(info, (int)devId);
      jw_end(info);
      jw_end(info);

      jw_info_stop(info);

      writeout(info->outputJsonString);
      return;
    } else if (streq(vDevCmd, "sub")) {
      // TODO - by default we're always sub'd
      return;
    } else if (streq(vDevCmd, "unsub")) {
      // TODO
      return;
    } else {
      return;
    }
  } else if (streq(vJsonCmd, "data")) {
    int64_t devId = json_getPropInt(pSub, "id");
    if (devId == 0xDEADBEEF) {
      jw_info_start(info);
      jw_begin(info);
        jw_key(info, "isAck");
        jw_value_boolean(info, true);
        jw_key(info, "cmd");
        jw_value_str(info, "log");

        jw_key(info, "log");
        jw_begin(info);

        jw_key(info, "error");
        jw_value_str(info, "no device id (fd) provided, cannot write ip packet to undeclared tunnel");
      
      jw_end(info);
      jw_info_stop(info);

      writeout(info->outputJsonString);
      return;
    }
    dev tunnel = tunnel = getDeviceByFD(devId);

    if (tunnel == NULL) {
      jw_info_start(info);
      jw_begin(info);
        jw_key(info, "isAck");
        jw_value_boolean(info, true);
        jw_key(info, "cmd");
        jw_value_str(info, "log");

        jw_key(info, "log");
        jw_begin(info);

        jw_key(info, "error");
        jw_value_str(info, "no tunnel device found for id, cannot send ip packet");
      
      jw_end(info);
      jw_info_stop(info);

      writeout(info->outputJsonString);
      return;
    }

    const char * base64 = json_getPropString(pSub, "base64");
    if (base64 == NULL) {
      jw_info_start(info);
      jw_begin(info);
        jw_key(info, "isAck");
        jw_value_boolean(info, true);
        jw_key(info, "cmd");
        jw_value_str(info, "log");

        jw_key(info, "log");
        jw_begin(info);

        jw_key(info, "error");
        jw_value_str(info, "no base64 provided, cannot write ip packet to tunnel");
      
      jw_end(info);
      jw_info_stop(info);

      writeout(info->outputJsonString);
    }
    int base64len = strlen(base64);

    size_t readDataSize = 0;
    unsigned char * readData = b64_decode_ex(base64, base64len, &readDataSize);

    tuntap_write(tunnel, readData, readDataSize);

    free(readData);

    jw_info_start(info);
    jw_begin(info);
      jw_key(info, "isAck");
      jw_value_boolean(info, true);
      jw_key(info, "cmd");
      jw_value_str(info, (char *) vJsonCmd);

      jw_key(info, (char*) vJsonCmd);
      jw_begin(info);

    jw_end(info);
    jw_info_stop(info);

    writeout(info->outputJsonString);
  }
}

#define IP_FRAME_IPV4_SRC 12
#define IP_FRAME_IPV4_DEST 16
#define IP_V4_CSTR_MAX_LEN 16 //including \0

int ip_frame_get_ipv4 (unsigned char * data, int IP_WHICH, char * out) {
  int dataOffset = IP_WHICH;

  int size = snprintf(
    out,
    IP_V4_CSTR_MAX_LEN,
    "%u.%u.%u.%u",
    data[dataOffset],
    data[dataOffset + 1],
    data[dataOffset + 2],
    data[dataOffset + 3]
  );

  return size;
}
int ip_frame_get_ipv4_strlen (unsigned char * data, int IP_WHICH) {
  int dataOffset = IP_WHICH;

  int size = snprintf(
    NULL, 0, 
    "%u.%u.%u.%u",
    data[dataOffset],
    data[dataOffset + 1],
    data[dataOffset + 2],
    data[dataOffset + 3]
  );
  return size;
}

int ip_frame_get_ipv4_strmaxlen () {
  return strlen("255.255.255.255");
}

void handleDeviceRead(dev d) {
  int readable = tuntap_get_readable(d);
  if (readable < 0) return;

  tuntap_read(d, devReadData, readable);

  char *base64 = b64_encode(devReadData, (size_t)readable);

  char srcIpv4[IP_V4_CSTR_MAX_LEN];
  ip_frame_get_ipv4(devReadData, IP_FRAME_IPV4_SRC, srcIpv4);
  char destIpv4[IP_V4_CSTR_MAX_LEN];
  ip_frame_get_ipv4(devReadData, IP_FRAME_IPV4_DEST, destIpv4);

  jw_info_start(info);
  jw_begin(info);
  jw_key(info, "isAck");
  jw_value_boolean(info, false);
  jw_key(info, "cmd");
  jw_value_str(info, "data");

  jw_key(info, "data");
  jw_begin(info);
  jw_key(info, "srcIpv4");
  jw_value_str(info, srcIpv4);
  jw_key(info, "destIpv4");
  jw_value_str(info, destIpv4);
  jw_key(info, "base64");
  jw_value_str(info, base64);
  jw_end(info);
  jw_end(info);
  jw_info_stop(info);

  if (jw_info_error(info)) {
    // TODO - handle this sanely
  }

  writeout(info->outputJsonString);

  free(base64);
  // printf("\norigin : ");
  // ipPrintFromBuffer(devReadData, 12);

  // printf(" -> destination : ");
  // ipPrintFromBuffer(devReadData, 16);

  // printf("\n--------\n");
  // fflush(stdout);
}

void handleDevicesRead() {
  dev d;
  for (int i = 0; i < nextUnusedDeviceIndex; i++) {
    if (devPoolStarted[i] == true) {
      d = devPool[i];
      handleDeviceRead(d);
    }
  }
}

int main(int argc, char **argv) {
  // allocation and setup
  init();

  while (doLoop) {
    // packets read from tunnel devices (usually there is one device)
    handleDevicesRead();

    // this function can set doLoop to false, breaking the loop
    handleJsonInput();
  }

  // clear out any devices and free memory
  cleanup();
}
