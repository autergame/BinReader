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

#ifndef _cJSON_H_
#define _cJSON_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <float.h>
#include <inttypes.h>

enum jType
{
    jinvalid = 0,
    jfalse = 1,
    jtrue = 2, 
    jnull = 3,
    jnumber = 4,
    jstring = 6,
    jarray = 7,
    jobject = 8
};

enum jnType
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
};

struct cJSON
{
    cJSON* next;
    cJSON* prev;
    cJSON* child;

    jType type;
    jnType typen;
    void* value;
    char* string;
};

#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

#ifndef isinf
#define isinf(d) (isnan((d - d)) && !isnan(d))
#endif
#ifndef isnan
#define isnan(d) (d != d)
#endif

struct error
{
    const char* json;
    int position;
};
static error global_error = { nullptr, 0 };

const char* cJSON_GetErrorPtr()
{
    return global_error.json + global_error.position;
}

static char* cJSON_strdup(const char* string)
{
    if (string == nullptr)
        return nullptr;

    size_t length = strlen(string);
    char* copy = (char*)calloc(length + 1, 1);
    if (copy == nullptr)
        return nullptr;

    if (memcpy(copy, string, length) != copy)
        return nullptr;

    return copy;
}

static cJSON* cJSON_New_Item()
{
    cJSON* node = (cJSON*)calloc(1, sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON* item)
{
    cJSON* next = nullptr;
    while (item != nullptr)
    {
        next = item->next;
        free(item);
        item = next;
    }
}

struct parse_buffer
{
    const char* content;
    size_t length;
    size_t offset;
    int depth;
};

#define can_read(buffer, size) ((buffer != nullptr) && (((buffer)->offset + size) <= (buffer)->length))
#define can_access_at_index(buffer, index) ((buffer != nullptr) && (((buffer)->offset + index) < (buffer)->length))
#define buffer_at_offset(buffer) ((buffer)->content + (buffer)->offset)

static bool parse_number(cJSON* item, parse_buffer* input_buffer)
{
    char number_c_string[32] = { '\0' };
    bool floate = false, minus = false;

    if ((input_buffer == nullptr) || (input_buffer->content == nullptr))
        return false;

    for (int i = 0; (i < (sizeof(number_c_string) - 1)) && can_access_at_index(input_buffer, i); i++)
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
                minus = false;
                number_c_string[i] = '-';
                break;
            }
            case '.':
            {
                floate = true;
                number_c_string[i] = '.';
                break;
            }
            default:
                break;
        }
    }

    size_t length = strlen(number_c_string);
    if (length == 0)
        return false;

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
    return true;
}

struct printbuffer
{
    char* buffer;
    int length;
    size_t offset;
    int depth;
    bool format;
};

static char* ensure(printbuffer* p, int needed)
{
    if ((p == nullptr) || (p->buffer == nullptr))
        return nullptr;

    if ((p->length > 0) && (p->offset >= p->length))
        return nullptr;

    needed += p->offset + 1;
    if (needed <= p->length)
        return p->buffer + p->offset;

    int newsize = needed * 2;
    char* newbuffer = (char*)realloc(p->buffer, newsize);
    if (newbuffer == nullptr)
    {
        free(p->buffer);
        p->length = 0;
        p->buffer = nullptr;

        return nullptr;
    }

    p->length = newsize;
    p->buffer = newbuffer;

    return newbuffer + p->offset;
}

static void update_offset(printbuffer* buffer)
{
    if ((buffer == nullptr) || (buffer->buffer == nullptr))
        return;

    const char* buffer_pointer = buffer->buffer + buffer->offset;
    buffer->offset += strlen(buffer_pointer);
}

