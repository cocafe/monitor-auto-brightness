#ifndef __JJ_JSON_H__
#define __JJ_JSON_H__

#include "cJSON.h"

int json_print(const char *json_path);
void json_traverse_print(cJSON *node, uint32_t depth);

#endif // __JJ_JSON_H__