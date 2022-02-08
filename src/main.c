
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <memory.h>

#include "./libtuntap/tuntap.h"
#include "./tiny-json/tiny-json.c"
#include "./jsonwrite.c"
#include "./b64.c/b64.h"

#define dev struct device *
#define constr char const *

#define MSGTYPE_LOG 0
#define MSGTYPE_DATA 1

__fortify_function int
writeout (const char *__restrict __fmt, ...) {
  int result = __printf_chk (__USE_FORTIFY_LEVEL - 1, __fmt, __va_arg_pack ());

  fflush(stdout);
  return result;
}

struct JsonMsg {
  bool isAck;
  int id;
  char * cmd;
};
#define JsonMsgP struct JsonMsg *

// int JsonMsgToSerialize (char * dest, JsonMsgP src) {
//   char* p = dest;                       // p always points to the null character
//   p = json_objOpen( p, NULL );          // --> {\0
//   p = json_int( p, "isAck", src->isAck ); // --> {"temp":22,\0
//   p = json_int( p, "id", src->id );   // --> {"temp":22,"hum":45,\0
//   p = json_str( p, "cmd", src->cmd );

//   //TODO - iterate based on cmd value

//   p = json_objClose( p );               // --> {"temp":22,"hum":45},\0
//   p = json_end( p );                    // --> {"temp":22,"hum":45}\0
//   return p - dest;       
// }

void _tuntap_log (int a, const char * b) {
  // writeout("%s", (char *)b);
}

//TODO - won't handle escaped { or } correctly!
int scan_json_string (char * buffer, int bufferSize) {
  int openBracketCount = 0;
  int closeBracketCount = 0;

  int ch = 0;
  int readSize = 0;

  for (int i=0; i<bufferSize; i++) {
    ch = getchar();
    if (i==0 && ch != '{') return 0;
    buffer[i] = ch;
    readSize ++;
    if (ch == '{') {
      openBracketCount ++;
    } else if (ch == '}') {
      closeBracketCount ++;
    }

    if (openBracketCount == closeBracketCount) return readSize;
  }
  return 0;
}

int jsonBufferAllocSize = 2048;
char * jsonBuffer;
char * jsonBufferTrimmed;

#define jsonPoolMaxFields 128
json_t jsonPool[ jsonPoolMaxFields ];
const json_t * json;

#define devPoolMaxSize 10
dev devPool[ devPoolMaxSize ];
int nextUnusedDeviceIndex = 0;
bool devPoolStarted [ devPoolMaxSize ];

#define devReadAllocSize 4096
unsigned char * devReadData;

bool doLoop;


int getDevicePoolIndex (dev d) {
  for (int i=0; i<nextUnusedDeviceIndex; i++) {
    if (devPool[i] == d) return i;
  }
  return -1;
}

bool canCreateDevice () {
  return nextUnusedDeviceIndex < devPoolMaxSize -1;
}

dev createDevice () {
  if (canCreateDevice()) {
    dev tunnel = tuntap_init();
    devPool[nextUnusedDeviceIndex] = tunnel;
    nextUnusedDeviceIndex ++;
    return tunnel;
  }
  return NULL;
}

void destroyDevice (dev d) {
  int idx = getDevicePoolIndex(d);
  if (idx > -1) {
    devPool[idx] = NULL;
    devPoolStarted[idx] = false;
  }
  tuntap_destroy(d);
}

dev getDeviceByFD (int deviceFileDescriptor) {
  dev d = NULL;
  for (int i=0; i<nextUnusedDeviceIndex; i++) {
    d = devPool[i];
    if (tuntap_get_fd(d) == deviceFileDescriptor) {
      break;
    }
  }
  return d;
}

bool isDeviceStarted (dev d) {
  int idx = getDevicePoolIndex(d);
  return (
    idx > 1 && devPoolStarted[idx] == true
  );
}

void setDeviceUpState (dev d, bool upState) {
  if (upState) {
    tuntap_up(d);
  } else {
    tuntap_down(d);
  }
  int idx = getDevicePoolIndex(d);
  
  if (idx > -1) devPoolStarted[idx] = upState;
}

void init () {
  tuntap_log_set_cb(&_tuntap_log);

  jsonBuffer = (char *) malloc(jsonBufferAllocSize * sizeof(char));
  jsonBufferTrimmed = (char *) malloc(jsonBufferAllocSize * sizeof(char));

  devReadData = (unsigned char *) malloc(devReadAllocSize * sizeof(unsigned char));

  for (int i=0; i<devPoolMaxSize; i++) {
    devPoolStarted[i] = false;
  }

  doLoop = true;
}
void cleanup () {
  for (int i=0; i<nextUnusedDeviceIndex; i++) {
    dev d = devPool[i];
    devPoolStarted[i] = false;
    tuntap_down(d);
    tuntap_destroy(d);
  }
  free(jsonBuffer);
  free(jsonBufferTrimmed);
  free(devReadData);
}

constr json_getPropString (const json_t * json, char * propname) {
  const json_t * prop = json_getProperty( json, propname );
  if (prop == NULL || json_getType(prop) != JSON_TEXT) return NULL;
  return json_getValue( prop );
}

/**
 * returns 0xDEADBEEF if prop didn't exist
 */
int64_t json_getPropInt (const json_t * json, char * propname) {
  json_t const * prop = json_getProperty( json, propname );
  if (prop == NULL) return 0xDEADBEEF;
  if(json_getType(prop) != JSON_INTEGER) return 0xDEADBEEF;

  return json_getInteger(prop);
}