static bool print_number(const cJSON* item, printbuffer* output_buffer)
{
    if (output_buffer == nullptr)
        return false;

    if (item->typen == jFloat32)
    {
        int length = 0;
        char number_buffer[26] = { '\0' };

        float d = *(float*)item->value;
        if (isnan(d) || isinf(d))
            length = sprintf(number_buffer, "nullptr");
        else
            length = sprintf(number_buffer, "%.9g", d);

        bool havepoint = false;
        for (int i = 0; i < length; i++)
        {
            if (number_buffer[i] == '.')
                havepoint = true;
        }

        if (!havepoint)
            length = sprintf(number_buffer, "%.9g.0", d);

        if ((length < 0) || (length > (int)(sizeof(number_buffer) - 1)))
            return false;

        char* output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == nullptr)
            return false;

        for (int i = 0; i < length; i++)
            output_pointer[i] = number_buffer[i];

        output_pointer[length] = '\0';
        output_buffer->offset += length;
    }
    else
    {
        int length = 0;
        char number_buffer[26] = { '\0' };

        switch (item->typen)
        {
            case jSInt8:
                length = sprintf(number_buffer, "%" PRIi8, *(int8_t*)item->value);
                break;
            case jUInt8:
                length = sprintf(number_buffer, "%" PRIu8, *(uint8_t*)item->value);
                break;
            case jSInt16:
                length = sprintf(number_buffer, "%" PRIi16, *(int16_t*)item->value);
                break;
            case jUInt16:
                length = sprintf(number_buffer, "%" PRIu16, *(uint16_t*)item->value);
                break;
            case jSInt32:
                length = sprintf(number_buffer, "%" PRIi32, *(int32_t*)item->value);
                break;
            case jUInt32:
                length = sprintf(number_buffer, "%" PRIu32, *(uint32_t*)item->value);
                break;
            case jSInt64:
                length = sprintf(number_buffer, "%" PRIi64, *(int64_t*)item->value);
                break;
            case jUInt64:
                length = sprintf(number_buffer, "%" PRIu64, *(uint64_t*)item->value);
                break;
        }  

        char* output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == nullptr)
            return false;

        for (int i = 0; i < length; i++)
            output_pointer[i] = number_buffer[i];

        output_pointer[length] = '\0';
        output_buffer->offset += length;
    }

    return true;
}

