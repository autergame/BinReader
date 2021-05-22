/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/


#if !defined(_CRT_SECURE_NO_DEPRECATE) && defined(_MSC_VER)
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif
#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4001)
#endif

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <float.h>

#if defined(_MSC_VER)
#pragma warning (pop)
#endif
#ifdef __GNUC__
#pragma GCC visibility pop
#endif

#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

    typedef enum jType
    {
        jinvalid = 0,
        jfalse = 1,
        jtrue = 2, 
        jnull = 3,
        jnumber = 4,
        jstring = 6,
        jarray = 7,
        jobject = 8,
        jraw = 9
    } jType;

    typedef enum jnType
    {
        jNone = 0,
        jSInt8 = 1,
        jUInt8 = 2,
        jSInt16 = 3,
        jUInt16 = 4,
        jSInt32 = 5,
        jUInt32 = 6,
        jSInt64 = 7,
        jUInt64 = 8,
        jFloat32 = 9
    } jnType;

    typedef struct cJSON
    {
        struct cJSON* next;
        struct cJSON* prev;
        struct cJSON* child;

        jType type;
        jnType typen;
        void* value;
        char* string;
    } cJSON;

#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

    cJSON* cJSON_Parse(const char* value);
    cJSON* cJSON_ParseWithLength(const char* value, size_t buffer_length);
    cJSON* cJSON_ParseWithOpts(const char* value, const char** return_parse_end, uint8_t require_null_terminated);
    cJSON* cJSON_ParseWithLengthOpts(const char* value, size_t buffer_length, const char** return_parse_end, uint8_t require_null_terminated);

    char* cJSON_Print(const cJSON* item, uint8_t format);
    void cJSON_Delete(cJSON* item);

    uint32_t cJSON_GetArraySize(const cJSON* array);
    cJSON* cJSON_GetArrayItem(const cJSON* array, int index);
    cJSON* cJSON_GetObjectItem(const cJSON* const object, const char* const string);
    uint8_t cJSON_HasObjectItem(const cJSON* object, const char* string);
    const char* cJSON_GetErrorPtr();

    cJSON* cJSON_CreateNull();
    cJSON* cJSON_CreateBool(uint8_t boolean);
    cJSON* cJSON_CreateNumber(void* num, jnType type);
    cJSON* cJSON_CreateString(const char* string);
    cJSON* cJSON_CreateRaw(const char* raw);
    cJSON* cJSON_CreateArray();
    cJSON* cJSON_CreateObject();

    uint8_t cJSON_AddItemToArray(cJSON* array, cJSON* item);
    uint8_t cJSON_AddItemToObject(cJSON* object, const char* string, cJSON* item);

#ifdef __cplusplus
}
#endif

#ifndef isinf
#define isinf(d) (isnan((d - d)) && !isnan(d))
#endif
#ifndef isnan
#define isnan(d) (d != d)
#endif

typedef struct {
    const unsigned char* json;
    int position;
} error;
static error global_error = { NULL, 0 };

const char* cJSON_GetErrorPtr()
{
    return (const char*)(global_error.json + global_error.position);
}

static unsigned char* cJSON_strdup(const unsigned char* string)
{
    size_t length = 0;
    unsigned char* copy = NULL;

    if (string == NULL)
    {
        return NULL;
    }

    length = strlen((const char*)string) + sizeof("");
    copy = (unsigned char*)calloc(length, 1);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, string, length);

    return copy;
}

static cJSON* cJSON_New_Item()
{
    cJSON* node = (cJSON*)calloc(1, sizeof(cJSON));
    if (node)
    {
        memset(node, '\0', sizeof(cJSON));
    }

    return node;
}

void cJSON_Delete(cJSON* item)
{
    cJSON* next = NULL;
    while (item != NULL)
    {
        next = item->next;
        free(item);
        item = next;
    }
}

typedef struct
{
    const unsigned char* content;
    size_t length;
    size_t offset;
    size_t depth;              
} parse_buffer;

#define can_read(buffer, size) ((buffer != NULL) && (((buffer)->offset + size) <= (buffer)->length))
#define can_access_at_index(buffer, index) ((buffer != NULL) && (((buffer)->offset + index) < (buffer)->length))
#define cannot_access_at_index(buffer, index) (!can_access_at_index(buffer, index))
#define buffer_at_offset(buffer) ((buffer)->content + (buffer)->offset)

