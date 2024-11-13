#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cJSON.h"

int json_object_begin(char* json);
int json_object_add_string(char* json, char* key,  char* value);
int json_object_add_object(char* json, char* key,  char* value);
int json_object_add_int(char* json, char* key,  int value);
int json_object_add_double(char* json, char* key, double value);
int json_object_add_bool(char* json, char* key,  bool value);
int json_object_add_null(char* json, char* key);
int json_object_end(char* json);
void json_check_data_length(const char* json, int buffer_size);

int hbf_safe_GetObjectInt(const cJSON * const object, const char * const string, const int default_value);
char* hbf_safe_GetObjectString(const cJSON * const object, const char * const string, char* default_value);

char* get_mac_address_str();