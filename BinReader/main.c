//author https://github.com/autergame
#define _CRT_SECURE_NO_WARNINGS
#include "cJSON.h"

uint32_t FNV1Hash(char* str)
{
    size_t Hash = 0x811c9dc5;
    for (size_t i = 0; i < strlen(str); i++)
        Hash = (Hash ^ tolower(str[i])) * 0x01000193;
    return Hash;
}

struct node
{
    uint32_t key;
    char* value;
    struct node* next;
};

typedef struct HashTable
{
    uint32_t size;
    struct node** list;
} HashTable;

HashTable* createHashTable(uint32_t size)
{
    HashTable* t = (HashTable*)malloc(sizeof(HashTable));
    t->size = size;
    t->list = (struct node**)calloc(size, sizeof(struct node*));
    return t;
}

void insertHashTable(HashTable* t, uint32_t key, char* val)
{
    uint32_t pos = key % t->size;
    struct node* list = t->list[pos];
    struct node* newNode = (struct node*)malloc(sizeof(struct node));
    struct node* temp = list;
    while (temp) {
        if (temp->key == key) {
            temp->value = val;
            return;
        }
        temp = temp->next;
    }
    newNode->key = key;
    newNode->value = val;
    newNode->next = list;
    t->list[pos] = newNode;
}

char* lookupHashTable(HashTable* t, uint32_t key)
{
    struct node* list = t->list[key % t->size];
    struct node* temp = list;
    while (temp) {
        if (temp->key == key) {
            return temp->value;
        }
        temp = temp->next;
    }
    return NULL;
}

int addhash(HashTable* map, char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file)
    {
        printf("ERROR: cannot read file \"%s\".\n", filename);
        scanf("press enter to exit.");
        return 1;
    }
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* fp = (char*)malloc(fsize + 1);
    fread(fp, fsize, 1, file);
    fp[fsize] = '\0';
    fclose(file);
    char* hashend;
    char* hashline = strtok(fp, "\n");
    while (hashline != NULL) {   
        uint32_t key = strtoul(hashline, &hashend, 16);
        insertHashTable(map, key, hashend+1);
        hashline = strtok(NULL, "\n");
    }
    return 0;
}

typedef struct charv
{
    char* data;
    size_t lenght;
} charv;

void memfwrite(char* buf, size_t bytes, charv* membuf)
{
    char* oblock = (char*)realloc(membuf->data, membuf->lenght + bytes);
    oblock += membuf->lenght;
    memcpy(oblock, buf, bytes);
    membuf->data = oblock;
    membuf->data -= membuf->lenght;
    membuf->lenght += bytes;
}

void memfread(void* buf, size_t bytes, char** membuf)
{
    memcpy(buf, *membuf, bytes);
    *membuf += bytes;
}

typedef enum Type
{
    NONE = 0, BOOL = 1, 
    SInt8 = 2, UInt8 = 3,
    SInt16 = 4, UInt16 = 5,
    SInt32 = 6, UInt32 = 7,
    SInt64 = 8, UInt64 = 9,
    Float32 = 10, VEC2 = 11,
    VEC3 = 12, VEC4 = 13, 
    MTX44 = 14, RGBA = 15,
    STRING = 16, HASH = 17, 
    CONTAINER = 18, STRUCT = 19,
    POINTER = 20, EMBEDDED = 21,
    LINK = 22, OPTION = 23,
    MAP = 24, FLAG = 25
} Type;

static const char* Type_strings[] = {
    "None", "Bool", "SInt8", "UInt8", "SInt16", "UInt16", "SInt32", "UInt32", "SInt64",
    "UInt64", "Float32", "Vector2", "Vector3", "Vector4", "Matrix4x4", "RGBA", "String", 
    "Hash", "Container", "Struct", "Pointer", "Embedded", "Link", "Option", "Map", "Flag"
};

Type uinttotype(uint8_t type)
{
    if (type & 0x80) {
        type &= 0x7F;
        type += 18;
    }
    return (Type)type;
}

uint8_t typetouint(Type type)
{
    uint8_t raw = type;
    if (raw >= 18) {
        raw -= 18;
        raw |= 0x80;
    }
    return raw;
}

Type findtypebystring(char* sval)
{
    Type result = NONE;
    for (int i = 0; Type_strings[i] != NULL; i++, result++)
        if (strcmp(sval, Type_strings[i]) == 0) 
            return result;
    return -1;
}

typedef struct BinField 
{
    Type typebin;
    void* data;
} BinField;

typedef struct Pair 
{
    BinField* key;
    BinField* value;
} Pair;

typedef struct Field 
{
    uint32_t key;
    BinField* value;
} Field;

typedef struct ContainerOrStruct
{
    Type valueType;
    BinField** items;
    uint32_t itemsize;
} ContainerOrStruct;

typedef struct PointerOrEmbed 
{
    uint32_t name;
    Field** items;
    uint16_t itemsize;
} PointerOrEmbed;

typedef struct Option 
{
    uint8_t count;
    Type valueType;
    BinField** items;
} Option;

typedef struct Map
{    
    Type keyType;
    Type valueType;
    Pair** items;
    uint32_t itemsize;
} Map;