static uint8_t parse_number(cJSON* const item, parse_buffer* const input_buffer)
{
    size_t i, length = 0;
    char number_c_string[32];
    uint8_t floate = 0, minus = 0;

    if ((input_buffer == NULL) || (input_buffer->content == NULL))
    {
        return 0;
    }

    for (i = 0; (i < (sizeof(number_c_string) - 1)) && can_access_at_index(input_buffer, i); i++)
    {
        switch (buffer_at_offset(input_buffer)[i])
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '+':
            case 'e':
            case 'E':
                number_c_string[i] = buffer_at_offset(input_buffer)[i];
                break;
            case '-':
            {
                minus = 1;
                number_c_string[i] = '-';
                break;
            }
            case '.':
            {
                floate = 1;
                number_c_string[i] = '.';
                break;
            }
            default:
                goto loop_end;
        }
    }
loop_end:
    number_c_string[i] = '\0';

    length = strlen(number_c_string);
    if (length == 0)
        return 0;

    item->type = jnumber;
    if (floate)
    {
        float* number = (float*)calloc(1, 4);
        sscanf(number_c_string, "%g", number);
        item->typen = jFloat32;
        item->value = number;
    }
    else if (minus)
    {
        int64_t* number = (int64_t*)calloc(1, 8);
        sscanf(number_c_string, "%" PRIi64, number);
        item->typen = jSInt64;
        item->value = number;
    }
    else
    {
        uint64_t* number = (uint64_t*)calloc(1, 8);
        sscanf(number_c_string, "%" PRIu64, number);
        item->typen = jUInt64;
        item->value = number;
    }

    input_buffer->offset += length;
    return 1;
}

typedef struct
{
    unsigned char* buffer;
    size_t length;
    size_t offset;
    size_t depth;        
    uint8_t noalloc;
    uint8_t format;        
} printbuffer;