static uint32_t parse_hex4(const char* input)
{
    uint32_t h = 0;

    for (int i = 0; i < 4; i++)
    {
        if ((input[i] >= '0') && (input[i] <= '9'))
        {
            h += (uint32_t)input[i] - '0';
        }
        else if ((input[i] >= 'A') && (input[i] <= 'F'))
        {
            h += (uint32_t)10 + input[i] - 'A';
        }
        else if ((input[i] >= 'a') && (input[i] <= 'f'))
        {
            h += (uint32_t)10 + input[i] - 'a';
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

static int utf16_literal_to_utf8(const char* input_pointer, const char* input_end, char** output_pointer)
{
    uint32_t codepoint = 0;
    uint32_t first_code = 0;
    const char* first_sequence = input_pointer;
    int utf8_length = 0, utf8_position = 0, sequence_length = 0, first_byte_mark = 0;

    if ((input_end - first_sequence) < 6)
        goto fail;

    first_code = parse_hex4(first_sequence + 2);

    if (((first_code >= 0xDC00) && (first_code <= 0xDFFF)))
        goto fail;

    if ((first_code >= 0xD800) && (first_code <= 0xDBFF))
    {
        const char* second_sequence = first_sequence + 6;
        uint32_t second_code = 0;
        sequence_length = 12;   

        if ((input_end - second_sequence) < 6)
            goto fail;

        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u'))
            goto fail;

        second_code = parse_hex4(second_sequence + 2);
        if ((second_code < 0xDC00) || (second_code > 0xDFFF))
            goto fail;

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

    for (utf8_position = (uint8_t)(utf8_length - 1); utf8_position > 0; utf8_position--)
    {
        (*output_pointer)[utf8_position] = (uint8_t)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    if (utf8_length > 1)
    {
        (*output_pointer)[0] = (uint8_t)((codepoint | first_byte_mark) & 0xFF);
    }
    else
    {
        (*output_pointer)[0] = (uint8_t)(codepoint & 0x7F);
    }

    *output_pointer += utf8_length;

    return sequence_length;

fail:
    return 0;
}

static bool parse_string(cJSON* item, parse_buffer* input_buffer)
{
    const char* input_pointer = buffer_at_offset(input_buffer) + 1;
    const char* input_end = buffer_at_offset(input_buffer) + 1;
    char* output_pointer = nullptr;
    char* output = nullptr;

    if (buffer_at_offset(input_buffer)[0] != '\"')
        goto fail;

    {
        int allocation_length = 0;
        int skipped_bytes = 0;
        while (((input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"'))
        {
            if (input_end[0] == '\\')
            {
                if ((input_end + 1 - input_buffer->content) >= input_buffer->length)
                    goto fail;

                skipped_bytes++;
                input_end++;
            }
            input_end++;
        }
        if (((input_end - input_buffer->content) >= input_buffer->length) || (*input_end != '\"'))
            goto fail;     

        allocation_length = (input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
        output = (char*)calloc(allocation_length + 1, 1);
        if (output == nullptr)
            goto fail;    
    }

    output_pointer = output;
    while (input_pointer < input_end)
    {
        if (*input_pointer != '\\') {
            *output_pointer++ = *input_pointer++;
        }
        else
        {
            int sequence_length = 2;
            if ((input_end - input_pointer) < 1)
                goto fail;

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
                        goto fail;
                    break;

                default:
                    goto fail;
            }
            input_pointer += sequence_length;
        }
    }

    *output_pointer = '\0';

    item->type = jstring;
    item->value = output;

    input_buffer->offset = (input_end - input_buffer->content);
    input_buffer->offset++;

    return true;

fail:
    if (output != nullptr)
        free(output);

    if (input_pointer != nullptr)
        input_buffer->offset = (input_pointer - input_buffer->content);

    return false;
}

static bool print_string_ptr(const char* input, printbuffer* output_buffer)
{
    const char* input_pointer = nullptr;
    char* output = nullptr;
    int escape_characters = 0;

    if (output_buffer == nullptr)
        return false;

    if (input == nullptr)
    {
        output = ensure(output_buffer, sizeof("\"\""));
        if (output == nullptr)
            return false;

        strcpy(output, "\"\"");
        return false;
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
                    escape_characters += 5;
                break;
        }
    }
    int output_length = (input_pointer - input) + escape_characters;

    output = ensure(output_buffer, output_length + sizeof("\"\""));
    if (output == nullptr)
        return false;

    if (escape_characters == 0)
    {
        output[0] = '\"';
        memcpy(output + 1, input, output_length);
        output[output_length + 1] = '\"';
        output[output_length + 2] = '\0';

        return true;
    }

    output[0] = '\"';
    char* output_pointer = output + 1;
    for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
    {
        if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\')) {
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
                    sprintf(output_pointer, "u%04x", *input_pointer);
                    output_pointer += 4;
                    break;
            }
        }
    }
    output[output_length + 1] = '\"';
    output[output_length + 2] = '\0';

    return true;
}

static int print_string(const cJSON* item, printbuffer* p)
{
    return print_string_ptr((char*)item->value, p);
}

static bool parse_value(cJSON* item, parse_buffer* input_buffer);
static bool print_value(const cJSON* item, printbuffer* output_buffer);

static bool parse_array(cJSON* item, parse_buffer* input_buffer);
static bool print_array(const cJSON* item, printbuffer* output_buffer);

static bool parse_object(cJSON* item, parse_buffer* input_buffer);
static bool print_object(const cJSON* item, printbuffer* output_buffer);

static parse_buffer* buffer_skip_whitespace(parse_buffer* buffer)
{
    if ((buffer == nullptr) || (buffer->content == nullptr))
        return nullptr;

    if (!can_access_at_index(buffer, 0))
        return buffer;

    while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] <= 32))
        buffer->offset++;

    if (buffer->offset == buffer->length)
        buffer->offset--;

    return buffer;
}

static parse_buffer* skip_utf8_bom(parse_buffer* buffer)
{
    if ((buffer == nullptr) || (buffer->content == nullptr) || (buffer->offset != 0))
        return nullptr;

    if (can_access_at_index(buffer, 4))     
    {
        if (strncmp(buffer_at_offset(buffer), "\xEF\xBB\xBF", 3) == 0)
            buffer->offset += 3;
    }

    return buffer;
}

cJSON* cJSON_ParseWithLengthOpts(const char* value, size_t buffer_length, const char** return_parse_end, int require_nullptr_terminated)
{
    parse_buffer buffer = { 0, 0, 0, 0 };
    cJSON* item = nullptr;

    global_error.json = nullptr;
    global_error.position = 0;

    if (value == nullptr || buffer_length == 0)
        goto fail;

    buffer.content = value;
    buffer.length = buffer_length;
    buffer.offset = 0;

    item = cJSON_New_Item();
    if (item == nullptr)    
        goto fail;

    if (!parse_value(item, buffer_skip_whitespace(skip_utf8_bom(&buffer))))
        goto fail;

    if (require_nullptr_terminated)
    {
        buffer_skip_whitespace(&buffer);
        if ((buffer.offset >= buffer.length) || buffer_at_offset(&buffer)[0] != '\0')
            goto fail;
    }
    if (return_parse_end)
        *return_parse_end = buffer_at_offset(&buffer);

    return item;

fail:
    if (item != nullptr)
        cJSON_Delete(item);

    if (value != nullptr)
    {
        error local_error;
        local_error.json = value;
        local_error.position = 0;

        if (buffer.offset < buffer.length) {
            local_error.position = (int)buffer.offset;
        }
        else if (buffer.length > 0) {
            local_error.position = (int)buffer.length - 1;
        }

        if (return_parse_end != nullptr)
            *return_parse_end = local_error.json + local_error.position;

        global_error = local_error;
    }

    return nullptr;
}

cJSON* cJSON_ParseWithOpts(const char* value, const char** return_parse_end, int require_nullptr_terminated)
{
    if (value == nullptr)
        return nullptr;

    size_t buffer_length = strlen(value) + 1;
    return cJSON_ParseWithLengthOpts(value, buffer_length, return_parse_end, require_nullptr_terminated);
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

char* cJSON_Print(const cJSON* item, bool format)
{
    char* printed = nullptr;

    printbuffer buffer = { nullptr, 0, 0, 0, false};
    buffer.buffer = (char*)calloc(256, 1);
    buffer.length = 256;
    buffer.format = format;
    if (buffer.buffer == nullptr)
        goto fail;

    if (!print_value(item, &buffer))
        goto fail;

    update_offset(&buffer);

    printed = (char*)realloc(buffer.buffer, buffer.offset + 1);
    if (printed == nullptr)
        goto fail;

    buffer.buffer = nullptr;

    return printed;

fail:
    if (buffer.buffer != nullptr)
        free(buffer.buffer);

    if (printed != nullptr)
        free(printed);

    return nullptr;
}

static bool parse_value(cJSON* item, parse_buffer* input_buffer)
{
    if ((input_buffer == nullptr) || (input_buffer->content == nullptr))
        return false;    

    uint32_t* number = (uint32_t*)calloc(1, 4);
    if (can_read(input_buffer, 4))
    {
        if (strncmp(buffer_at_offset(input_buffer), "null", 4) == 0)
        {
            item->type = jnull;
            input_buffer->offset += 4;
            return true;
        }
    }
    if (can_read(input_buffer, 5))
    {
        if (strncmp(buffer_at_offset(input_buffer), "false", 5) == 0)
        {
            *number = 0;
            item->type = jfalse;
            item->value = number;
            input_buffer->offset += 5;
            return true;
        }
    }
    if (can_read(input_buffer, 4))
    {
        if (strncmp(buffer_at_offset(input_buffer), "true", 4) == 0)
        {
            *number = 1;
            item->type = jtrue;
            item->value = number;
            input_buffer->offset += 4;
            return true;
        }
    }
    if (can_access_at_index(input_buffer, 0))
        if (buffer_at_offset(input_buffer)[0] == '\"')
            return parse_string(item, input_buffer);

    if (can_access_at_index(input_buffer, 0))
        if ((buffer_at_offset(input_buffer)[0] == '-') || ((buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(input_buffer)[0] <= '9')))
            return parse_number(item, input_buffer);

    if (can_access_at_index(input_buffer, 0))  
        if (buffer_at_offset(input_buffer)[0] == '[')
            return parse_array(item, input_buffer);

    if (can_access_at_index(input_buffer, 0))
        if (buffer_at_offset(input_buffer)[0] == '{')
            return parse_object(item, input_buffer);

    return false;
}

static bool print_value(const cJSON* item, printbuffer* output_buffer)
{
    if ((item == nullptr) || (output_buffer == nullptr))
        return false;

    char* output = nullptr;
    switch (item->type)
    {
        case jnull:
            output = ensure(output_buffer, 5);
            if (output == nullptr)
                return false;

            strcpy(output, "null");
            return true;

        case jfalse:
            output = ensure(output_buffer, 6);
            if (output == nullptr)
                return false;

            strcpy(output, "false");
            return true;

        case jtrue:
            output = ensure(output_buffer, 5);
            if (output == nullptr)
                return false;

            strcpy(output, "true");
            return true;

        case jnumber:
            return print_number(item, output_buffer);

        case jstring:
            return print_string(item, output_buffer);

        case jarray:
            return print_array(item, output_buffer);

        case jobject:
            return print_object(item, output_buffer);
    }

    return false;
}

static bool parse_array(cJSON* item, parse_buffer* input_buffer)
{
    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
        return false; 

    input_buffer->depth++;

    cJSON* head = nullptr;
    cJSON* current_item = nullptr;

    if (buffer_at_offset(input_buffer)[0] != '[')
        goto fail;

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0))
        if (buffer_at_offset(input_buffer)[0] == ']')
            goto success;

    if (!can_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    input_buffer->offset--;
    do
    {
        cJSON* new_item = cJSON_New_Item();
        if (new_item == nullptr)
            goto fail;    

        if (head == nullptr) {
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
            goto fail;      

        buffer_skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (!can_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != ']')
        goto fail;      

success:
    input_buffer->depth--;

    if (head != nullptr)
        head->prev = current_item;

    item->type = jarray;
    item->child = head;

    input_buffer->offset++;

    return true;

fail:
    if (head != nullptr)
        cJSON_Delete(head);

    return false;
}

static bool print_array(const cJSON* item, printbuffer* output_buffer)
{
    if (output_buffer == nullptr)
        return false;

    char* output_pointer = ensure(output_buffer, 1);
    if (output_pointer == nullptr)
        return false;

    *output_pointer = '[';
    output_buffer->offset++;
    output_buffer->depth++;

    cJSON* current_element = item->child;
    if (current_element != nullptr)
    {
        if (output_buffer->format && current_element->type != jobject)
        {
            output_pointer = ensure(output_buffer, output_buffer->depth + 1);
            if (output_pointer == nullptr)
                return false;

            *output_pointer++ = '\n';

            for (int i = 0; i < output_buffer->depth; i++)
                *output_pointer++ = '\t';

            output_buffer->offset += output_buffer->depth + 1;
        }
    }

    while (current_element != nullptr)
    {
        if (!print_value(current_element, output_buffer))
            return false;

        update_offset(output_buffer);
        if (current_element->next)
        {
            int length = 0;
            if (output_buffer->format)
            {
                if (current_element->type == jobject || current_element->type == jnumber)
                    length = 2;
                else
                    length = 2 + output_buffer->depth;
            }
            else {
                length = 1;
            }
            output_pointer = ensure(output_buffer, length);
            if (output_pointer == nullptr)
                return false;

            *output_pointer++ = ',';
            if (output_buffer->format)
            {
                if (current_element->type == jobject || current_element->type == jnumber) {
                    *output_pointer++ = ' ';
                }
                else
                {
                    *output_pointer++ = '\n';
                    for (int i = 0; i < output_buffer->depth; i++)
                        *output_pointer++ = '\t';
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
        if (output_pointer == nullptr)
            return false;

        *output_pointer++ = '\n';
        for (int i = 0; i < output_buffer->depth - 1; i++)
            *output_pointer++ = '\t';

        output_buffer->offset += output_buffer->depth;
    }

    output_pointer = ensure(output_buffer, 2);
    if (output_pointer == nullptr)
        return false;

    *output_pointer++ = ']';
    *output_pointer = '\0';
    output_buffer->depth--;

    return true;
}

static bool parse_object(cJSON* item, parse_buffer* input_buffer)
{
    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
        return false;     

    input_buffer->depth++;

    cJSON* head = nullptr;
    cJSON* current_item = nullptr;

    if (!can_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '{'))
        goto fail;     

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0))
        if (buffer_at_offset(input_buffer)[0] == '}')
            goto success;    

    if (!can_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    input_buffer->offset--;
    do
    {
        cJSON* new_item = cJSON_New_Item();
        if (new_item == nullptr)
            goto fail;    

        if (head == nullptr) {
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
            goto fail;      

        buffer_skip_whitespace(input_buffer);

        current_item->string = (char*)current_item->value;
        current_item->value = nullptr;

        if (!can_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':'))
            goto fail;    

        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer))
            goto fail;      

        buffer_skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (!can_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '}'))
        goto fail;      

success:
    input_buffer->depth--;

    if (head != nullptr)
        head->prev = current_item;

    item->type = jobject;
    item->child = head;

    input_buffer->offset++;
    return true;

fail:
    if (head != nullptr)
        cJSON_Delete(head);

    return false;
}

static bool print_object(const cJSON* item, printbuffer* output_buffer)
{
    if (output_buffer == nullptr)
        return false;

    if (output_buffer->format)
    {
        int length = (output_buffer->depth ? output_buffer->depth + 1 : 0) + 2;       
        char* output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == nullptr)
            return false;

        if (output_buffer->depth)
            *output_pointer++ = '\n';

        for (int i = 0; i < output_buffer->depth; i++)
            *output_pointer++ = '\t';

        *output_pointer++ = '{';
        *output_pointer++ = '\n';

        output_buffer->depth++;
        output_buffer->offset += length;

    }
    else
    {
        int length = 1;    
        char* output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == nullptr)
            return false;

        *output_pointer++ = '{';

        output_buffer->depth++;
        output_buffer->offset += length;
    }

    cJSON* current_item = item->child;
    while (current_item)
    {
        if (output_buffer->format)
        {
            char* output_pointer = ensure(output_buffer, output_buffer->depth);
            if (output_pointer == nullptr)
                return false;

            for (int i = 0; i < output_buffer->depth; i++)
                *output_pointer++ = '\t';

            output_buffer->offset += output_buffer->depth;
        }

        if (!print_string_ptr(current_item->string, output_buffer))
            return false;

        update_offset(output_buffer);

        int length = (output_buffer->format ? 2 : 1);
        char* output_pointer = ensure(output_buffer, length);
        if (output_pointer == nullptr)
            return false;

        *output_pointer++ = ':';
        if (output_buffer->format)
            *output_pointer++ = ' ';

        output_buffer->offset += length;

        if (!print_value(current_item, output_buffer))
            return false;

        update_offset(output_buffer);

        length = ((output_buffer->format ? 1 : 0) + (current_item->next ? 1 : 0));
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == nullptr)
            return false;

        if (current_item->next)
            *output_pointer++ = ',';

        if (output_buffer->format)
            *output_pointer++ = '\n';

        *output_pointer = '\0';
        output_buffer->offset += length;

        current_item = current_item->next;
    }

    char* output_pointer = ensure(output_buffer, output_buffer->format ? (output_buffer->depth + 1) : 2);
    if (output_pointer == nullptr)
        return false;

    if (output_buffer->format)
    {
        for (int i = 0; i < (output_buffer->depth - 1); i++)
            *output_pointer++ = '\t';
    }
    *output_pointer++ = '}';
    *output_pointer = '\0';
    output_buffer->depth--;

    return true;
}

