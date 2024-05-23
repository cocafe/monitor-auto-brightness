#include <stdlib.h>
#include <errno.h>

#include "cJSON.h"
#include "file.h"
#include "logging.h"

void json_traverse_do_print(cJSON *node)
{
        switch (node->type) {
        case cJSON_NULL:
                pr_color(FG_LT_RED, "[null]   ");
                break;
        case cJSON_Number:
                pr_color(FG_LT_MAGENTA, "[number] ");
                break;
        case cJSON_String:
                pr_color(FG_LT_GREEN, "[string] ");
                break;
        case cJSON_Array:
                pr_color(FG_LT_CYAN, "[array]  ");
                break;
        case cJSON_Object:
                pr_color(FG_LT_BLUE, "[object] ");
                break;
        case cJSON_Raw:
                pr_color(FG_LT_RED, "[raws]   ");
                break;
        case cJSON_True:
        case cJSON_False:
                pr_color(FG_YELLOW, "[bool]   ");
                break;
        }

        if (node->string)
                pr_color(FG_LT_YELLOW, "\"%s\" ", node->string);

        switch (node->type) {
        case cJSON_False:
                pr_color(FG_RED, ": false");
                break;
        case cJSON_True:
                pr_color(FG_GREEN, ": true");
                break;
        case cJSON_Number:
                pr_color(FG_LT_CYAN, ": %.f", cJSON_GetNumberValue(node));
                break;
        case cJSON_String:
                pr_color(FG_LT_CYAN, ": \"%s\"", cJSON_GetStringValue(node));
                break;
        }

        pr_color(FG_LT_WHITE, "\n");
}

void json_traverse_print(cJSON *node, uint32_t depth)
{
        static char padding[32] = { [0 ... 31] = '\t' };
        cJSON *child = NULL;

        if (!node)
                return;

        pr_color(FG_LT_WHITE, "%.*s", depth, padding);
        json_traverse_do_print(node);

        // child = root->child
        cJSON_ArrayForEach(child, node) {
                json_traverse_print(child, depth + 1);
        }
}

int json_print(const char *json_path)
{
        cJSON *root_node;
        char *text;
        int err = 0;

        if (!json_path)
                return -EINVAL;

        if (json_path[0] == '\0') {
                pr_err("@path is empty\n");
                return -ENODATA;
        }

        text = file_read(json_path);
        if (!text) {
                pr_err("failed to read file: %s\n", json_path);
                return -EIO;
        }

        root_node = cJSON_Parse(text);
        if (!root_node) {
                pr_err("cJSON failed to parse text\n");
                err = -EINVAL;

                goto free_text;
        }

        json_traverse_print(root_node, 0);

        cJSON_Delete(root_node);

free_text:
        free(text);

        return err;
}