static unsigned char* ensure(printbuffer* const p, size_t needed)
{
    unsigned char* newbuffer = NULL;
    size_t newsize = 0;

    if ((p == NULL) || (p->buffer == NULL))
    {
        return NULL;
    }

    if ((p->length > 0) && (p->offset >= p->length))
    {
        return NULL;
    }

    if (needed > INT64_MAX)
    {
        return NULL;
    }

    needed += p->offset + 1;
    if (needed <= p->length)
    {
        return p->buffer + p->offset;
    }

    if (p->noalloc) {
        return NULL;
    }

    if (needed > (SIZE_MAX / 2))
    {
        if (needed <= SIZE_MAX)
        {
            newsize = SIZE_MAX;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        newsize = needed * 2;
    }

    newbuffer = (unsigned char*)realloc(p->buffer, newsize);
    if (newbuffer == NULL)
    {
        free(p->buffer);
        p->length = 0;
        p->buffer = NULL;

        return NULL;
    }

    p->length = newsize;
    p->buffer = newbuffer;

    return newbuffer + p->offset;
}

static void update_offset(printbuffer* const buffer)
{
    const unsigned char* buffer_pointer = NULL;
    if ((buffer == NULL) || (buffer->buffer == NULL))
    {
        return;
    }
    buffer_pointer = buffer->buffer + buffer->offset;

    buffer->offset += strlen((const char*)buffer_pointer);
}

static uint8_t print_number(const cJSON* const item, printbuffer* const output_buffer)
{
    size_t i = 0;
    int length = 0;
    unsigned char* output_pointer = NULL;
    unsigned char number_buffer[26] = { 0 };         

    if (output_buffer == NULL)
        return 0;

    if (item->typen == jFloat32)
    {
        float d = *(float*)item->value;
        if (isnan(d) || isinf(d))
            length = sprintf((char*)number_buffer, "null");
        else
            length = sprintf((char*)number_buffer, "%.9g", d);

        uint8_t havepoint = 0;
        for (i = 0; i < ((size_t)length); i++)
            if (number_buffer[i] == '.')
                havepoint = 1;

        if (havepoint == 0)
            length = sprintf((char*)number_buffer, "%.9g.0", d);

        if ((length < 0) || (length > (int)(sizeof(number_buffer) - 1)))
            return 0;

        output_pointer = ensure(output_buffer, (size_t)length + sizeof(""));
        if (output_pointer == NULL)
            return 0;

        for (i = 0; i < ((size_t)length); i++)
            output_pointer[i] = number_buffer[i];
    }
    else
    {
        switch (item->typen)
        {
            case jSInt8:
                length = sprintf((char*)number_buffer, "%" PRIi8, *(int8_t*)item->value);
                break;
            case jUInt8:
                length = sprintf((char*)number_buffer, "%" PRIu8, *(uint8_t*)item->value);
                break;
            case jSInt16:
                length = sprintf((char*)number_buffer, "%" PRIi16, *(int16_t*)item->value);
                break;
            case jUInt16:
                length = sprintf((char*)number_buffer, "%" PRIu16, *(uint16_t*)item->value);
                break;
            case jSInt32:
                length = sprintf((char*)number_buffer, "%" PRIi32, *(int32_t*)item->value);
                break;
            case jUInt32:
                length = sprintf((char*)number_buffer, "%" PRIu32, *(uint32_t*)item->value);
                break;
            case jSInt64:
                length = sprintf((char*)number_buffer, "%" PRIi64, *(int64_t*)item->value);
                break;
            case jUInt64:
                length = sprintf((char*)number_buffer, "%" PRIu64, *(uint64_t*)item->value);
                break;
        }     
        output_pointer = ensure(output_buffer, (size_t)length + sizeof(""));
        if (output_pointer == NULL)
            return 0;
        for (i = 0; i < ((size_t)length); i++)
            output_pointer[i] = number_buffer[i];
    }

    output_pointer[i] = '\0';
    output_buffer->offset += (size_t)length;

    return 1;
}

static unsigned parse_hex4(const unsigned char* const input)
{
    unsigned int h = 0;
    size_t i = 0;

    for (i = 0; i < 4; i++)
    {
        if ((input[i] >= '0') && (input[i] <= '9'))
        {
            h += (unsigned int)input[i] - '0';
        }
        else if ((input[i] >= 'A') && (input[i] <= 'F'))
        {
            h += (unsigned int)10 + input[i] - 'A';
        }
        else if ((input[i] >= 'a') && (input[i] <= 'f'))
        {
            h += (unsigned int)10 + input[i] - 'a';
        }
        else   
        {
            return 0;
        }

        if (i < 3)
        {
            h = h << 4;
        }
    }

    return h;
}

static unsigned char utf16_literal_to_utf8(const unsigned char* const input_pointer, const unsigned char* const input_end, unsigned char** output_pointer)
{
    long unsigned int codepoint = 0;
    unsigned int first_code = 0;
    const unsigned char* first_sequence = input_pointer;
    unsigned char utf8_length = 0;
    unsigned char utf8_position = 0;
    unsigned char sequence_length = 0;
    unsigned char first_byte_mark = 0;

    if ((input_end - first_sequence) < 6)
    {
        goto fail;
    }

    first_code = parse_hex4(first_sequence + 2);

    if (((first_code >= 0xDC00) && (first_code <= 0xDFFF)))
    {
        goto fail;
    }

    if ((first_code >= 0xD800) && (first_code <= 0xDBFF))
    {
        const unsigned char* second_sequence = first_sequence + 6;
        unsigned int second_code = 0;
        sequence_length = 12;   

        if ((input_end - second_sequence) < 6)
        {
            goto fail;
        }

        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u'))
        {
            goto fail;
        }

        second_code = parse_hex4(second_sequence + 2);
        if ((second_code < 0xDC00) || (second_code > 0xDFFF))
        {
            goto fail;
        }


        codepoint = 0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
    }
    else
    {
        sequence_length = 6;   
        codepoint = first_code;
    }

    if (codepoint < 0x80)
    {
        utf8_length = 1;
    }
    else if (codepoint < 0x800)
    {
        utf8_length = 2;
        first_byte_mark = 0xC0;   
    }
    else if (codepoint < 0x10000)
    {
        utf8_length = 3;
        first_byte_mark = 0xE0;   
    }
    else if (codepoint <= 0x10FFFF)
    {
        utf8_length = 4;
        first_byte_mark = 0xF0;   
    }
    else
    {
        goto fail;
    }

    for (utf8_position = (unsigned char)(utf8_length - 1); utf8_position > 0; utf8_position--)
    {
        (*output_pointer)[utf8_position] = (unsigned char)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    if (utf8_length > 1)
    {
        (*output_pointer)[0] = (unsigned char)((codepoint | first_byte_mark) & 0xFF);
    }
    else
    {
        (*output_pointer)[0] = (unsigned char)(codepoint & 0x7F);
    }

    *output_pointer += utf8_length;

    return sequence_length;

fail:
    return 0;
}

static uint8_t parse_string(cJSON* const item, parse_buffer* const input_buffer)
{
    const unsigned char* input_pointer = buffer_at_offset(input_buffer) + 1;
    const unsigned char* input_end = buffer_at_offset(input_buffer) + 1;
    unsigned char* output_pointer = NULL;
    unsigned char* output = NULL;

    if (buffer_at_offset(input_buffer)[0] != '\"')
    {
        goto fail;
    }

    {
        size_t allocation_length = 0;
        size_t skipped_bytes = 0;
        while (((size_t)(input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"'))
        {
            if (input_end[0] == '\\')
            {
                if ((size_t)(input_end + 1 - input_buffer->content) >= input_buffer->length)
                {
                    goto fail;
                }
                skipped_bytes++;
                input_end++;
            }
            input_end++;
        }
        if (((size_t)(input_end - input_buffer->content) >= input_buffer->length) || (*input_end != '\"'))
        {
            goto fail;     
        }

        allocation_length = (size_t)(input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
        output = (unsigned char*)calloc(allocation_length + 1, 1);
        if (output == NULL)
        {
            goto fail;    
        }
    }

    output_pointer = output;
    while (input_pointer < input_end)
    {
        if (*input_pointer != '\\')
        {
            *output_pointer++ = *input_pointer++;
        }
        else
        {
            unsigned char sequence_length = 2;
            if ((input_end - input_pointer) < 1)
            {
                goto fail;
            }

            switch (input_pointer[1])
            {
            case 'b':
                *output_pointer++ = '\b';
                break;
            case 'f':
                *output_pointer++ = '\f';
                break;
            case 'n':
                *output_pointer++ = '\n';
                break;
            case 'r':
                *output_pointer++ = '\r';
                break;
            case 't':
                *output_pointer++ = '\t';
                break;
            case '\"':
            case '\\':
            case '/':
                *output_pointer++ = input_pointer[1];
                break;

            case 'u':
                sequence_length = utf16_literal_to_utf8(input_pointer, input_end, &output_pointer);
                if (sequence_length == 0)
                {
                    goto fail;
                }
                break;

            default:
                goto fail;
            }
            input_pointer += sequence_length;
        }
    }

    *output_pointer = '\0';

    item->type = jstring;
    item->value = (char*)output;

    input_buffer->offset = (size_t)(input_end - input_buffer->content);
    input_buffer->offset++;

    return 1;

fail:
    if (output != NULL)
    {
        free(output);
    }

    if (input_pointer != NULL)
    {
        input_buffer->offset = (size_t)(input_pointer - input_buffer->content);
    }

    return 0;
}

static uint8_t print_string_ptr(const unsigned char* const input, printbuffer* const output_buffer)
{
    const unsigned char* input_pointer = NULL;
    unsigned char* output = NULL;
    unsigned char* output_pointer = NULL;
    size_t output_length = 0;
    size_t escape_characters = 0;

    if (output_buffer == NULL)
    {
        return 0;
    }

    if (input == NULL)
    {
        output = ensure(output_buffer, sizeof("\"\""));
        if (output == NULL)
        {
            return 0;
        }
        strcpy((char*)output, "\"\"");

        return 1;
    }

    for (input_pointer = input; *input_pointer; input_pointer++)
    {
        switch (*input_pointer)
        {
        case '\"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            escape_characters++;
            break;
        default:
            if (*input_pointer < 32)
            {
                escape_characters += 5;
            }
            break;
        }
    }
    output_length = (size_t)(input_pointer - input) + escape_characters;

    output = ensure(output_buffer, output_length + sizeof("\"\""));
    if (output == NULL)
    {
        return 0;
    }

    if (escape_characters == 0)
    {
        output[0] = '\"';
        memcpy(output + 1, input, output_length);
        output[output_length + 1] = '\"';
        output[output_length + 2] = '\0';

        return 1;
    }

    output[0] = '\"';
    output_pointer = output + 1;
    for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
    {
        if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
        {
            *output_pointer = *input_pointer;
        }
        else
        {
            *output_pointer++ = '\\';
            switch (*input_pointer)
            {
            case '\\':
                *output_pointer = '\\';
                break;
            case '\"':
                *output_pointer = '\"';
                break;
            case '\b':
                *output_pointer = 'b';
                break;
            case '\f':
                *output_pointer = 'f';
                break;
            case '\n':
                *output_pointer = 'n';
                break;
            case '\r':
                *output_pointer = 'r';
                break;
            case '\t':
                *output_pointer = 't';
                break;
            default:
                sprintf((char*)output_pointer, "u%04x", *input_pointer);
                output_pointer += 4;
                break;
            }
        }
    }
    output[output_length + 1] = '\"';
    output[output_length + 2] = '\0';

    return 1;
}

static uint8_t print_string(const cJSON* const item, printbuffer* const p)
{
    return print_string_ptr((unsigned char*)item->value, p);
}

static uint8_t parse_value(cJSON* const item, parse_buffer* const input_buffer);
static uint8_t print_value(const cJSON* const item, printbuffer* const output_buffer);
static uint8_t parse_array(cJSON* const item, parse_buffer* const input_buffer);
static uint8_t print_array(const cJSON* const item, printbuffer* const output_buffer);
static uint8_t parse_object(cJSON* const item, parse_buffer* const input_buffer);
static uint8_t print_object(const cJSON* const item, printbuffer* const output_buffer);

static parse_buffer* buffer_skip_whitespace(parse_buffer* const buffer)
{
    if ((buffer == NULL) || (buffer->content == NULL))
    {
        return NULL;
    }

    if (cannot_access_at_index(buffer, 0))
    {
        return buffer;
    }

    while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] <= 32))
    {
        buffer->offset++;
    }

    if (buffer->offset == buffer->length)
    {
        buffer->offset--;
    }

    return buffer;
}

static parse_buffer* skip_utf8_bom(parse_buffer* const buffer)
{
    if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0))
    {
        return NULL;
    }

    if (can_access_at_index(buffer, 4) && (strncmp((const char*)buffer_at_offset(buffer), "\xEF\xBB\xBF", 3) == 0))
    {
        buffer->offset += 3;
    }

    return buffer;
}

cJSON* cJSON_ParseWithOpts(const char* value, const char** return_parse_end, uint8_t require_null_terminated)
{
    size_t buffer_length;

    if (NULL == value)
    {
        return NULL;
    }

    buffer_length = strlen(value) + sizeof("");

    return cJSON_ParseWithLengthOpts(value, buffer_length, return_parse_end, require_null_terminated);
}

cJSON* cJSON_ParseWithLengthOpts(const char* value, size_t buffer_length, const char** return_parse_end, uint8_t require_null_terminated)
{
    parse_buffer buffer = { 0, 0, 0, 0 };
    cJSON* item = NULL;

    global_error.json = NULL;
    global_error.position = 0;

    if (value == NULL || 0 == buffer_length)
    {
        goto fail;
    }

    buffer.content = (const unsigned char*)value;
    buffer.length = buffer_length;
    buffer.offset = 0;

    item = cJSON_New_Item();
    if (item == NULL)    
    {
        goto fail;
    }

    if (!parse_value(item, buffer_skip_whitespace(skip_utf8_bom(&buffer))))
    {
        goto fail;
    }

    if (require_null_terminated)
    {
        buffer_skip_whitespace(&buffer);
        if ((buffer.offset >= buffer.length) || buffer_at_offset(&buffer)[0] != '\0')
        {
            goto fail;
        }
    }
    if (return_parse_end)
    {
        *return_parse_end = (const char*)buffer_at_offset(&buffer);
    }

    return item;

fail:
    if (item != NULL)
    {
        cJSON_Delete(item);
    }

    if (value != NULL)
    {
        error local_error;
        local_error.json = (const unsigned char*)value;
        local_error.position = 0;

        if (buffer.offset < buffer.length)
        {
            local_error.position = (int)buffer.offset;
        }
        else if (buffer.length > 0)
        {
            local_error.position = (int)buffer.length - 1;
        }

        if (return_parse_end != NULL)
        {
            *return_parse_end = (const char*)local_error.json + local_error.position;
        }

        global_error = local_error;
    }

    return NULL;
}

cJSON* cJSON_Parse(const char* value)
{
    return cJSON_ParseWithOpts(value, 0, 0);
}

cJSON* cJSON_ParseWithLength(const char* value, size_t buffer_length)
{
    return cJSON_ParseWithLengthOpts(value, buffer_length, 0, 0);
}

#define cjson_min(a, b) (((a) < (b)) ? (a) : (b))

char* cJSON_Print(const cJSON* const item, uint8_t format)
{
    static const size_t default_buffer_size = 256;
    printbuffer buffer[1];
    char* printed = NULL;

    memset(buffer, 0, sizeof(buffer));

    buffer->buffer = (unsigned char*)calloc(default_buffer_size, 1);
    buffer->length = default_buffer_size;
    buffer->format = format;
    if (buffer->buffer == NULL)
    {
        goto fail;
    }

    if (!print_value(item, buffer))
    {
        goto fail;
    }
    update_offset(buffer);

    printed = (char*)realloc(buffer->buffer, buffer->offset + 1);
    if (printed == NULL) {
        goto fail;
    }
    buffer->buffer = NULL;

    return printed;

fail:
    if (buffer->buffer != NULL)
    {
        free(buffer->buffer);
    }

    if (printed != NULL)
    {
        free(printed);
    }

    return NULL;
}

static uint8_t parse_value(cJSON* const item, parse_buffer* const input_buffer)
{
    if ((input_buffer == NULL) || (input_buffer->content == NULL))
    {
        return 0;    
    }
    uint32_t* number = (uint32_t*)calloc(1, 4);
    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "null", 4) == 0))
    {
        item->type = jnull;
        input_buffer->offset += 4;
        return 1;
    }
    if (can_read(input_buffer, 5) && (strncmp((const char*)buffer_at_offset(input_buffer), "false", 5) == 0))
    {
        *number = 0;
        item->type = jfalse;
        item->value = number;
        input_buffer->offset += 5;
        return 1;
    }
    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "true", 4) == 0))
    {
        *number = 1;
        item->type = jtrue;
        item->value = number;
        input_buffer->offset += 4;
        return 1;
    }
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '\"'))
    {
        return parse_string(item, input_buffer);
    }
    if (can_access_at_index(input_buffer, 0) && ((buffer_at_offset(input_buffer)[0] == '-') || ((buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(input_buffer)[0] <= '9'))))
    {
        return parse_number(item, input_buffer);
    }
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '['))
    {
        return parse_array(item, input_buffer);
    }
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '{'))
    {
        return parse_object(item, input_buffer);
    }

    return 0;
}