char* hashtostr(HashTable* hasht, uint32_t value)
{
    char* strvalue = lookupHashTable(hasht, value);
    if (strvalue == NULL)
    {
        strvalue = (char*)calloc(10, 1);
        sprintf(strvalue, "0x%08X", value);
    }
    return strvalue;
}

uint32_t hashfromstring(char* string)
{
    uint32_t hash = 0;
    if (string[0] == '0' && string[1] == 'x')
        sscanf(string, "0x%08X", &hash);
    else
        hash = FNV1Hash(string);
    return hash;
}

cJSON* getvaluefromtype(BinField* value, HashTable* hasht, cJSON* json, const char* strdata)
{
    switch (value->typebin)
    {
        case NONE:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNull());
            break;
        case FLAG:
        case BOOL:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateBool((uint8_t)value->data));
            break;
        case SInt8:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data, jSInt8));
            break;
        case UInt8:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data, jUInt8));
            break;
        case SInt16:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data, jSInt16));
            break;
        case UInt16:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data, jUInt16));
            break;
        case SInt32:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data, jSInt32));
            break;
        case UInt32:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data, jUInt32));
            break;
        case SInt64:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data, jSInt64));
            break;
        case UInt64:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data, jUInt64));
            break;
        case Float32:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data, jFloat32));
            break;
        case VEC2:
        {
            float* arr = (float*)value->data;
            cJSON* jsonarr = cJSON_CreateArray();
            for (int i = 0; i < 2; i++)
                cJSON_AddItemToArray(jsonarr, cJSON_CreateNumber(&arr[i], jFloat32));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            break;
        }
        case VEC3:
        {
            float* arr = (float*)value->data;
            cJSON* jsonarr = cJSON_CreateArray();
            for (int i = 0; i < 3; i++)
                cJSON_AddItemToArray(jsonarr, cJSON_CreateNumber(&arr[i], jFloat32));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            break;
        }
        case VEC4:
        {
            float* arr = (float*)value->data;
            cJSON* jsonarr = cJSON_CreateArray();
            for (int i = 0; i < 4; i++)
                cJSON_AddItemToArray(jsonarr, cJSON_CreateNumber(&arr[i], jFloat32));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            break;
        }
        case MTX44:
        {
            float* arr = (float*)value->data;
            cJSON* jsonarr = cJSON_CreateArray();
            for (int i = 0; i < 16; i++)
                cJSON_AddItemToArray(jsonarr, cJSON_CreateNumber(&arr[i], jFloat32));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            break;
        }
        case RGBA:
        {
            uint8_t* arr = (uint8_t*)value->data;
            cJSON* jsonarr = cJSON_CreateArray();
            for (int i = 0; i < 4; i++)
                cJSON_AddItemToArray(jsonarr, cJSON_CreateNumber(&arr[i], jUInt8));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            break;
        }
        case STRING:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateString((char*)value->data));
            break;
        case HASH:
        case LINK:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateString(hashtostr(hasht, (uint32_t)value->data)));
            break;
        case CONTAINER:
        case STRUCT:
        {
            cJSON* jsonarr = cJSON_CreateArray();
            ContainerOrStruct* cs = (ContainerOrStruct*)value->data;
            if (cs->valueType >= 18 && cs->valueType <= 21 || cs->valueType == MAP)
                jsonarr = cJSON_CreateObject();
            cJSON_AddItemToObject(json, "containertype", cJSON_CreateString(Type_strings[cs->valueType]));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            for (uint32_t i = 0; i < cs->itemsize; i++)
                getvaluefromtype(cs->items[i], hasht, jsonarr, "data");
            break;
        }
        case POINTER:
        case EMBEDDED:
        {
            cJSON* jsonmp = cJSON_CreateObject();
            cJSON* jsonarr = cJSON_CreateArray();
            PointerOrEmbed* pe = (PointerOrEmbed*)value->data;
            if (strcmp(strdata, "keydata") == 0 || strcmp(strdata, "valuedata") == 0)
            {
                cJSON_AddItemToObject(json, strdata, jsonmp);
                cJSON_AddItemToObject(jsonmp, hashtostr(hasht, pe->name), jsonarr);
            }
            else
                cJSON_AddItemToObject(json, hashtostr(hasht, pe->name), jsonarr);
            for (uint16_t i = 0; i < pe->itemsize; i++)
            {
                cJSON* jsonobj = cJSON_CreateObject();
                cJSON_AddItemToObject(jsonobj, "name", cJSON_CreateString(hashtostr(hasht, pe->items[i]->key)));
                cJSON_AddItemToObject(jsonobj, "type", cJSON_CreateString(Type_strings[pe->items[i]->value->typebin]));
                getvaluefromtype(pe->items[i]->value, hasht, jsonobj, "data");
                cJSON_AddItemToArray(jsonarr, jsonobj);
            }
            break;
        }
        case OPTION:
        {
            Option* op = (Option*)value->data;
            cJSON* jsonarr = cJSON_CreateArray();
            cJSON_AddItemToObject(json, "optiontype", cJSON_CreateString(Type_strings[op->valueType]));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            for (uint8_t i = 0; i < op->count; i++)
                getvaluefromtype(op->items[i], hasht, jsonarr, "data");
            break;
        }
        case MAP:
        {
            Map* mp = (Map*)value->data;
            cJSON* jsonarr = cJSON_CreateArray();
            cJSON_AddItemToObject(json, "keytype", cJSON_CreateString(Type_strings[mp->keyType]));
            cJSON_AddItemToObject(json, "valuetype", cJSON_CreateString(Type_strings[mp->valueType]));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            for (uint32_t i = 0; i < mp->itemsize; i++)
            {
                cJSON* jsonobj = cJSON_CreateObject();
                getvaluefromtype(mp->items[i]->key, hasht, jsonobj, "keydata");
                getvaluefromtype(mp->items[i]->value, hasht, jsonobj, "valuedata");
                cJSON_AddItemToArray(jsonarr, jsonobj);
            }
            break;
        }
    }
    return json;
}