uint32_t cJSON_GetArraySize(const cJSON* array)
{
    if (array == nullptr)
        return 0;

    uint32_t size = 0;
    cJSON* child = array->child;
    while (child != nullptr)
    {
        size++;
        child = child->next;
    }

    return (int)size;
}

static cJSON* get_array_item(const cJSON* array, int index)
{
    if (array == nullptr)
        return nullptr;

    cJSON* current_child = array->child;
    while ((current_child != nullptr) && (index > 0))
    {
        index--;
        current_child = current_child->next;
    }

    return current_child;
}

cJSON* cJSON_GetArrayItem(const cJSON* array, int index)
{
    if (index < 0)
        return nullptr;

    return get_array_item(array, index);
}

static cJSON* cJSON_GetObjectItem(const cJSON* object, const char* name)
{
    if ((object == nullptr) || (name == nullptr))
        return nullptr;

    cJSON* current_element = object->child;
    while ((current_element != nullptr) && (current_element->string != nullptr) && strcmp(name, current_element->string) != 0)
        current_element = current_element->next;

    if ((current_element == nullptr) || (current_element->string == nullptr))
        return nullptr;

    return current_element;
}

bool cJSON_HasObjectItem(const cJSON* object, const char* string)
{
    return cJSON_GetObjectItem(object, string) ? true : false;
}