static uint8_t print_value(const cJSON* const item, printbuffer* const output_buffer)
{
    unsigned char* output = NULL;

    if ((item == NULL) || (output_buffer == NULL))
    {
        return 0;
    }

    switch (item->type)
    {
    case jnull:
        output = ensure(output_buffer, 5);
        if (output == NULL)
        {
            return 0;
        }
        strcpy((char*)output, "null");
        return 1;

    case jfalse:
        output = ensure(output_buffer, 6);
        if (output == NULL)
        {
            return 0;
        }
        strcpy((char*)output, "false");
        return 1;

    case jtrue:
        output = ensure(output_buffer, 5);
        if (output == NULL)
        {
            return 0;
        }
        strcpy((char*)output, "true");
        return 1;

    case jnumber:
        return print_number(item, output_buffer);

    case jraw:
    {
        size_t raw_length = 0;
        if (item->value == NULL)
        {
            return 0;
        }

        raw_length = strlen((char*)item->value) + sizeof("");
        output = ensure(output_buffer, raw_length);
        if (output == NULL)
        {
            return 0;
        }
        memcpy(output, item->value, raw_length);
        return 1;
    }

    case jstring:
        return print_string(item, output_buffer);

    case jarray:
        return print_array(item, output_buffer);

    case jobject:
        return print_object(item, output_buffer);

    default:
        return 0;
    }
}

