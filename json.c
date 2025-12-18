#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

/* Default initial capacity for fields array */
#define DEFAULT_FIELD_CAPACITY 64

/* Initial buffer size for file reading */
#define INITIAL_BUFFER_SIZE 4096

/* Growth factor for dynamic buffer expansion */
#define BUFFER_GROWTH_FACTOR 2

int json_schema_object_init(struct JsonSchemaObject *object)
{
    object->field_num = 0;
    object->field_cap = DEFAULT_FIELD_CAPACITY;
    object->fields = calloc(object->field_cap, sizeof(struct JsonSchemaField));
    
    if (object->fields == NULL) {
        fprintf(stderr, "Failed to allocate memory for schema fields\n");
        return -1;
    }
    
    return 0;
}

int json_schema_object_append_field(struct JsonSchemaObject *object,
                                    const char *key,
                                    size_t key_len,
                                    enum JsonType type)
{
    if (object->field_num >= object->field_cap) {
        size_t new_cap = object->field_cap * BUFFER_GROWTH_FACTOR;
        struct JsonSchemaField *new_fields = reallocarray(object->fields,
                                                          new_cap,
                                                          sizeof(struct JsonSchemaField));
        if (new_fields == NULL) {
            fprintf(stderr, "Failed to reallocate memory for schema fields\n");
            return -1;
        }
        
        /* Zero-initialize the new portion of the array */
        memset(new_fields + object->field_cap,
               0,
               (new_cap - object->field_cap) * sizeof(struct JsonSchemaField));
        
        object->fields = new_fields;
        object->field_cap = new_cap;
    }
    
    struct JsonSchemaField *field = &object->fields[object->field_num];
    
    field->name = calloc(key_len + 1, sizeof(char));
    if (field->name == NULL) {
        fprintf(stderr, "Failed to allocate memory for field name\n");
        return -1;
    }
    
    field->name_len = key_len;
    strncpy(field->name, key, key_len);
    field->type = type;
    object->field_num++;
    
    return 0;
}

int skip_whitespace(const char *content, int start, size_t content_len)
{
    int i = start;
    
    while (i < (int)content_len && isspace((unsigned char)content[i])) {
        i++;
    }
    
    if (i == (int)content_len) {
        return -1;
    }
    
    return i;
}

int skip_until_char(const char *content, int start, size_t content_len, char c)
{
    int i = start;
    
    while (i < (int)content_len && content[i] != c) {
        i++;
    }
    
    if (i == (int)content_len) {
        return -1;
    }
    
    return i;
}

int skip_until_unescaped_quote(const char *content, int start, size_t content_len)
{
    int i = start;
    
    while (i < (int)content_len) {
        if (content[i] == '"') {
            /* Check if this quote is escaped by counting preceding backslashes */
            int backslash_count = 0;
            int j = i - 1;
            
            while (j >= start && content[j] == '\\') {
                backslash_count++;
                j--;
            }
            
            /* Quote is unescaped if preceded by even number of backslashes */
            if (backslash_count % 2 == 0) {
                return i;
            }
        }
        i++;
    }
    
    return -1;
}

/**
 * Check if a field with the given name already exists in the object.
 * 
 * @param object The schema object to search.
 * @param name The field name to look for.
 * @param name_len Length of the field name.
 * @return Index of existing field, or -1 if not found.
 */
static int find_existing_field(struct JsonSchemaObject *object,
                               const char *name,
                               size_t name_len)
{
    for (size_t i = 0; i < object->field_num; i++) {
        if (object->fields[i].name_len != name_len) {
            continue;
        }
        if (strncmp(object->fields[i].name, name, name_len) == 0) {
            return (int)i;
        }
    }
    
    return -1;
}

int json_schema_field_parser(struct JsonSchemaField *field,
                             const char *content,
                             int start,
                             size_t content_len)
{
    int i = start;
    int j = start;
    char prev_char = 0;
    
    while (i < (int)content_len) {
        j = skip_whitespace(content, i, content_len);
        if (j == -1) {
            return 0;
        }
        
        if (content[j] == '"' && prev_char == 0) {
            /* Parse field name - handle escaped strings */
            i = j + 1;
            j = skip_until_unescaped_quote(content, i, content_len);
            if (j == -1) {
                return 0;
            }
            
            field->name_len = j - i;
            field->name = calloc(field->name_len + 1, sizeof(char));
            if (field->name == NULL) {
                return 0;
            }
            strncpy(field->name, content + i, field->name_len);
            i = j + 1;
            prev_char = '"';
            
        } else if (content[j] == ':' && prev_char == '"') {
            i = j + 1;
            prev_char = ':';
            
        } else if (content[j] == '"' && prev_char == ':') {
            /* Parse field value - handle escaped strings */
            i = j + 1;
            j = skip_until_unescaped_quote(content, i, content_len);
            if (j == -1) {
                return 0;
            }
            
            field->value_len = j - i;
            field->value = calloc(field->value_len + 1, sizeof(char));
            if (field->value == NULL) {
                return 0;
            }
            strncpy(field->value, content + i, field->value_len);
            i = j;
            break;
            
        } else {
            fprintf(stderr, "Unexpected character: '%c' (0x%02x)\n",
                    content[j], (unsigned char)content[j]);
            return 0;
        }
    }
    
    if (i == (int)content_len) {
        return 0;
    }
    
    return i;
}