static void suffix_object(cJSON* prev, cJSON* item)
{
    prev->next = item;
    item->prev = prev;
}

static bool add_item_to_array(cJSON* array, cJSON* item)
{
    if ((item == nullptr) || (array == nullptr) || (array == item))
        return false;

    cJSON* child = array->child;
    if (child == nullptr)
    {
        array->child = item;
        item->prev = item;
        item->next = nullptr;
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
                child = child->next;

            suffix_object(child, item);
            array->child->prev = item;
        }
    }

    return true;
}

int cJSON_AddItemToArray(cJSON* array, cJSON* item)
{
    return add_item_to_array(array, item);
}

static bool add_item_to_object(cJSON* object, const char* string, cJSON* item)
{
    if ((object == nullptr) || (string == nullptr) || (item == nullptr) || (object == item))
        return false;

    char* new_key = cJSON_strdup(string);
    if (new_key == nullptr)
        return false;

    if (item->string != nullptr)
        free(item->string);

    item->string = new_key;
    item->type = item->type;

    return add_item_to_array(object, item);
}

int cJSON_AddItemToObject(cJSON* object, const char* string, cJSON* item)
{
    return add_item_to_object(object, string, item);
}

cJSON* cJSON_CreateNull(void)
{
    cJSON* item = cJSON_New_Item();
    if (item)
        item->type = jnull;
    return item;
}

cJSON* cJSON_CreateBool(bool boolean)
{
    cJSON* item = cJSON_New_Item();
    if (item)
        item->type = boolean ? jtrue : jfalse;
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
        item->value = cJSON_strdup(string);
        if (!item->value)
        {
            cJSON_Delete(item);
            return nullptr;
        }
    }
    return item;
}

cJSON* cJSON_CreateArray()
{
    cJSON* item = cJSON_New_Item();
    if (item)
        item->type = jarray;
    return item;
}

cJSON* cJSON_CreateObject()
{
    cJSON* item = cJSON_New_Item();
    if (item)
        item->type = jobject;
    return item;
}

#endif //_cJSON_H_