static uint8_t parse_array(cJSON* const item, parse_buffer* const input_buffer)
{
    cJSON* head = NULL;       
    cJSON* current_item = NULL;

    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        return 0;     
    }
    input_buffer->depth++;

    if (buffer_at_offset(input_buffer)[0] != '[')
    {
        goto fail;
    }

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ']'))
    {
        goto success;
    }

    if (cannot_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    input_buffer->offset--;
    do
    {
        cJSON* new_item = cJSON_New_Item();
        if (new_item == NULL)
        {
            goto fail;    
        }

        if (head == NULL)
        {
            current_item = head = new_item;
        }
        else
        {
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer))
        {
            goto fail;      
        }
        buffer_skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (cannot_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != ']')
    {
        goto fail;      
    }

success:
    input_buffer->depth--;

    if (head != NULL) {
        head->prev = current_item;
    }

    item->type = jarray;
    item->child = head;

    input_buffer->offset++;

    return 1;

fail:
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    return 0;
}

static uint8_t print_array(const cJSON* const item, printbuffer* const output_buffer)
{
    unsigned char* output_pointer = NULL;
    size_t length = 0;
    cJSON* current_element = item->child;
    size_t i;

    if (output_buffer == NULL)
    {
        return 0;
    }

    output_pointer = ensure(output_buffer, 1);
    if (output_pointer == NULL)
    {
        return 0;
    }

    *output_pointer = '[';
    output_buffer->offset++;
    output_buffer->depth++;

    if (current_element != NULL)
    {
        if (output_buffer->format && current_element->type != jobject)
        {
            output_pointer = ensure(output_buffer, output_buffer->depth + 1);
            if (output_pointer == NULL)
            {
                return 0;
            }

            *output_pointer++ = '\n';

            for (i = 0; i < output_buffer->depth; i++)
            {
                *output_pointer++ = '\t';
            }

            output_buffer->offset += output_buffer->depth + 1;
        }
    }

    while (current_element != NULL)
    {
        if (!print_value(current_element, output_buffer))
        {
            return 0;
        }
        update_offset(output_buffer);
        if (current_element->next)
        {
            if (output_buffer->format)
                if (current_element->type == jobject || current_element->type == jnumber)
                    length = (size_t)2;
                else
                    length = (size_t)2 + output_buffer->depth;
            else
                length = (size_t)1;
            output_pointer = ensure(output_buffer, length);
            if (output_pointer == NULL)
            {
                return 0;
            }
            *output_pointer++ = ',';
            if (output_buffer->format)
            {
                if (current_element->type == jobject || current_element->type == jnumber)
                {
                    *output_pointer++ = ' ';
                }
                else
                {
                    *output_pointer++ = '\n';
                    for (i = 0; i < output_buffer->depth; i++)
                    {
                        *output_pointer++ = '\t';
                    }
                }
            }
            *output_pointer = '\0';
            output_buffer->offset += length;
        }
        current_element = current_element->next;
    }

    if (output_buffer->format)
    {
        output_pointer = ensure(output_buffer, output_buffer->depth);
        if (output_pointer == NULL)
        {
            return 0;
        }

        *output_pointer++ = '\n';

        for (i = 0; i < output_buffer->depth - 1; i++)
        {
            *output_pointer++ = '\t';
        }

        output_buffer->offset += output_buffer->depth;
    }

    output_pointer = ensure(output_buffer, 2);
    if (output_pointer == NULL)
    {
        return 0;
    }
    *output_pointer++ = ']';
    *output_pointer = '\0';
    output_buffer->depth--;

    return 1;
}