bool json_propIs (json_t * json, char * propname, char * value) {
  char const * vProp = json_getPropString(json, propname);
  if (vProp == NULL) return false;
  return strcmp(vProp, value) == 0;
}

bool streq (const char * a, const char * b) {
  return strcmp(a, b) == 0;
}

void handleJsonInput () {
  //see if we even need to do anything
  int n;
  if (ioctl(0, FIONREAD, &n) != 0 || n < 1) return;
  int jsonBufferReadSize = scan_json_string(jsonBuffer, jsonBufferAllocSize);

  //do nothing if there is no data
  if (jsonBufferReadSize == 0) {
    // printf("no valid input, skipping\n");
    return;
  }

  //copy only relevant data by erasing previous and populating with subset of array
  memset(jsonBufferTrimmed, 0x00, jsonBufferAllocSize);
  memcpy(jsonBufferTrimmed, jsonBuffer, jsonBufferReadSize);
  
  // printf("You entered %.*s\n", jsonBufferReadSize, jsonBufferTrimmed);

  //parse json
  json = json_create(jsonBufferTrimmed, jsonPool, jsonPoolMaxFields);
  if (json == NULL) return;

  constr vJsonCmd = json_getPropString(json, "cmd");
  if (vJsonCmd == NULL) return;

  if (streq(vJsonCmd,"die")) {
    doLoop = false;
    return;
  }
  json_t const* pSub = json_getProperty(json, vJsonCmd);
  if (pSub == NULL) return;
  if (json_getType(pSub) != JSON_OBJ) return;

  //may be 0xDEADBEEF (aka null, but null is 0)!
  // int64_t id = json_getPropInt(json, "id");

  if (streq(vJsonCmd, "dev")) {
    constr vDevCmd = json_getPropString(pSub, "cmd");
    if (vDevCmd == NULL) return;
    //may be 0xDEADBEEF (aka null, but null is 0)!
    int64_t devId = json_getPropInt(pSub, "id");
    dev tunnel = NULL;
    if (devId != 0xDEADBEEF) tunnel = getDeviceByFD(devId);

    if (tunnel == NULL) {
      if (streq(vDevCmd, "create")) {
        writeout("attempting to create device");

        tunnel = createDevice();
        if (tunnel == NULL) {
          //respond with pool too big exception
          writeout("did not create device");
          return;
        }
        
        tuntap_start(tunnel, TUNTAP_MODE_TUNNEL, TUNTAP_ID_ANY);

        // char * ifname = tuntap_get_ifname(tunnel);
        
        //TODO - grab ipaddr from json if it is present
        char * ipaddr = "10.0.0.1";
        tuntap_set_ip(tunnel, ipaddr, 24);
        
        int fd = tuntap_get_fd(tunnel);
        writeout("tunnel created with fd %i and ip %s\n", fd, ipaddr);

        //TODO - respond with device info
        
        return;
      }

      //TODO - cannot perform action without device fd
      return; 
    }

    if (streq(vDevCmd, "up")) {
      // tuntap_up(tunnel);
      setDeviceUpState(tunnel, true);

      //TODO - respond with success
      writeout("tunnel up\n");
      return;
    } else if (streq(vDevCmd, "down")) {
      setDeviceUpState(tunnel, false);
      // tuntap_down(tunnel);
      writeout("tunnel down\n");
      //TODO - respond with success
      return;
    } else if (streq(vDevCmd, "destroy")) {
      tuntap_destroy(tunnel);
      writeout("tunnel destroy\n");
      //TODO - respond with success
      return;
    } else if (streq(vDevCmd, "sub")) {
      //TODO - by default we're always sub'd
      return;
    } else if (streq(vDevCmd, "unsub")) {
      //TODO
      return;
    } else {
      return;
    }
  } else if (streq(vJsonCmd, "data")) {

  } else if (streq(vJsonCmd, "die")) {
    doLoop = false;
    //TODO - respond with ack
    printf("u die");
    fflush(stdout);
    return;
  }
  

  printf("[json] type '%s' from %s\n", vJsonCmd, jsonBuffer);
  
  fflush(stdout); //necessary to let the output flow to process parent
}

void ipPrintFromBuffer (unsigned char * data, int dataOffset) {
  printf(
    "%u.%u.%u.%u",
    data[dataOffset],
    data[dataOffset+1],
    data[dataOffset+2],
    data[dataOffset+3]
  );
}

void handleDeviceRead (dev d) {
  int readable = tuntap_get_readable(d);
  if (readable < 0) return;

  tuntap_read(d, devReadData, readable);

  printf("\norigin : ");
  ipPrintFromBuffer(devReadData, 12);

  printf(" -> destination : ");
  ipPrintFromBuffer(devReadData, 16);

  printf("\n--------\n");
  fflush(stdout);
}

void handleDevicesRead () {
  dev d;
  for (int i=0; i<nextUnusedDeviceIndex; i++) {
    if (devPoolStarted[i] == true) {
      d = devPool[i];
      handleDeviceRead(d);
    }
  }
}

int main(int argc, char **argv) {
  writeout("testing jw\n");
  jw_test();
  return 0;

  //allocation and setup
  init();

  while(doLoop) {
    //packets read from tunnel devices (usually there is one device)
    handleDevicesRead();

    //this function can set doLoop to false, breaking the loop
    handleJsonInput();

  }

  //clear out any devices and free memory
  cleanup();
}