BinField* getvaluefromjson(Type typebin, cJSON* json, uint8_t getobject)
{
    int i = 0;
    cJSON* obj;
    cJSON* jdata = getobject ? cJSON_GetObjectItem(json, "data") : json;
    BinField* result = (BinField*)calloc(1, sizeof(BinField));
    result->typebin = typebin;
    switch (typebin)
    {
        case NONE:
        case FLAG:
        case BOOL:
        case SInt8:
        case UInt8:
        case SInt16:
        case UInt16:
        case SInt32:
        case UInt32:
        case SInt64:
        case UInt64:
        case Float32:
        case STRING:
            result->data = jdata->value;
            break;
        case VEC2:
        {
            float* data = (float*)calloc(2, 4);
            for (i = 0, obj = jdata->child; obj != NULL; obj = obj->next, i++)
                data[i] = *(float*)obj->value;
            result->data = data;
            break;
        }
        case VEC3:
        {
            float* data = (float*)calloc(3, 4);
            for (i = 0, obj = jdata->child; obj != NULL; obj = obj->next, i++)
                data[i] = *(float*)obj->value;
            result->data = data;
            break;
        }
        case VEC4:
        {
            float* data = (float*)calloc(4, 4);
            for (i = 0, obj = jdata->child; obj != NULL; obj = obj->next, i++)
                data[i] = *(float*)obj->value;
            result->data = data;
            break;
        }
        case MTX44:
        {
            float* data = (float*)calloc(16, 4);
            for (i = 0, obj = jdata->child; obj != NULL; obj = obj->next, i++)
                data[i] = *(float*)obj->value;
            result->data = data;
            break;
        }
        case RGBA:
        {
            uint8_t* data = (uint8_t*)calloc(4, 1);
            for (i = 0, obj = jdata->child; obj != NULL; obj = obj->next, i++)
                data[i] = *(uint8_t*)obj->value;
            result->data = data;
            break;
        }
        case HASH:
        case LINK:
        {
            uint32_t* data = (uint32_t*)calloc(1, 4);
            *data = hashfromstring((char*)jdata->value);
            result->data = data;
            break;
        }
        case CONTAINER:
        case STRUCT:
        {
            cJSON* cs = cJSON_GetObjectItem(json, "data");
            ContainerOrStruct* tmpcs = (ContainerOrStruct*)calloc(1, sizeof(ContainerOrStruct));
            tmpcs->valueType = findtypebystring((char*)cJSON_GetObjectItem(json, "containertype")->value);
            tmpcs->itemsize = cJSON_GetArraySize(cs);
            tmpcs->items = (BinField**)calloc(tmpcs->itemsize, sizeof(BinField*));
            for (i = 0, obj = cs->child; obj != NULL; obj = obj->next, i++)
                tmpcs->items[i] = getvaluefromjson(tmpcs->valueType, obj, 0);
            result->data = tmpcs;
            break;
        }
        case POINTER:
        case EMBEDDED:
        {
            PointerOrEmbed* tmppe = (PointerOrEmbed*)calloc(1, sizeof(PointerOrEmbed));
            cJSON* pe = getobject ? json->child->next->next : json;
            tmppe->name = hashfromstring(pe->string);
            if (tmppe->name == 0)
            {
                result->data = tmppe;
                break;
            }
            tmppe->itemsize = (uint16_t)cJSON_GetArraySize(pe);
            tmppe->items = (Field**)calloc(tmppe->itemsize, sizeof(Field*));
            for (i = 0, obj = pe->child; obj != NULL; obj = obj->next, i++)
            {
                Field* tmpfield = (Field*)calloc(1, sizeof(Field));
                tmpfield->key = hashfromstring((char*)cJSON_GetObjectItem(obj, "name")->value);
                tmpfield->value = getvaluefromjson(findtypebystring((char*)cJSON_GetObjectItem(obj, "type")->value), obj, 1);
                tmppe->items[i] = tmpfield;
            }
            result->data = tmppe;
            break;
        }
        case OPTION:
        {
            cJSON* op = cJSON_GetObjectItem(json, "data");
            Option* tmpo = (Option*)calloc(1, sizeof(Option));
            tmpo->valueType = findtypebystring((char*)cJSON_GetObjectItem(json, "optiontype")->value);
            tmpo->count = (uint8_t)cJSON_GetArraySize(op);
            tmpo->items = (BinField**)calloc(tmpo->count, sizeof(BinField*));
            for (i = 0, obj = op->child; obj != NULL; obj = obj->next, i++)
                tmpo->items[i] = getvaluefromjson(tmpo->valueType, obj, 0);
            result->data = tmpo;
            break;
        }
        case MAP:
        {
            cJSON* key = cJSON_CreateObject();
            cJSON* value = cJSON_CreateObject();
            cJSON* mp = cJSON_GetObjectItem(json, "data");
            Map* tmpmap = (Map*)calloc(1, sizeof(Map));
            tmpmap->itemsize = cJSON_GetArraySize(mp);
            tmpmap->keyType = findtypebystring((char*)cJSON_GetObjectItem(json, "keytype")->value);
            tmpmap->valueType = findtypebystring((char*)cJSON_GetObjectItem(json, "valuetype")->value);
            tmpmap->items = (Pair**)calloc(tmpmap->itemsize, sizeof(Pair*));
            for (i = 0, obj = mp->child; obj != NULL; obj = obj->next, i++)
            {
                Pair* pairtmp = (Pair*)calloc(1, sizeof(Pair));
                key = cJSON_GetObjectItem(obj, "keydata");
                value = cJSON_GetObjectItem(obj, "valuedata");
                if (tmpmap->keyType >= 18 && tmpmap->keyType <= 21 || tmpmap->keyType == MAP)
                    key = key->child;
                if (tmpmap->valueType >= 18 && tmpmap->valueType <= 21 || tmpmap->valueType == MAP)
                    value = value->child;
                pairtmp->key = getvaluefromjson(tmpmap->keyType, key, 0);
                pairtmp->value = getvaluefromjson(tmpmap->valueType, value, 0);
                tmpmap->items[i] = pairtmp;
            }
            result->data = tmpmap;
            break;
        }
    }
    return result;
}