static uint8_t parse_object(cJSON* const item, parse_buffer* const input_buffer)
{
    cJSON* head = NULL;     
    cJSON* current_item = NULL;

    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        return 0;     
    }
    input_buffer->depth++;

    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '{'))
    {
        goto fail;     
    }

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '}'))
    {
        goto success;    
    }

    if (cannot_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    input_buffer->offset--;
    do
    {
        cJSON* new_item = cJSON_New_Item();
        if (new_item == NULL)
        {
            goto fail;    
        }

        if (head == NULL)
        {
            current_item = head = new_item;
        }
        else
        {
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_string(current_item, input_buffer))
        {
            goto fail;      
        }
        buffer_skip_whitespace(input_buffer);

        current_item->string = (char*)current_item->value;
        current_item->value = NULL;

        if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':'))
        {
            goto fail;    
        }

        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer))
        {
            goto fail;      
        }
        buffer_skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '}'))
    {
        goto fail;      
    }

success:
    input_buffer->depth--;

    if (head != NULL) {
        head->prev = current_item;
    }

    item->type = jobject;
    item->child = head;

    input_buffer->offset++;
    return 1;

fail:
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    return 0;
}

static uint8_t print_object(const cJSON* const item, printbuffer* const output_buffer)
{
    unsigned char* output_pointer = NULL;
    size_t length = 0;
    size_t i;
    cJSON* current_item = item->child;

    if (output_buffer == NULL)
    {
        return 0;
    }

    if (output_buffer->format)
    {
        length = (size_t)(output_buffer->depth ? output_buffer->depth + 1 : 0) + 2;       
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == NULL)
        {
            return 0;
        }

        if (output_buffer->depth)
        {
            *output_pointer++ = '\n';
        }

        for (i = 0; i < output_buffer->depth; i++)
        {
            *output_pointer++ = '\t';
        }

        *output_pointer++ = '{';
        *output_pointer++ = '\n';

        output_buffer->depth++;
        output_buffer->offset += length;

    }
    else
    {
        length = (size_t)1;    
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == NULL)
        {
            return 0;
        }

        *output_pointer++ = '{';

        output_buffer->depth++;
        output_buffer->offset += length;
    }

    while (current_item)
    {
        if (output_buffer->format)
        {
            size_t i;
            output_pointer = ensure(output_buffer, output_buffer->depth);
            if (output_pointer == NULL)
            {
                return 0;
            }
            for (i = 0; i < output_buffer->depth; i++)
            {
                *output_pointer++ = '\t';
            }
            output_buffer->offset += output_buffer->depth;
        }

        if (!print_string_ptr((unsigned char*)current_item->string, output_buffer))
        {
            return 0;
        }
        update_offset(output_buffer);

        length = (size_t)(output_buffer->format ? 2 : 1);
        output_pointer = ensure(output_buffer, length);
        if (output_pointer == NULL)
        {
            return 0;
        }
        *output_pointer++ = ':';
        if (output_buffer->format)
        {
            *output_pointer++ = ' ';
        }
        output_buffer->offset += length;

        if (!print_value(current_item, output_buffer))
        {
            return 0;
        }
        update_offset(output_buffer);

        length = ((size_t)(output_buffer->format ? 1 : 0) + (size_t)(current_item->next ? 1 : 0));
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == NULL)
        {
            return 0;
        }
        if (current_item->next)
        {
            *output_pointer++ = ',';
        }

        if (output_buffer->format)
        {
            *output_pointer++ = '\n';
        }
        *output_pointer = '\0';
        output_buffer->offset += length;

        current_item = current_item->next;
    }

    output_pointer = ensure(output_buffer, output_buffer->format ? (output_buffer->depth + 1) : 2);
    if (output_pointer == NULL)
    {
        return 0;
    }
    if (output_buffer->format)
    {
        size_t i;
        for (i = 0; i < (output_buffer->depth - 1); i++)
        {
            *output_pointer++ = '\t';
        }
    }
    *output_pointer++ = '}';
    *output_pointer = '\0';
    output_buffer->depth--;

    return 1;
}

