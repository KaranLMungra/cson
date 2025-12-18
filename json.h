#include<stdlib.h>

enum JsonType {
    JSON_TYPE_STRING,
    JSON_TYPE_OBJECT,
};

struct JsonSchemaString {
    char* value;
    size_t len;
};
    
struct JsonSchemaField {
    char* name;
    size_t name_len;
    char* value;
    size_t value_len;
    enum JsonType type;
};

struct JsonSchemaObject {
    struct JsonSchemaField* fields;
    size_t field_num;
    size_t field_cap;
};

void json_schema_object_init(struct JsonSchemaObject *object);
void json_schema_object_append_field(struct JsonSchemaObject *object, const char* key, size_t key_len, enum JsonType type);
inline int skip_whitespace(const char* content, int start, size_t content_len);
inline int skip_until_char(const char* content, int start, size_t content_len, char c);
int json_schema_field_parser(struct JsonSchemaField *field, const char* content, int start, size_t content_len);
int json_schema_object_parser(struct JsonSchemaObject *object, const char* content, int start, size_t content_len);
int json_schema_object_parser_from_file(struct JsonSchemaObject *object, const char* file_path);

void json_schema_object_free(struct JsonSchemaObject *object);


