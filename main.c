#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const size_t MAX_BUFFER_SIZE = 4096;

char* read_file(const char* path) {
    FILE* fp = fopen(path, "r");
    if(fp == NULL) {
        fprintf(stderr, "Failed to open file, due to error %s\n", strerror(errno));
        return NULL;
    }
    char buffer[4096];
    char* content = calloc(MAX_BUFFER_SIZE, sizeof(char));
    int read_bytes = 0;
    while(1) {
        read_bytes = fread(buffer, sizeof(char), MAX_BUFFER_SIZE, fp); 
        if(feof(fp) != 0) {
            strncpy(content, buffer, read_bytes); 
            clearerr(fp);
            break;
        } else if(ferror(fp) != 0) {
            fprintf(stderr, "Failed to read from file\n");
            clearerr(fp);
            return NULL;
        } else {
            // TODO: Handle read when the content is more than MAX_BUFFER_SIZE
            strncpy(content, buffer, read_bytes); 
        }
    }
    if(fclose(fp) == EOF) {
        fprintf(stderr, "Failed to close file, due to error %s\n", strerror(errno));
    }
    return content;
}

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
void json_schema_object_free(struct JsonSchemaObject *object);

void json_schema_object_init(struct JsonSchemaObject *object) {
    object->field_num = 0;
    object->field_cap = 64;
    // TODO: Handle error here
    object->fields = calloc(object->field_cap, sizeof(struct JsonSchemaField));
}

void json_schema_object_append_field(struct JsonSchemaObject *object, const char* key, size_t key_len, enum JsonType type) {
    if(object->field_num >= object->field_cap) {
        object->field_cap *= 2;
        // TODO: Handle error here
        object->fields = reallocarray(object->fields, object->field_cap, sizeof(struct JsonSchemaField));
    }
    object->fields[object->field_num].name = calloc(key_len + 1, sizeof(char));
    object->fields[object->field_num].name_len = key_len;
    strncpy(object->fields[object->field_num].name, key, key_len);
    object->fields[object->field_num].type = type;
    object->field_num++;
}

int skip_whitespace(const char* content, int start, size_t content_len) {
    int i = start;
    while(i < content_len && isspace(content[i])) {
        i++;
    }
    if (i == content_len) return -1;
    return i;
}

int skip_until_char(const char* content, int start, size_t content_len, char c) {
    int i = start;
    while(i < content_len && content[i] != c) i++;
    if (i == content_len) return -1;
    return i;
}

int json_schema_field_parser(struct JsonSchemaField *field, const char* content, int start, size_t content_len) {
    int i = start;
    int j = start; 
    char prev_char = 0;
    while(i < content_len) {
        j = skip_whitespace(content, i, content_len);
        if(j == -1) {
            return 0;
        }
        if(content[j] == '"' && prev_char == 0) {
            // TODO: Handle escaped strings
            i = j + 1;
            j = skip_until_char(content, i, content_len, '"');
            if(j == -1) {
               return 0; 
            }
            field->name_len = j - i;
            field->name = calloc(field->name_len + 1, sizeof(char));
            strncpy(field->name, content+i, field->name_len);
            i = j + 1;
            prev_char = '"';
        } else if(content[j] == ':' && prev_char == '"') {
            i = j + 1;
            prev_char = ':';
        } else if(content[j] == '"' && prev_char == ':') {
            // TODO: Handle escaped strings
            i = j + 1;
            j = skip_until_char(content, i, content_len, '"');
            if(j == -1) {
                return 0;
            }
            field->value_len = j - i;
            field->value = calloc(field->value_len + 1, sizeof(char));
            strncpy(field->value, content+i, field->value_len);
            i = j;
            break;
        } else {
            fprintf(stderr, "Unwanted char: %d\n", content[j]);
            return 0;
        }
    }
    if(i == content_len) {
        return 0;
    }
    return i;
}

int json_schema_object_parser(struct JsonSchemaObject *object, const char* content, int start, size_t content_len) {
    int i = start;
    int j = start;
    char prev_char = 0;
    while(i < content_len) {
        j = skip_whitespace(content, i, content_len);
        if(j == -1) {
            return -1;
        }
        if(content[j] == '{' && prev_char == 0) {
            prev_char = '{';
            i = j + 1;
        } else if(content[j] == '"' && (prev_char == '{' || prev_char == ',')) {
            // TODO: This can be optimize break it into parts, compare key, then think about value
            struct JsonSchemaField field = {0};
            j = json_schema_field_parser(&field, content, j, content_len);
            if(!j) {
                free(field.name);
                free(field.value);
                return -2;
            }
            int k;
            //  TODO: Check, if the field already came
            for(k = 0; k<object->field_num; k++) {
                if(object->fields[k].name_len != field.name_len) {
                    continue;
                }
                if(strncmp(object->fields[k].name, field.name, field.name_len) == 0) {
                    break;
                }
            }
            if(k == object->field_num) {
                return -3;
            }
            object->fields[k].value = calloc(field.value_len + 1, sizeof(char));
            object->fields[k].value_len = field.value_len;
            strncpy(object->fields[k].value, field.value, field.value_len);
            i = j + 1;
            prev_char = '"';
            free(field.name);
            free(field.value);
        } else if(content[j] == ',' && prev_char == '"') {
            i = j + 1;
            prev_char = ',';
        } else if(content[j] == '}' && prev_char == '"') {
            i = j;
            break;
        } else if(isspace(content[j])) {
            i = j;
            continue;

        } else {
            return -4;
        }
    }
    if(i == content_len) {
        return -5;
    }
    j = skip_whitespace(content, i+1, content_len);
    if(j == -1) {
        return 1;
    }
    return 0;
}

void json_schema_object_free(struct JsonSchemaObject *object) {
    for(int i = 0; i < object->field_cap; ++i) {
        free(object->fields[i].name);
        free(object->fields[i].value);
    }
    free(object->fields);
    object->field_cap = 0;
    object->field_num = 0;
}


int main(void) {
    const char* file_path = "./jsons/hello.json";
    char* content = read_file(file_path);
    size_t content_len = strlen(content);
    if (content == NULL) {
        return -1;
    }
    struct JsonSchemaObject schema = {0};
    json_schema_object_init(&schema);
    json_schema_object_append_field(&schema, "message", 7, JSON_TYPE_STRING);
    json_schema_object_append_field(&schema, "length", 6, JSON_TYPE_STRING);
    int ret = json_schema_object_parser(&schema, content, 0, content_len);
    if(ret <= 0) {
        fprintf(stderr, "Failed to parse json, due to error %d\n", ret);
    } else {
        printf("Message: %s\n", schema.fields[0].value);
        printf("Message Length: %s\n", schema.fields[1].value);
    }
    json_schema_object_free(&schema);
    free(content);
    return 0;
}