uint32_t cJSON_GetArraySize(const cJSON* array)
{
    cJSON* child = NULL;
    uint32_t size = 0;

    if (array == NULL)
    {
        return 0;
    }

    child = array->child;

    while (child != NULL)
    {
        size++;
        child = child->next;
    }

    return (int)size;
}

static cJSON* get_array_item(const cJSON* array, size_t index)
{
    cJSON* current_child = NULL;

    if (array == NULL)
    {
        return NULL;
    }

    current_child = array->child;
    while ((current_child != NULL) && (index > 0))
    {
        index--;
        current_child = current_child->next;
    }

    return current_child;
}

cJSON* cJSON_GetArrayItem(const cJSON* array, int index)
{
    if (index < 0)
    {
        return NULL;
    }

    return get_array_item(array, (size_t)index);
}

static cJSON* cJSON_GetObjectItem(const cJSON* const object, const char* const name)
{
    cJSON* current_element = NULL;

    if ((object == NULL) || (name == NULL))
    {
        return NULL;
    }

    current_element = object->child;
    while ((current_element != NULL) && (current_element->string != NULL) && strcmp(name, current_element->string) != 0)
    {
        current_element = current_element->next;
    }

    if ((current_element == NULL) || (current_element->string == NULL)) {
        return NULL;
    }

    return current_element;
}

