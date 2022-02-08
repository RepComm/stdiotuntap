
/**Basic JsonWrite module
 * @author Jonathan Crowder
 * 
 * @see jw_tst() for example usage
 */

#include <string.h>
#include <stdio.h>
#include <memory.h>
#include <stdbool.h>

#define JW_ERROR_NONE 0
#define JW_ERROR_MEM 1

#define str char *
struct jw_info {
  str outputJsonString;
  int outputJsonStringAllocSize;
  int outputJsonLength;
  str debug;
  int errorCode;
};
#define jw_info_p struct jw_info *

#define JW_DEFAULT_OUTPUT_ALLOC_SIZE 4096

int jw_output_length (jw_info_p info) {
  return info->outputJsonLength;
}

void jw_output_copy (jw_info_p info, str out) {
  strcpy(out, info->outputJsonString);
}

/**Reset all internal data
 * Call when you want to re-use a struct jw_info
 */
void jw_info_clear (jw_info_p info) {
  info->errorCode = JW_ERROR_NONE;
  info->debug = NULL;
  info->outputJsonLength = 0;
  memset(info->outputJsonString, 0x00, info->outputJsonStringAllocSize);
}

jw_info_p jw_info_create_ex(int outputJsonStringAllocSize) {
  jw_info_p info = (jw_info_p) malloc( sizeof(struct jw_info) );

  info->outputJsonStringAllocSize = outputJsonStringAllocSize;
  info->outputJsonString = (str) malloc( sizeof (char) * info->outputJsonStringAllocSize);

  jw_info_clear(info);
  return info;
}
jw_info_p jw_info_create() {
  return jw_info_create_ex(JW_DEFAULT_OUTPUT_ALLOC_SIZE);
}

/**Destroy struct jw_info * created with jw_info_create[_ex]();
 * 
 * Warning: Destroys all associated resources, including outputJsonString!
 * If you need outputJsonString, allocate a string with info->outputJsonStringLength
 * and copy
 * 
 * Example:
 * 
 * char * jsonString = (char *) malloc(sizeof(char) * info->outputJsonStringLength);
 * strcpy (jsonString, info->outputJsonString);
 * jw_info_destroy(info);
 * 
 */
void jw_info_destroy (jw_info_p info) {
  free(info->outputJsonString);
  free(info);
}

bool jw_can_fit_additional (jw_info_p info, int bytes) {
  return (
    info->outputJsonStringAllocSize > (info->outputJsonLength -1) + bytes
  );
}

bool jw_info_error (jw_info_p info) {
  return (
    info->errorCode != JW_ERROR_NONE
  );
}

bool jw_info_debug (jw_info_p info) {
  return info->debug != NULL;
}

bool jw_char (jw_info_p info, char ch) {
  if (jw_info_error(info)) return false;
  if (!jw_can_fit_additional(info, 1)) {
    info->errorCode = JW_ERROR_MEM;
    info->debug = "outputJsonStringAllocSize limit reached";
    return false;
  }
  info->outputJsonString[info->outputJsonLength] = ch;
  info->outputJsonLength++;

  return true;
}

bool jw_str (jw_info_p info, str s) {
  if (jw_info_error(info)) return false;
  int sLength = strlen(s);
  if (!jw_can_fit_additional(info, sLength)) {
    info->errorCode = JW_ERROR_MEM;
    info->debug = "outputJsonStringAllocSize limit reached";
    return false;
  }
  
  memcpy(info->outputJsonString+info->outputJsonLength, s, sLength);
  info->outputJsonLength += sLength;

  return true;
}

bool jw_begin(jw_info_p info) {
  return jw_char(info, '{');
}

bool jw_last_char_was (jw_info_p info, char ch) {
  return (
    info->outputJsonLength > 0 &&
    info->outputJsonString[info->outputJsonLength-1] == ch
  );
}

/**Internal
 * Decrements outputJsonlength by count
 */
void jw_rewind (jw_info_p info, int count) {
  info->outputJsonLength -= count;
}

/**End an object
 */
bool jw_end (jw_info_p info) {
  if (jw_info_error(info)) return false;
  
  //deal with trailing comma
  if (jw_last_char_was(info, ',')) jw_rewind(info, 1);

  return jw_char(info, '}');
}

bool jw_key (jw_info_p info, str key) {
  if (!jw_char(info, '"')) return false;

  if (!jw_str(info, key)) return false;
  
  if (!jw_char(info, '"')) return false;

  if (!jw_char(info, ':')) return false;
  return true;
}

bool jw_value_str (jw_info_p info, str value) {
  if (!jw_char(info, '"')) return false;

  if (!jw_str(info, value)) return false;

  if (!jw_char(info, '"')) return false;

  if (!jw_char(info, ',')) return false;
  return true;
}

/**
 * https://stackoverflow.com/a/8257728/8112809
 */
int int_string_size (int num) {
  return snprintf( NULL, 0, "%d", num );
}

bool jw_value_int (jw_info_p info, int value) {
  if (jw_info_error(info)) return false;

  int valuelen = int_string_size(value);

  if (!jw_can_fit_additional(info, valuelen)) {
    info->errorCode = JW_ERROR_MEM;
    info->debug = "outputJsonStringAllocSize limit reached";
    return false;
  }

  //https://stackoverflow.com/a/8257728/8112809
  snprintf(
    info->outputJsonString+info->outputJsonLength,
    valuelen + 1,
    "%d",
    value  
  );
  info->outputJsonLength += valuelen;

  if (!jw_char(info, ',')) return false;
  return true;
}

/**
 * https://stackoverflow.com/a/8257728/8112809
 */
int float_string_size (float num) {
  return snprintf( NULL, 0, "%f", num );
}

bool jw_value_float (jw_info_p info, float value) {
  if (jw_info_error(info)) return false;

  int valuelen = float_string_size(value);

  if (!jw_can_fit_additional(info, valuelen)) {
    info->errorCode = JW_ERROR_MEM;
    info->debug = "outputJsonStringAllocSize limit reached";
    return false;
  }

  //https://stackoverflow.com/a/8257728/8112809
  snprintf(
    info->outputJsonString+info->outputJsonLength,
    valuelen + 1,
    "%f",
    value  
  );
  info->outputJsonLength += valuelen;

  if (!jw_char(info, ',')) return false;
  return true;
}

void jw_test () {
  //@see jw_info_create_ex() for creating with different sized allocations
  //for jsonOutputString
  jw_info_p info = jw_info_create();

  jw_begin(info);
    jw_key(info, "cmd");
    jw_value_str(info, "dev");

    jw_key(info, "dev");
    jw_begin(info);
      jw_key(info, "cmd");
      jw_value_str(info, "create");

      jw_key(info, "id");
      jw_value_int(info, 4);
      // jw_value_float(info, 4.1);
    jw_end(info);
  jw_end(info);

  //check for errors
  if (jw_info_error(info)) {
    printf("JsonWrite encountered an error, output is not gaurenteed\n");
    if (jw_info_debug(info)) {
      printf("JsonWrite debug: %s\n", info->debug);
    } else {
      printf("JsonWrite did not provide debug info, this should be reported\n");
    }
  }

  //optionally save output for later, jsonString survives jw_info_destroy
  str jsonString = (char *) malloc(sizeof(char) * jw_output_length(info));
  jw_output_copy(info, jsonString);

  //or just use the output directly
  // printf("JsonWrite: %s\n", info->outputJsonString);
  printf("JsonWrite: %s\n", jsonString);

  //cleanup
  jw_info_destroy(info);
  //you could instead simply jw_info_clear(info);
  //and reuse it.
}