BinField* readvaluebytype(uint8_t typeidbin, HashTable* hasht, char** fp)
{
    BinField* result = (BinField*)calloc(1, sizeof(BinField));
    result->typebin = uinttotype(typeidbin);
    switch (result->typebin)
    {
        case FLAG:
        case BOOL:
        case SInt8:
        case UInt8:
            memfread(&result->data, 1, fp);
            break;
        case SInt16:
        case UInt16:
            memfread(&result->data, 2, fp);
            break;
        case LINK:
        case HASH:
        case SInt32:
        case UInt32:
        case Float32:
            memfread(&result->data, 4, fp);
            break;
        case SInt64:
        case UInt64:
            memfread(&result->data, 8, fp);
            break;
        case VEC2:
        {
            float* data = (float*)calloc(2, 4);
            memfread(data, 4 * 2, fp);
            result->data = data;
            break;
        }
        case VEC3:
        {
            float* data = (float*)calloc(3, 4);
            memfread(data, 4 * 3, fp);
            result->data = data;
            break;
        }       
        case VEC4:
        {
            float* data = (float*)calloc(4, 4);
            memfread(data, 4 * 4, fp);
            result->data = data;
            break;
        }
        case MTX44:
        {
            float* data = (float*)calloc(16, 4);
            memfread(data, 4 * 16, fp);
            result->data = data;
            break;
        }
        case RGBA:
        {
            uint8_t* data = (uint8_t*)calloc(4, 1);
            memfread(data, 4, fp);
            result->data = data;
            break;
        }
        case STRING:
        {
            uint16_t stringlength = 0;
            memfread(&stringlength, 2, fp);
            char* stringb = (char*)calloc(stringlength + 1, 1);
            memfread(stringb, (size_t)stringlength, fp);
            stringb[stringlength] = '\0';
            result->data = stringb;
            insertHashTable(hasht, FNV1Hash(stringb), stringb);
            break;
        }       
        case STRUCT:
        case CONTAINER:
        {
            uint8_t type = 0;
            uint32_t size = 0;
            uint32_t count = 0;
            ContainerOrStruct* tmpcs = (ContainerOrStruct*)calloc(1, sizeof(ContainerOrStruct));
            memfread(&type, 1, fp);
            memfread(&size, 4, fp);
            memfread(&count, 4, fp);
            tmpcs->itemsize = count;
            tmpcs->valueType = uinttotype(type);
            tmpcs->items = (BinField**)calloc(count, sizeof(BinField*));
            for (uint32_t i = 0; i < count; i++)
                tmpcs->items[i] = readvaluebytype(tmpcs->valueType, hasht, fp);
            result->data = tmpcs;
            break;
        }
        case POINTER:
        case EMBEDDED:
        {
            uint32_t size = 0;
            uint16_t count = 0;
            PointerOrEmbed* tmppe = (PointerOrEmbed*)calloc(1, sizeof(PointerOrEmbed));
            memfread(&tmppe->name, 4, fp);
            if (tmppe->name == 0)
            {
                result->data = tmppe;
                break;
            }
            memfread(&size, 4, fp);
            memfread(&count, 2, fp);
            tmppe->itemsize = count;
            tmppe->items = (Field**)calloc(count, sizeof(Field*));
            for (uint16_t i = 0; i < count; i++)
            {
                uint8_t type = 0;
                Field* tmpfield = (Field*)calloc(1, sizeof(Field));
                memfread(&tmpfield->key, 4, fp);
                memfread(&type, 1, fp);
                tmpfield->value = readvaluebytype(type, hasht, fp);
                tmppe->items[i] = tmpfield;
            }
            result->data = tmppe;
            break;  
        }
        case OPTION:
        {
            uint8_t type = 0;
            uint8_t count = 0;
            Option* tmpo = (Option*)calloc(1, sizeof(Option));
            memfread(&type, 1, fp);
            memfread(&count, 1, fp);
            tmpo->count = count;
            tmpo->valueType = uinttotype(type);
            tmpo->items = (BinField**)calloc(count, sizeof(BinField*));
            for (uint8_t i = 0; i < count; i++)
                tmpo->items[i] = readvaluebytype(tmpo->valueType, hasht, fp);
            result->data = tmpo;
            break;
        }
        case MAP:
        {
            uint32_t size = 0;
            uint8_t typek = 0;
            uint8_t typev = 0;
            uint32_t count = 0;
            Map* tmpmap = (Map*)calloc(1, sizeof(Map));
            memfread(&typek, 1, fp);
            memfread(&typev, 1, fp);
            memfread(&size, 4, fp);
            memfread(&count, 4, fp);
            tmpmap->itemsize = count;
            tmpmap->keyType = uinttotype(typek);
            tmpmap->valueType = uinttotype(typev);
            tmpmap->items = (Pair**)calloc(count, sizeof(Pair*));
            for (uint32_t i = 0; i < count; i++)
            {
                Pair* pairtmp = (Pair*)calloc(1, sizeof(Pair));
                pairtmp->key = readvaluebytype(tmpmap->keyType, hasht, fp);
                pairtmp->value = readvaluebytype(tmpmap->valueType, hasht, fp);
                tmpmap->items[i] = pairtmp;
            }
            result->data = tmpmap;
            break;
        }
    }
    return result;
}