uint8_t cJSON_HasObjectItem(const cJSON* object, const char* string)
{
    return cJSON_GetObjectItem(object, string) ? 1 : 0;
}

static void suffix_object(cJSON* prev, cJSON* item)
{
    prev->next = item;
    item->prev = prev;
}

static uint8_t add_item_to_array(cJSON* array, cJSON* item)
{
    cJSON* child = NULL;

    if ((item == NULL) || (array == NULL) || (array == item))
    {
        return 0;
    }

    child = array->child;
    if (child == NULL)
    {
        array->child = item;
        item->prev = item;
        item->next = NULL;
    }
    else
    {
        if (child->prev)
        {
            suffix_object(child->prev, item);
            array->child->prev = item;
        }
        else
        {
            while (child->next)
            {
                child = child->next;
            }
            suffix_object(child, item);
            array->child->prev = item;
        }
    }

    return 1;
}

uint8_t cJSON_AddItemToArray(cJSON* array, cJSON* item)
{
    return add_item_to_array(array, item);
}

static uint8_t add_item_to_object(cJSON* const object, const char* const string, cJSON* const item)
{
    char* new_key = NULL;
    jType new_type = jinvalid;

    if ((object == NULL) || (string == NULL) || (item == NULL) || (object == item))
    {
        return 0;
    }

    new_key = (char*)cJSON_strdup((const unsigned char*)string);
    if (new_key == NULL)
    {
        return 0;
    }

    new_type = item->type;

    if (item->string != NULL)
    {
        free(item->string);
    }

    item->string = new_key;
    item->type = new_type;

    return add_item_to_array(object, item);
}

uint8_t cJSON_AddItemToObject(cJSON* object, const char* string, cJSON* item)
{
    return add_item_to_object(object, string, item);
}

cJSON* cJSON_CreateNull(void)
{
    cJSON* item = cJSON_New_Item();
    if (item)
    {
        item->type = jnull;
    }

    return item;
}

cJSON* cJSON_CreateBool(uint8_t boolean)
{
    cJSON* item = cJSON_New_Item();
    if (item)
    {
        item->type = boolean ? jtrue : jfalse;
    }

    return item;
}

cJSON* cJSON_CreateNumber(void* num, jnType type)
{
    cJSON* item = cJSON_New_Item();
    if (item)
    {
        item->type = jnumber;
        item->typen = type;
        item->value = num;
    }

    return item;
}

cJSON* cJSON_CreateString(const char* string)
{
    cJSON* item = cJSON_New_Item();
    if (item)
    {
        item->type = jstring;
        item->value = (char*)cJSON_strdup((const unsigned char*)string);
        if (!item->value)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}

cJSON* cJSON_CreateRaw(const char* raw)
{
    cJSON* item = cJSON_New_Item();
    if (item)
    {
        item->type = jraw;
        item->value = (char*)cJSON_strdup((const unsigned char*)raw);
        if (!item->value)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}

cJSON* cJSON_CreateArray()
{
    cJSON* item = cJSON_New_Item();
    if (item)
    {
        item->type = jarray;
    }

    return item;
}

cJSON* cJSON_CreateObject()
{
    cJSON* item = cJSON_New_Item();
    if (item)
    {
        item->type = jobject;
    }

    return item;
}

#endif