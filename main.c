#include <stdio.h>

#include "json.h"

int main(void) {
    const char* file_path = "./jsons/hello.json";
    struct JsonSchemaObject schema = {0};
    json_schema_object_init(&schema);
    json_schema_object_append_field(&schema, "message", 7, JSON_TYPE_STRING);
    json_schema_object_append_field(&schema, "length", 6, JSON_TYPE_STRING);
    int ret = json_schema_object_parser_from_file(&schema, file_path);
    if(ret <= 0) {
        fprintf(stderr, "Failed to parse json, due to error %d\n", ret);
    } else {
        printf("Message: %s\n", schema.fields[0].value);
        printf("Message Length: %s\n", schema.fields[1].value);
    }
    json_schema_object_free(&schema);
    return 0;
}