uint32_t getsize(BinField* value)
{
    uint32_t size = 0;
    switch (value->typebin)
    {
        case FLAG:
        case BOOL:
        case SInt8:
        case UInt8:
            size = 1;
            break;
        case SInt16:
        case UInt16:
            size = 2;
            break;
        case LINK:
        case HASH:
        case RGBA:
        case SInt32:
        case UInt32:
        case Float32:
            size = 4;
            break;
        case VEC2:
        case SInt64:
        case UInt64:
            size = 8;
            break;
        case VEC3:
            size = 12;
            break;
        case VEC4:
            size = 16;
            break;
        case MTX44:
            size = 64;
            break;
        case STRING:
            size = 2 + strlen((char*)value->data);
            break;
        case STRUCT:
        case CONTAINER:
        {
            size = 1 + 4 + 4;
            ContainerOrStruct* cs = (ContainerOrStruct*)value->data;
            for (uint32_t i = 0; i < cs->itemsize; i++)
                size += getsize(cs->items[i]);
            break;
        }
        case POINTER:
        case EMBEDDED:
        {
            size = 4;
            PointerOrEmbed* pe = (PointerOrEmbed*)value->data;
            if (pe->name != 0)
            { 
                size += 4 + 2;
                for (uint16_t i = 0; i < pe->itemsize; i++)
                  size += getsize(pe->items[i]->value) + 4 + 1;
            }
            break;
        }
        case OPTION:
        {
            size = 2;
            Option* op = (Option*)value->data;
            for (uint8_t i = 0; i < op->count; i++)
                size += getsize(op->items[i]);
            break;
        }
        case MAP:
        {
            size = 1 + 1 + 4 + 4;
            Map* map = (Map*)value->data;
            for (uint32_t i = 0; i < map->itemsize; i++)
                size += getsize(map->items[i]->key) + getsize(map->items[i]->value);
            break;
        }
    }
    return size;
}

