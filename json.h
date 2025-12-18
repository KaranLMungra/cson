#ifndef JSON_H
#define JSON_H

#include <stdlib.h>

/**
 * JSON value types supported by the parser.
 */
enum JsonType {
    JSON_TYPE_STRING,
    JSON_TYPE_OBJECT,
};

/**
 * Represents a JSON string value.
 */
struct JsonSchemaString {
    char *value;
    size_t len;
};

/**
 * Represents a single field in a JSON object.
 * Contains the field name, value, and type information.
 */
struct JsonSchemaField {
    char *name;
    size_t name_len;
    char *value;
    size_t value_len;
    enum JsonType type;
};

/**
 * Represents a JSON object with a dynamic array of fields.
 * Uses a capacity-based growth strategy for efficient memory management.
 */
struct JsonSchemaObject {
    struct JsonSchemaField *fields;
    size_t field_num;
    size_t field_cap;
};

/**
 * Initialize a JSON schema object with default capacity.
 * 
 * @param object Pointer to the JsonSchemaObject to initialize.
 * @return 0 on success, -1 on memory allocation failure.
 */
int json_schema_object_init(struct JsonSchemaObject *object);

/**
 * Append a new field definition to the schema object.
 * 
 * @param object Pointer to the JsonSchemaObject.
 * @param key The field name.
 * @param key_len Length of the field name.
 * @param type The expected JSON type for this field.
 * @return 0 on success, -1 on memory allocation failure.
 */
int json_schema_object_append_field(struct JsonSchemaObject *object,
                                    const char *key,
                                    size_t key_len,
                                    enum JsonType type);

/**
 * Skip whitespace characters in the content buffer.
 * 
 * @param content The content buffer to scan.
 * @param start Starting position in the buffer.
 * @param content_len Total length of the content.
 * @return Position of first non-whitespace character, or -1 if end reached.
 */
int skip_whitespace(const char *content, int start, size_t content_len);

/**
 * Skip characters until a specific character is found.
 * 
 * @param content The content buffer to scan.
 * @param start Starting position in the buffer.
 * @param content_len Total length of the content.
 * @param c The character to find.
 * @return Position of the found character, or -1 if not found.
 */
int skip_until_char(const char *content, int start, size_t content_len, char c);

/**
 * Skip characters until an unescaped quote is found.
 * Handles escape sequences within JSON strings.
 * 
 * @param content The content buffer to scan.
 * @param start Starting position (after opening quote).
 * @param content_len Total length of the content.
 * @return Position of the closing quote, or -1 if not found.
 */
int skip_until_unescaped_quote(const char *content, int start, size_t content_len);

/**
 * Parse a single JSON field (key-value pair).
 * 
 * @param field Pointer to store the parsed field.
 * @param content The JSON content buffer.
 * @param start Starting position in the buffer.
 * @param content_len Total length of the content.
 * @return Position after the parsed field, or 0 on error.
 */
int json_schema_field_parser(struct JsonSchemaField *field,
                             const char *content,
                             int start,
                             size_t content_len);

/**
 * Parse a JSON object from a content buffer.
 * 
 * @param object Pointer to the initialized JsonSchemaObject.
 * @param content The JSON content buffer.
 * @param start Starting position in the buffer.
 * @param content_len Total length of the content.
 * @return 1 on success (end of content), 0 on success (more content), negative on error.
 */
int json_schema_object_parser(struct JsonSchemaObject *object,
                              const char *content,
                              int start,
                              size_t content_len);

/**
 * Parse a JSON object from a file.
 * 
 * @param object Pointer to the initialized JsonSchemaObject.
 * @param file_path Path to the JSON file.
 * @return Same as json_schema_object_parser, or -1 on file read error.
 */
int json_schema_object_parser_from_file(struct JsonSchemaObject *object,
                                        const char *file_path);

/**
 * Free all memory associated with a JSON schema object.
 * 
 * @param object Pointer to the JsonSchemaObject to free.
 */
void json_schema_object_free(struct JsonSchemaObject *object);

#endif /* JSON_H */
