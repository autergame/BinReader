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

void addhash(HashTable* map, char* filename)
{
    FILE* file = fopen(filename, "rb");
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* fp = (char*)malloc(fsize + 1);
    fread(fp, fsize, 1, file);
    fp[fsize] = '\0';
    fclose(file);
    char* hashline = strtok(fp, "\n");
    while (hashline != NULL) {   
        uint32_t key = strtoul(hashline, NULL, 16);
        insertHashTable(map, key, hashline+9);
        hashline = strtok(NULL, "\n");
    }
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
    Type valueType;
    BinField* item;
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
                cJSON_AddItemToArray(jsonarr, cJSON_CreateNumber(&arr[i], jFloat32));
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
            cJSON_AddItemToObject(json, "valuetype", cJSON_CreateString(Type_strings[cs->valueType]));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            for (size_t i = 0; i < cs->itemsize; i++)
                getvaluefromtype(cs->items[i], hasht, jsonarr, "data");
            break;
        }
        case POINTER:
        case EMBEDDED:
        {
            cJSON* jsonarr = cJSON_CreateArray();
            PointerOrEmbed* pe = (PointerOrEmbed*)value->data;
            cJSON_AddItemToObject(json, strdata, cJSON_CreateString(hashtostr(hasht, pe->name)));
            cJSON_AddItemToObject(json, Type_strings[value->typebin], jsonarr);
            for (size_t i = 0; i < pe->itemsize; i++)
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
            cJSON_AddItemToObject(json, "valuetype", cJSON_CreateString(Type_strings[op->valueType]));
            getvaluefromtype(op->item, hasht, json, "data");
            break;
        }
        case MAP:
        {
            Map* mp = (Map*)value->data;
            cJSON* jsonarr = cJSON_CreateArray();
            cJSON_AddItemToObject(json, "keytype", cJSON_CreateString(Type_strings[mp->keyType]));
            cJSON_AddItemToObject(json, "valuetype", cJSON_CreateString(Type_strings[mp->valueType]));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            for (size_t i = 0; i < mp->itemsize; i++)
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

Type uinttotype(uint8_t type)
{
    if (type & 0x80) {
        type &= 0x7F;
        type += 18;
    }
    return (Type)type;
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
            tmpo->item = (BinField*)calloc(1, sizeof(BinField));
            memfread(&type, 1, fp);
            memfread(&count, 1, fp);
            tmpo->valueType = uinttotype(type);
            if (count != 0) 
                tmpo->item = readvaluebytype(tmpo->valueType, hasht, fp);
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

char* fname(char* path)
{
    char* aux = path;
    while (*path++);
    while (*path-- != '\\' && path != aux);
    return (aux == path) ? path : path + 2;
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
            printf("ERROR: cannot read file \"%s\".", argv[2]);
            scanf("press enter to exit.");
            return 1;
        }
        char* name = fname(argv[2]);
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
            for (uint16_t i = 0; i < fieldcount; i++)
            {
                uint32_t name = 0;
                memfread(&name, 4, &fp);

                uint8_t type = 0;
                memfread(&type, 1, &fp);

                Field* fieldtmp = (Field*)calloc(1, sizeof(Field));
                fieldtmp->key = name;
                fieldtmp->value = readvaluebytype(type, hasht, &fp);
                entry->items[i] = fieldtmp;
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
        cJSON* entrysarray = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "Entrys", entrysarray);
        for (size_t i = 0; i < entriesMap->itemsize; i++)
        {
            cJSON* entry = cJSON_CreateObject();
            cJSON* entryarr = cJSON_CreateArray();
            PointerOrEmbed* pe = (PointerOrEmbed*)entriesMap->items[i]->value->data;
            cJSON_AddItemToObject(entrysarray, hashtostr(hasht, *(uint32_t*)entriesMap->items[i]->key->data), entry);
            cJSON_AddItemToObject(entry, "data", cJSON_CreateString(hashtostr(hasht, pe->name)));
            cJSON_AddItemToObject(entry, "embedded", entryarr);
            for (size_t o = 0; o < pe->itemsize; o++)
            {
                cJSON* entryobj = cJSON_CreateObject();
                cJSON_AddItemToObject(entryobj, "name", cJSON_CreateString(hashtostr(hasht, pe->items[o]->key)));
                cJSON_AddItemToObject(entryobj, "type", cJSON_CreateString(Type_strings[pe->items[o]->value->typebin]));
                cJSON_AddItemToArray(entryarr, getvaluefromtype(pe->items[o]->value, hasht, entryobj, "data"));
            }
        }
        printf("finised json file.\n");
        printf("writing to file.\n");
        file = fopen(name, "wb");
        fprintf(file, cJSON_Print(root, 1));
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
        char* name = fname(argv[2]);
        strip_ext(name);
        strcat(name, ".bin");

        printf("reading file.\n");
        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        fseek(file, 0, SEEK_SET);
        char* fp = (char*)malloc(fsize + 1);
        fread(fp, fsize, 1, file);
        fclose(file);

        cJSON* obj;
        size_t i = 0;
        cJSON* root = cJSON_ParseWithLength(fp, fsize);
        char* Signature = (char*)cJSON_GetObjectItemCaseSensitive(root, "Signature")->value;
        uint32_t Version = *(uint32_t*)cJSON_GetObjectItemCaseSensitive(root, "Version")->value;


        cJSON* linked = cJSON_GetObjectItemCaseSensitive(root, "Linked");
        size_t likedsize = cJSON_GetArraySize(linked);
        char** LinkedList = (char**)calloc(likedsize, sizeof(char*));
        for (i = 0, obj = linked->child; obj != NULL; obj = obj->next, i++)
            LinkedList[i] = (char*)obj->value;

        cJSON* entries = cJSON_GetObjectItemCaseSensitive(root, "Entries");
        size_t entryssize = cJSON_GetArraySize(entries);
        uint32_t* entryNameHashes = (uint32_t*)calloc(entryssize, 4);
        for (i = 0, obj = entries->child; obj != NULL; obj = obj->next, i++)
        {

        }
    }
    return 0;
}