void writevaluebybin(BinField* value, charv* str)
{
    switch (value->typebin)
    {
        case FLAG:
        case BOOL:
        case SInt8:
        case UInt8:
            memfwrite(value->data, 1, str);
            break;
        case SInt16:
        case UInt16:
            memfwrite(value->data, 2, str);
            break;
        case LINK:
        case HASH:
        case RGBA:
        case SInt32:
        case UInt32:
        case Float32:
            memfwrite(value->data, 4, str);
            break;
        case VEC2:
        case SInt64:
        case UInt64:
            memfwrite(value->data, 8, str);
            break;
        case VEC3:
            memfwrite(value->data, 12, str);
            break;
        case VEC4:
            memfwrite(value->data, 16, str);
            break;
        case MTX44:
            memfwrite(value->data, 64, str);
            break;
        case STRING:
        {
            char* string = (char*)value->data;
            uint16_t size = (uint16_t)strlen(string);
            memfwrite((char*)&size, 2, str);
            memfwrite(string, size, str);
            break;
        }
        case STRUCT:
        case CONTAINER:
        {
            uint32_t size = 4;
            ContainerOrStruct* cs = (ContainerOrStruct*)value->data;
            uint8_t type = typetouint(cs->valueType);
            memfwrite(&type, 1, str);
            for (uint16_t k = 0; k < cs->itemsize; k++)
                size += getsize(cs->items[k]);
            memfwrite((char*)&size, 4, str);
            memfwrite((char*)&cs->itemsize, 4, str);
            for (uint32_t i = 0; i < cs->itemsize; i++)
                writevaluebybin(cs->items[i], str);
            break;
        }
        case POINTER:
        case EMBEDDED:
        {
            uint32_t size = 2;
            PointerOrEmbed* pe = (PointerOrEmbed*)value->data;
            memfwrite((char*)&pe->name, 4, str);
            if (pe->name == 0)
                break;
            for (uint16_t k = 0; k < pe->itemsize; k++)
                size += getsize(pe->items[k]->value) + 4 + 1;
            memfwrite((char*)&size, 4, str);
            memfwrite((char*)&pe->itemsize, 2, str);
            for (uint16_t i = 0; i < pe->itemsize; i++)
            {
                uint8_t type = typetouint(pe->items[i]->value->typebin);
                memfwrite((char*)&pe->items[i]->key, 4, str);
                memfwrite((char*)&type, 1, str);
                writevaluebybin(pe->items[i]->value, str);
            }
            break;
        }
        case OPTION:
        {
            uint8_t count = 1;
            Option* op = (Option*)value->data;
            uint8_t type = typetouint(op->valueType);
            memfwrite(&type, 1, str);
            memfwrite(&op->count, 1, str);
            for(uint8_t i = 0; i < op->count; i++)
                writevaluebybin(op->items[i], str);
            break;
        }
        case MAP:
        {
            uint32_t size = 4;
            Map* map = (Map*)value->data;
            uint8_t typek = typetouint(map->keyType);
            uint8_t typev = typetouint(map->valueType);
            memfwrite((char*)&typek, 1, str);
            memfwrite((char*)&typev, 1, str);
            for (uint16_t k = 0; k < map->itemsize; k++)
                size += getsize(map->items[k]->key) + getsize(map->items[k]->value);
            memfwrite((char*)&size, 4, str);
            memfwrite((char*)&map->itemsize, 4, str);
            for (uint32_t i = 0; i < map->itemsize; i++)
            {
                writevaluebybin(map->items[i]->key, str);
                writevaluebybin(map->items[i]->value, str);
            }
            break;
        }
    }
}

