
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "tuntap.h"
#include "tiny-json.c"

#define dev struct device *

#define MSGTYPE_LOG 0
#define MSGTYPE_DATA 1

__fortify_function int
writeout (const char *__restrict __fmt, ...) {
  int result = __printf_chk (__USE_FORTIFY_LEVEL - 1, __fmt, __va_arg_pack ());

  fflush(stdout);
  return result;
}

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

void init () {
  tuntap_log_set_cb(&_tuntap_log);

  jsonBuffer = (char *) malloc(jsonBufferAllocSize * sizeof(char));
  jsonBufferTrimmed = (char *) malloc(jsonBufferAllocSize * sizeof(char));
}

void handleJsonInput () {
  //see if we even need to do anything
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
  
  if (json == NULL) {
    printf("couldn't parse json");
    return;
  }

  json_t const* msgtype = json_getProperty( json, "type" );
  if (msgtype == NULL) {
    printf("json has no 'type' field, ignoring");
    return;
  }

  if ( json_getType( msgtype ) != JSON_TEXT ) {
    printf("expected 'type' to be string, ignoring");
    return;
  }

  char const* msgtypeValue = json_getValue( msgtype );

  printf("[json] type '%s' from %s\n", msgtypeValue, jsonBuffer);
  
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

int main(int argc, char **argv) {
  init();

  writeout("%s\n", "init device");
  dev tunnel = tuntap_init();

  writeout("%s\n", "start device");
  tuntap_start(tunnel, TUNTAP_MODE_TUNNEL, TUNTAP_ID_ANY);

  char * ifname = tuntap_get_ifname(tunnel);
  writeout("ifname: %s\n", ifname);

  char * ipaddr = "10.0.0.1";
  tuntap_set_ip(tunnel, ipaddr, 24);
  writeout("ip: %s\n", ipaddr);

  // tuntap_set_hwaddr(tunnel, "198403152209");

  tuntap_up(tunnel);
  writeout("%s\n", "device up");

  t_tun fd = tuntap_get_fd(tunnel);
  writeout("device fd %i\n", fd);


  bool loop = true;
  int readable = 0;

  // char * data = (char *) malloc(2048 * sizeof(char));
  unsigned char * data = (unsigned char *) malloc(2048 * sizeof(unsigned char));

  while(loop) {

    readable = tuntap_get_readable(tunnel);
    if (readable > 0) {
      tuntap_read(tunnel, data, readable);

      printf("-------- %i bytes from tunnel\n", readable);

      // for (int i = 0; i < readable; i++) {
      //   printf(" %02hhx", data[i]);
      // }

      printf("\norigin : ");
      ipPrintFromBuffer(data, 12);

      printf(" -> destination : ");
      ipPrintFromBuffer(data, 16);

      printf("\n--------\n");
      fflush(stdout);
    }

    handleJsonInput();
  }

  tuntap_down(tunnel);
  
  tuntap_destroy(tunnel);
}