int json_schema_object_parser(struct JsonSchemaObject *object,
                              const char *content,
                              int start,
                              size_t content_len)
{
    int i = start;
    int j = start;
    char prev_char = 0;
    
    while (i < (int)content_len) {
        j = skip_whitespace(content, i, content_len);
        if (j == -1) {
            return -1;
        }
        
        if (content[j] == '{' && prev_char == 0) {
            prev_char = '{';
            i = j + 1;
            
        } else if (content[j] == '"' && (prev_char == '{' || prev_char == ',')) {
            /* Parse field and match against schema */
            struct JsonSchemaField field = {0};
            j = json_schema_field_parser(&field, content, j, content_len);
            
            if (!j) {
                free(field.name);
                free(field.value);
                return -2;
            }
            
            /* Check if field already has a value (duplicate field detection) */
            int field_idx = find_existing_field(object, field.name, field.name_len);
            
            if (field_idx == -1) {
                /* Field not found in schema */
                free(field.name);
                free(field.value);
                return -3;
            }
            
            /* Check if this field was already parsed (duplicate in JSON) */
            if (object->fields[field_idx].value != NULL) {
                fprintf(stderr, "Duplicate field detected: %s\n", field.name);
                free(field.name);
                free(field.value);
                return -6;
            }
            
            /* Copy value to schema field */
            object->fields[field_idx].value = calloc(field.value_len + 1, sizeof(char));
            if (object->fields[field_idx].value == NULL) {
                free(field.name);
                free(field.value);
                return -7;
            }
            
            object->fields[field_idx].value_len = field.value_len;
            strncpy(object->fields[field_idx].value, field.value, field.value_len);
            
            i = j + 1;
            prev_char = '"';
            
            free(field.name);
            free(field.value);
            
        } else if (content[j] == ',' && prev_char == '"') {
            i = j + 1;
            prev_char = ',';
            
        } else if (content[j] == '}' && prev_char == '"') {
            i = j;
            break;
            
        } else if (isspace((unsigned char)content[j])) {
            i = j;
            continue;
            
        } else {
            return -4;
        }
    }
    
    if (i == (int)content_len) {
        return -5;
    }
    
    j = skip_whitespace(content, i + 1, content_len);
    if (j == -1) {
        return 1;
    }
    
    return 0;
}

void json_schema_object_free(struct JsonSchemaObject *object)
{
    for (size_t i = 0; i < object->field_cap; ++i) {
        free(object->fields[i].name);
        free(object->fields[i].value);
    }
    
    free(object->fields);
    object->fields = NULL;
    object->field_cap = 0;
    object->field_num = 0;
}

/**
 * Read entire file contents into a dynamically allocated buffer.
 * Handles files of any size by growing the buffer as needed.
 * 
 * @param path Path to the file to read.
 * @return Allocated buffer containing file contents, or NULL on error.
 *         Caller is responsible for freeing the returned buffer.
 */
static char *read_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    
    size_t buffer_size = INITIAL_BUFFER_SIZE;
    size_t total_read = 0;
    char *content = malloc(buffer_size);
    
    if (content == NULL) {
        fprintf(stderr, "Failed to allocate memory for file content\n");
        fclose(fp);
        return NULL;
    }
    
    while (1) {
        size_t space_remaining = buffer_size - total_read - 1;
        size_t bytes_read = fread(content + total_read, 1, space_remaining, fp);
        total_read += bytes_read;
        
        if (feof(fp)) {
            content[total_read] = '\0';
            break;
        }
        
        if (ferror(fp)) {
            fprintf(stderr, "Failed to read from file '%s'\n", path);
            free(content);
            fclose(fp);
            return NULL;
        }
        
        /* Buffer full, need to grow */
        if (bytes_read == space_remaining) {
            size_t new_size = buffer_size * BUFFER_GROWTH_FACTOR;
            char *new_content = realloc(content, new_size);
            
            if (new_content == NULL) {
                fprintf(stderr, "Failed to grow buffer for file content\n");
                free(content);
                fclose(fp);
                return NULL;
            }
            
            content = new_content;
            buffer_size = new_size;
        }
    }
    
    if (fclose(fp) == EOF) {
        fprintf(stderr, "Failed to close file '%s': %s\n", path, strerror(errno));
    }
    
    return content;
}

int json_schema_object_parser_from_file(struct JsonSchemaObject *object,
                                        const char *file_path)
{
    char *content = read_file(file_path);
    if (content == NULL) {
        return -1;
    }
    
    size_t content_len = strlen(content);
    int ret = json_schema_object_parser(object, content, 0, content_len);
    free(content);
    
    return ret;
}