void strip_ext(char* fname)
{
    char* end = fname + strlen(fname);
    while (end > fname && *end != '.') {
        --end;
    }
    if (end > fname) {
        *end = '\0';
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("Usage: binreader -d file.bin\n");
        printf("Usage: binreader -e file.json\n");
        scanf("press enter to exit.");
        return 1;
    }
    if (strcmp(argv[1], "-d") == 0)
    {
        FILE* file = fopen(argv[2], "rb");
        if (!file)
        {
            printf("ERROR: cannot read file \"%s\".\n", argv[2]);
            scanf("press enter to exit.");
            return 1;
        }
        char* name = (char*)calloc(strlen(argv[2]), 1);
        strcat(name, argv[2]);
        strip_ext(name);
        strcat(name, ".json");

        printf("loading hashes.\n");
        HashTable* hasht = createHashTable(1000000);
        addhash(hasht, "hashes.bintypes.txt");
        addhash(hasht, "hashes.binfields.txt");
        addhash(hasht, "hashes.binhashes.txt");
        addhash(hasht, "hashes.binentries.txt");
        printf("finised loading hashes.\n");

        printf("reading file.\n");
        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        fseek(file, 0, SEEK_SET);
        char* fp = (char*)malloc(fsize + 1);
        fread(fp, fsize, 1, file);
        fp[fsize] = '\0';
        fclose(file);

        cJSON* root = cJSON_CreateObject();

        uint32_t Signature = 0;
        memfread(&Signature, 4, &fp);
        if (memcmp(&Signature, "PTCH", 4) == 0)
        {
            fp += 8;
            memfread(&Signature, 4, &fp);
            if (memcmp(&Signature, "PROP", 4) != 0)
            {
                printf("skn has no valid signature\n");
                scanf("press enter to exit.");
                return 1;
            }
            cJSON_AddItemToObject(root, "signature", cJSON_CreateString("PTCH"));
        }
        if (memcmp(&Signature, "PROP", 4) != 0)
        {
            printf("skn has no valid signature\n");
            scanf("press enter to exit.");
            return 1;
        }
        else
            cJSON_AddItemToObject(root, "Signature", cJSON_CreateString("PROP"));

        uint32_t Version = 0;
        memfread(&Version, 4, &fp);
        cJSON_AddItemToObject(root, "Version", cJSON_CreateNumber(&Version, jUInt32));

        uint32_t linkedFilesCount = 0;
        char** LinkedList = (char**)calloc(1, 1);
        if (Version >= 2)
        {
            uint16_t stringlength = 0;
            memfread(&linkedFilesCount, 4, &fp);
            char** LinkedListt = (char**)calloc(linkedFilesCount, sizeof(char*));
            for (uint32_t i = 0; i < linkedFilesCount; i++) {
                memfread(&stringlength, 2, &fp);
                LinkedListt[i] = (char*)calloc(stringlength + 1, 1);
                memfread(LinkedListt[i], (size_t)stringlength, &fp);
                LinkedListt[i][stringlength] = '\0';
            }
            LinkedList = LinkedListt;
        }

        cJSON* linkedListarray = cJSON_CreateArray();
        for (uint32_t i = 0; i < linkedFilesCount; i++)
            cJSON_AddItemToArray(linkedListarray, cJSON_CreateString(LinkedList[i]));
        cJSON_AddItemToObject(root, "Linked", linkedListarray);

        uint32_t entryCount = 0;
        memfread(&entryCount, 4, &fp);

        uint32_t* entryTypes = (uint32_t*)calloc(entryCount, 4);
        memfread(entryTypes, entryCount * 4, &fp);

        Map* entriesMap = (Map*)calloc(1, sizeof(Map));
        entriesMap->keyType = HASH;
        entriesMap->valueType = EMBEDDED;
        entriesMap->itemsize = entryCount;
        entriesMap->items = (Pair**)calloc(entryCount, sizeof(Pair*));
        for (size_t i = 0; i < entryCount; i++)
        {
            uint32_t entryLength = 0;
            memfread(&entryLength, 4, &fp);

            uint32_t entryKeyHash = 0;
            memfread(&entryKeyHash, 4, &fp);

            uint16_t fieldcount = 0;
            memfread(&fieldcount, 2, &fp);

            PointerOrEmbed* entry = (PointerOrEmbed*)calloc(1, sizeof(PointerOrEmbed));
            entry->itemsize = fieldcount;
            entry->name = entryTypes[i];
            entry->items = (Field**)calloc(fieldcount, sizeof(Field*));
            for (uint16_t o = 0; o < fieldcount; o++)
            {
                uint32_t name = 0;
                memfread(&name, 4, &fp);

                uint8_t type = 0;
                memfread(&type, 1, &fp);

                Field* fieldtmp = (Field*)calloc(1, sizeof(Field));
                fieldtmp->key = name;
                fieldtmp->value = readvaluebytype(type, hasht, &fp);
                entry->items[o] = fieldtmp;
            }

            void* ptr = calloc(1, sizeof(uint32_t));
            *((uint32_t*)ptr) = entryKeyHash;
            BinField* hash = (BinField*)calloc(1, sizeof(BinField));
            hash->typebin = HASH;
            hash->data = ptr;

            BinField* entrye = (BinField*)calloc(1, sizeof(BinField));
            entrye->typebin = EMBEDDED;
            entrye->data = entry;

            Pair* pairtmp = (Pair*)calloc(1, sizeof(Pair));
            pairtmp->key = hash;
            pairtmp->value = entrye;
            entriesMap->items[i] = pairtmp;
        }

        printf("finised reading file.\n");
        printf("creating json file.\n");
        cJSON* entriesarray = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "Entries", entriesarray);
        for (size_t i = 0; i < entriesMap->itemsize; i++)
        {
            cJSON* entry = cJSON_CreateObject();
            cJSON* entryarr = cJSON_CreateArray();
            PointerOrEmbed* pe = (PointerOrEmbed*)entriesMap->items[i]->value->data;
            cJSON_AddItemToObject(entriesarray, hashtostr(hasht, *(uint32_t*)entriesMap->items[i]->key->data), entry);
            cJSON_AddItemToObject(entry, hashtostr(hasht, pe->name), entryarr);
            for (size_t o = 0; o < pe->itemsize; o++)
            {
                cJSON* entryobj = cJSON_CreateObject();
                cJSON_AddItemToObject(entryobj, "name", cJSON_CreateString(hashtostr(hasht, pe->items[o]->key)));
                cJSON_AddItemToObject(entryobj, "type", cJSON_CreateString(Type_strings[pe->items[o]->value->typebin]));
                getvaluefromtype(pe->items[o]->value, hasht, entryobj, "data");
                cJSON_AddItemToArray(entryarr, entryobj);
            }
        }
        printf("finised creating json file.\n");
        printf("writing to file.\n");
        file = fopen(name, "wb");
        if (!file)
        {
            printf("ERROR: cannot write file \"%s\".", name);
            scanf("press enter to exit.");
            return 1;
        }
        char* out =  cJSON_Print(root, 1);
        fwrite(out, strlen(out), 1, file);
        printf("finised writing to file.\n");
        fclose(file);
    }
    else if (strcmp(argv[1], "-e") == 0)
    {
        FILE* file = fopen(argv[2], "rb");
        if (!file)
        {
            printf("ERROR: cannot read file \"%s\".", argv[2]);
            scanf("press enter to exit.");
            return 1;
        }
        char* name = (char*)calloc(strlen(argv[2]), 1);
        strcat(name, argv[2]);
        strip_ext(name);
        strcat(name, ".bin");

        printf("reading file.\n");
        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        fseek(file, 0, SEEK_SET);
        char* fp = (char*)malloc(fsize + 1);
        fread(fp, fsize, 1, file);
        fp[fsize] = '\0';
        fclose(file);

        cJSON* obj, *obje;
        size_t i = 0, k = 0;
        cJSON* root = cJSON_ParseWithLength(fp, fsize);
        char* Signature = (char*)cJSON_GetObjectItem(root, "Signature")->value;
        uint32_t Version = *(uint32_t*)cJSON_GetObjectItem(root, "Version")->value;     

        cJSON* linked = cJSON_GetObjectItem(root, "Linked");
        size_t likedsize = cJSON_GetArraySize(linked);
        char** LinkedList = (char**)calloc(likedsize, sizeof(char*));
        for (i = 0, obj = linked->child; obj != NULL; obj = obj->next, i++)
            LinkedList[i] = (char*)obj->value;

        cJSON* entries = cJSON_GetObjectItem(root, "Entries");
        size_t entryssize = cJSON_GetArraySize(entries);

        Map* entriesMap = (Map*)calloc(1, sizeof(Map));
        entriesMap->keyType = HASH;
        entriesMap->valueType = EMBEDDED;
        entriesMap->itemsize = entryssize;
        entriesMap->items = (Pair**)calloc(entryssize, sizeof(Pair*));
        for (i = 0, obj = entries->child; obj != NULL; obj = obj->next, i++)
        {
            uint32_t entryKeyHash = hashfromstring(obj->string);
            size_t fieldcount = cJSON_GetArraySize(obj->child);
            PointerOrEmbed* entry = (PointerOrEmbed*)calloc(1, sizeof(PointerOrEmbed));
            entry->itemsize = (uint16_t)fieldcount;
            entry->name = hashfromstring(obj->child->string);
            entry->items = (Field**)calloc(fieldcount, sizeof(Field*));
            for (k = 0, obje = obj->child->child; obje != NULL; obje = obje->next, k++)
            {
                Field* fieldtmp = (Field*)calloc(1, sizeof(Field));
                Type typebin = findtypebystring((char*)cJSON_GetObjectItem(obje, "type")->value);
                fieldtmp->key = hashfromstring((char*)cJSON_GetObjectItem(obje, "name")->value);
                fieldtmp->value = getvaluefromjson(typebin, obje, 1);
                entry->items[k] = fieldtmp;
            }
            void* ptr = calloc(1, sizeof(uint32_t));
            *((uint32_t*)ptr) = entryKeyHash;
            BinField* hash = (BinField*)calloc(1, sizeof(BinField));
            hash->typebin = HASH;
            hash->data = ptr;
            BinField* entryee = (BinField*)calloc(1, sizeof(BinField));
            entryee->typebin = EMBEDDED;
            entryee->data = entry;
            Pair* pairtmp = (Pair*)calloc(1, sizeof(Pair));
            pairtmp->key = hash;
            pairtmp->value = entryee;
            entriesMap->items[i] = pairtmp;
        }

        printf("finised reading file.\n");
        printf("creating bin file.\n");
        char* sig1 = "PROP", *sig2 = "PTCH";
        charv* str = (charv*)calloc(1, sizeof(charv));
        if (strcmp(Signature, sig2) == 0)
        {
            uint32_t unk1 = 1, unk2 = 0;
            memfwrite(sig2, 4, str);
            memfwrite((char*)&unk1, 4, str);
            memfwrite((char*)&unk2, 4, str);
        }
        memfwrite(sig1, 4, str);
        memfwrite((char*)&Version, 4, str);

        if (Version >= 2)
        {
            memfwrite((char*)&likedsize, 4, str);
            for (i = 0; i < likedsize; i++)
            {
                uint16_t len = (uint16_t)strlen(LinkedList[i]);
                memfwrite((char*)&len, 2, str);
                memfwrite(LinkedList[i], len, str);
            }
        }

        memfwrite((char*)&entriesMap->itemsize, 4, str);
        for (uint32_t i = 0; i < entriesMap->itemsize; i++)
            memfwrite((char*)&((PointerOrEmbed*)entriesMap->items[i]->value->data)->name, 4, str);

        for (uint32_t i = 0; i < entriesMap->itemsize; i++)
        {
            uint32_t entryLength = 0;
            PointerOrEmbed* pe = (PointerOrEmbed*)entriesMap->items[i]->value->data;
            uint32_t entryKeyHash = *(uint32_t*)entriesMap->items[i]->key->data;
            for (uint16_t k = 0; k < pe->itemsize; k++)
                entryLength += getsize(pe->items[k]->value) + 4 + 1;
            entryLength += 4 + 2;

            memfwrite((char*)&entryLength, 4, str);
            memfwrite((char*)&entryKeyHash, 4, str);
            memfwrite((char*)&pe->itemsize, 2, str);

            for (uint16_t k = 0; k < pe->itemsize; k++)
            {
                uint32_t name = pe->items[k]->key;
                uint8_t type = typetouint(pe->items[k]->value->typebin);

                memfwrite((char*)&name, 4, str);
                memfwrite((char*)&type, 1, str);

                writevaluebybin(pe->items[k]->value, str);
            }
        }

        printf("finised creating bin file.\n");
        printf("writing to file.\n");
        file = fopen(name, "wb");
        if (!file)
        {
            printf("ERROR: cannot write file \"%s\".", name);
            scanf("press enter to exit.");
            return 1;
        }
        fwrite(str->data, str->lenght, 1, file);
        printf("finised writing to file.\n");
        fclose(file);
    }
    return 0;
}