//author https://github.com/autergame
#define _CRT_SECURE_NO_WARNINGS
#include "cJSON.h"

#include <string>

#include "Myassert.h"
#include "Hashtable.h"
#include "BinReader.h"

std::string HashToString(HashTable& hashT, const uint32_t hashValue)
{
    std::string strvalue = hashT.Lookup(hashValue);
    if (strvalue.size() == 0)
    {
        strvalue.reserve(16);
        myassert(sprintf_s(strvalue.data(), 16, "0x%08" PRIX32, hashValue) <= 0)
    }
    return strvalue;
}

std::string HashToStringxx(HashTable& hashT, const uint64_t hashValue)
{
    std::string strvalue = hashT.Lookup(hashValue);
    if (strvalue.size() == 0)
    {
        strvalue.reserve(32);
        myassert(sprintf_s(strvalue.data(), 32, "0x%016" PRIX64, hashValue) <= 0)
    }
    return strvalue;
}

uint32_t HashFromString(std::string string)
{
    uint32_t hashValue = 0;
    if (string[0] == '0' && (string[1] == 'x' || string[1] == 'X'))
        hashValue = strtoul(string.c_str(), nullptr, 16);
    else
        hashValue = FNV1Hash(string.c_str(), string.size());
    return hashValue;
}

uint64_t HashFromStringxx(std::string string)
{
    uint64_t hashValue = 0;
    if (string[0] == '0' && (string[1] == 'x' || string[1] == 'X'))
        hashValue = strtoull(string.c_str(), nullptr, 16);
    else
        hashValue = XXHash(string.c_str(), string.size());
    return hashValue;
}

cJSON* WriteJsonValueByBinFieldType(BinField* value, HashTable& hashT, cJSON* json, const char* strdata)
{
    switch (value->type)
    {
        case BinType::NONE:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNull());
            break;
        case BinType::FLAG:
        case BinType::BOOLB:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateBool(value->data->b));
            break;
        case BinType::SInt8:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data->ui64, jSInt8));
            break;
        case BinType::UInt8:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data->ui64, jUInt8));
            break;
        case BinType::SInt16:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data->ui64, jSInt16));
            break;
        case BinType::UInt16:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data->ui64, jUInt16));
            break;
        case BinType::SInt32:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data->ui64, jSInt32));
            break;
        case BinType::UInt32:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data->ui64, jUInt32));
            break;
        case BinType::SInt64:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data->ui64, jSInt64));
            break;
        case BinType::UInt64:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(&value->data->ui64, jUInt64));
            break;
        case BinType::Float32:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateNumber(value->data->floatv, jFloat32));
            break;
        case BinType::VEC2:
        case BinType::VEC3:
        case BinType::VEC4:
        case BinType::MTX44:
        {
            float* arr = value->data->floatv;
            cJSON* jsonarr = cJSON_CreateArray();
            for (size_t i = 0; i < Type_size[(uint8_t)value->type] / 4; i++)
                cJSON_AddItemToArray(jsonarr, cJSON_CreateNumber(&arr[i], jFloat32));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            break;
        }
        case BinType::RGBA:
        {
            uint8_t* arr = value->data->rgba;
            cJSON* jsonarr = cJSON_CreateArray();
            for (int i = 0; i < 4; i++)
                cJSON_AddItemToArray(jsonarr, cJSON_CreateNumber(&arr[i], jUInt8));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            break;
        }
        case BinType::STRING:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateString(value->data->string));
            break;
        case BinType::HASH:
        case BinType::LINK:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateString(HashToString(hashT, value->data->ui32).c_str()));
            break;
        case BinType::WADENTRYLINK:
            cJSON_AddItemToObject(json, strdata, cJSON_CreateString(HashToStringxx(hashT, value->data->ui64).c_str()));
            break;
        case BinType::CONTAINER:
        case BinType::STRUCT:
        {
            cJSON* jsonarr = cJSON_CreateArray();
            ContainerOrStructOrOption* cs = value->data->cso;
            cJSON_AddItemToObject(json, "containertype", cJSON_CreateString(Type_strings[(uint8_t)cs->valueType]));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            for (uint32_t i = 0; i < cs->items.size(); i++)
            {
                if ((cs->valueType >= BinType::CONTAINER && cs->valueType <= BinType::EMBEDDED) ||
                     cs->valueType == BinType::MAP || cs->valueType == BinType::OPTION)
                {
                    cJSON* jsonobj = cJSON_CreateObject();
                    WriteJsonValueByBinFieldType(cs->items[i], hashT, jsonobj, "data");
                    cJSON_AddItemToArray(jsonarr, jsonobj);
                }
                else
                    WriteJsonValueByBinFieldType(cs->items[i], hashT, jsonarr, "data");
            }
            break;
        }
        case BinType::POINTER:
        case BinType::EMBEDDED:
        {
            cJSON* jsonarr = cJSON_CreateArray();
            PointerOrEmbed* pe = value->data->pe;
            cJSON_AddItemToObject(json, HashToString(hashT, pe->name).c_str(), jsonarr);
            for (uint16_t i = 0; i < pe->items.size(); i++)
            {
                cJSON* jsonobj = cJSON_CreateObject();
                cJSON_AddItemToObject(jsonobj, "name", cJSON_CreateString(HashToString(hashT, pe->items[i].key).c_str()));
                cJSON_AddItemToObject(jsonobj, "type", cJSON_CreateString(Type_strings[(uint8_t)pe->items[i].value->type]));
                WriteJsonValueByBinFieldType(pe->items[i].value, hashT, jsonobj, "data");
                cJSON_AddItemToArray(jsonarr, jsonobj);
            }
            break;
        }
        case BinType::OPTION:
        {
            ContainerOrStructOrOption* op = value->data->cso;
            cJSON* jsonarr = cJSON_CreateArray();
            cJSON_AddItemToObject(json, "optiontype", cJSON_CreateString(Type_strings[(uint8_t)op->valueType]));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            for (uint8_t i = 0; i < op->items.size(); i++)
            {
                if ((op->valueType >= BinType::CONTAINER && op->valueType <= BinType::EMBEDDED) ||
                     op->valueType == BinType::MAP || op->valueType == BinType::OPTION)
                {
                    cJSON* jsonobj = cJSON_CreateObject();
                    WriteJsonValueByBinFieldType(op->items[i], hashT, jsonobj, "data");
                    cJSON_AddItemToArray(jsonarr, jsonobj);
                }
                else
                    WriteJsonValueByBinFieldType(op->items[i], hashT, jsonarr, "data");
            }
            break;
        }
        case BinType::MAP:
        {
            Map* mp = value->data->map;
            cJSON* jsonarr = cJSON_CreateArray();
            cJSON_AddItemToObject(json, "keytype", cJSON_CreateString(Type_strings[(uint8_t)mp->keyType]));
            cJSON_AddItemToObject(json, "valuetype", cJSON_CreateString(Type_strings[(uint8_t)mp->valueType]));
            cJSON_AddItemToObject(json, strdata, jsonarr);
            for (uint32_t i = 0; i < mp->items.size(); i++)
            {
                cJSON* jsonobj = cJSON_CreateObject();
                if ((mp->keyType >= BinType::CONTAINER && mp->keyType <= BinType::EMBEDDED) || 
                     mp->keyType == BinType::MAP || mp->keyType == BinType::OPTION)
                {
                    cJSON* jsonobje = cJSON_CreateObject();
                    cJSON_AddItemToObject(jsonobj, "keydata", jsonobje);
                    WriteJsonValueByBinFieldType(mp->items[i].key, hashT, jsonobje, "data");
                }
                else
                    WriteJsonValueByBinFieldType(mp->items[i].key, hashT, jsonobj, "keydata");
                if ((mp->valueType >= BinType::CONTAINER && mp->valueType <= BinType::EMBEDDED) || 
                     mp->valueType == BinType::MAP || mp->valueType == BinType::OPTION)
                {
                    cJSON* jsonobje = cJSON_CreateObject();
                    cJSON_AddItemToObject(jsonobj, "valuedata", jsonobje);
                    WriteJsonValueByBinFieldType(mp->items[i].value, hashT, jsonobje, "data");
                }
                else
                    WriteJsonValueByBinFieldType(mp->items[i].value, hashT, jsonobj, "valuedata");
                cJSON_AddItemToArray(jsonarr, jsonobj);
            }
            break;
        }
    }
    return json;
}

BinField* ReadValuerFomJsonValue(BinType typebin, cJSON* json, uint8_t getobject)
{
    int i = 0;
    cJSON* obj;
    cJSON* jdata = getobject ? cJSON_GetObjectItem(json, "data") : json;
    BinField* result = new BinField;
    result->type = typebin;
    switch (typebin)
    {
        case BinType::SInt8:
        case BinType::UInt8:
        case BinType::SInt16:
        case BinType::UInt16:
        case BinType::SInt32:
        case BinType::UInt32:
        case BinType::SInt64:
        case BinType::UInt64:
            memcpy(&result->data->ui64, jdata->value, Type_size[(uint8_t)typebin]);
            break;
        case BinType::FLAG:
        case BinType::BOOLB:
            result->data->b = *(bool*)jdata->value;
            break;
        case BinType::Float32:
            result->data->floatv = (float*)jdata->value;
            break;
        case BinType::VEC2:
        case BinType::VEC3:
        case BinType::VEC4:
        case BinType::MTX44:
        {
            float* data = new float[Type_size[(uint8_t)typebin] / 4];
            for (i = 0, obj = jdata->child; obj != NULL; obj = obj->next, i++)
                data[i] = *(float*)obj->value;
            result->data->floatv = data;
            break;
        }
        case BinType::RGBA:
        {
            uint8_t* data = new uint8_t[Type_size[(uint8_t)typebin]];
            for (i = 0, obj = jdata->child; obj != NULL; obj = obj->next, i++)
                data[i] = *(uint8_t*)obj->value;
            result->data->rgba = data;
            break;
        }
        case BinType::STRING:
            result->data->string = (char*)jdata->value;
            break;
        case BinType::HASH:
        case BinType::LINK:
        {
            uint32_t data = HashFromString((char*)jdata->value);
            result->data->ui32 = data;
            break;
        }
        case BinType::WADENTRYLINK:
        {
            uint64_t data = HashFromStringxx((char*)jdata->value);
            result->data->ui64 = data;
            break;
        }
        case BinType::CONTAINER:
        case BinType::STRUCT:
        {
            ContainerOrStructOrOption* tmpcs = new ContainerOrStructOrOption;

            tmpcs->valueType = FindTypeByString((char*)cJSON_GetObjectItem(json, "containertype")->value);

            cJSON* cs = cJSON_GetObjectItem(json, "data");
            tmpcs->items.reserve(cJSON_GetArraySize(cs));

            for (i = 0, obj = cs->child; obj != NULL; obj = obj->next, i++)
            { 
                if (tmpcs->valueType == BinType::POINTER || tmpcs->valueType == BinType::EMBEDDED)
                    tmpcs->items.emplace_back(ReadValuerFomJsonValue(tmpcs->valueType, obj->child, 0));
                else
                    tmpcs->items.emplace_back(ReadValuerFomJsonValue(tmpcs->valueType, obj, 0));           
            }
            result->data->cso = tmpcs;
            break;
        }
        case BinType::POINTER:
        case BinType::EMBEDDED:
        {
            PointerOrEmbed* tmppe = new PointerOrEmbed;

            cJSON* pe = getobject ? json->child->next->next : json;
            if (pe != NULL)
            {
                tmppe->name = HashFromString(pe->string);
                tmppe->items.reserve(cJSON_GetArraySize(pe));
                for (i = 0, obj = pe->child; obj != NULL; obj = obj->next, i++)
                {
                    EPField field;
                    field.key = HashFromString((char*)cJSON_GetObjectItem(obj, "name")->value);
                    field.value = ReadValuerFomJsonValue(FindTypeByString((char*)cJSON_GetObjectItem(obj, "type")->value), obj, 1);
                    tmppe->items.emplace_back(field);
                }
            }
            result->data->pe = tmppe;
            break;
        }
        case BinType::OPTION:
        {
            ContainerOrStructOrOption* tmpo = new ContainerOrStructOrOption;

            tmpo->valueType = FindTypeByString((char*)cJSON_GetObjectItem(json, "optiontype")->value);

            cJSON* op = cJSON_GetObjectItem(json, "data");
            tmpo->items.reserve(cJSON_GetArraySize(op));

            for (i = 0, obj = op->child; obj != NULL; obj = obj->next, i++)
            {
                if ((tmpo->valueType >= BinType::CONTAINER && tmpo->valueType <= BinType::EMBEDDED))
                    tmpo->items.emplace_back(ReadValuerFomJsonValue(tmpo->valueType, obj->child, 0));
                else
                    tmpo->items.emplace_back(ReadValuerFomJsonValue(tmpo->valueType, obj, 0));
            }
            result->data->cso = tmpo;
            break;
        }
        case BinType::MAP:
        {
            cJSON* key = cJSON_CreateObject();
            cJSON* value = cJSON_CreateObject();
            cJSON* map = cJSON_GetObjectItem(json, "data");

            Map* tmpmap = new Map;
            tmpmap->items.reserve(cJSON_GetArraySize(map));

            tmpmap->keyType = FindTypeByString((char*)cJSON_GetObjectItem(json, "keytype")->value);
            tmpmap->valueType = FindTypeByString((char*)cJSON_GetObjectItem(json, "valuetype")->value);

            for (i = 0, obj = map->child; obj != NULL; obj = obj->next, i++)
            {
                MapPair pairField;

                key = cJSON_GetObjectItem(obj, "keydata");
                value = cJSON_GetObjectItem(obj, "valuedata");

                if (tmpmap->keyType == BinType::POINTER || tmpmap->keyType == BinType::EMBEDDED)
                    key = key->child;
                if (tmpmap->valueType == BinType::POINTER || tmpmap->valueType == BinType::EMBEDDED)
                    value = value->child;

                pairField.key = ReadValuerFomJsonValue(tmpmap->keyType, key, 0);
                pairField.value = ReadValuerFomJsonValue(tmpmap->valueType, value, 0);

                tmpmap->items.emplace_back(pairField);
            }
            result->data->map = tmpmap;
            break;
        }
    }
    return result;
}

void strip_ext(char* fname)
{
    char* end = fname + strlen(fname);
    while (end > fname && *end != '.')
        --end;
    if (end > fname)
        *end = '\0';
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("Usage: binreader -d file.bin\n");
        printf("Usage: binreader -e file.json\n");
        scanf_s("press enter to exit.");
        return 1;
    }
    if (strcmp(argv[1], "-d") == 0)
    {
        printf("Loading hashes\n");

        HashTable hashT;
        hashT.LoadFromFile("hashes.bintypes.txt");
        hashT.LoadFromFile("hashes.binfields.txt");
        hashT.LoadFromFile("hashes.binhashes.txt");
        hashT.LoadFromFile("hashes.binentries.txt");
#ifdef NDEBUG
        hashT.LoadFromFile("hashes.game.txt");
        hashT.LoadFromFile("hashes.lcu.txt");
#endif

        hashT.Insert(0xf9100aa9, "patch");
        hashT.Insert(0x84874d36, "path");
        hashT.Insert(0x425ed3ca, "value");

        printf("Loaded total of hashes: %zd\n", hashT.table.size());

        printf("Finised loading hashes\n\n");

        PacketBin packet;
        packet.DecodeBin(argv[2], hashT);

        printf("Creating json file.\n");

        cJSON* root = cJSON_CreateObject();

        if (packet.m_isPatch)
            cJSON_AddItemToObject(root, "signature", cJSON_CreateString("PTCH"));
        else
            cJSON_AddItemToObject(root, "Signature", cJSON_CreateString("PROP"));

        cJSON_AddItemToObject(root, "Version", cJSON_CreateNumber(&packet.m_Version, jUInt32));

        if (packet.m_Version >= 2)
        {
            cJSON* linkedListarray = cJSON_CreateArray();
            for (uint32_t i = 0; i < packet.m_linkedList.size(); i++)
                cJSON_AddItemToArray(linkedListarray, cJSON_CreateString(packet.m_linkedList[i].c_str()));
            cJSON_AddItemToObject(root, "Linked", linkedListarray);
        }

        cJSON* entriesarray = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "Entries", entriesarray);

        Map* map = packet.m_entriesBin->data->map;
        for (size_t i = 0; i < map->items.size(); i++)
        {
            cJSON* entry = cJSON_CreateObject();
            cJSON* entryarr = cJSON_CreateArray();
            PointerOrEmbed* pe = map->items[i].value->data->pe;
            cJSON_AddItemToObject(entriesarray, HashToString(hashT, map->items[i].key->data->ui32).c_str(), entry);
            cJSON_AddItemToObject(entry, HashToString(hashT, pe->name).c_str(), entryarr);
            for (size_t o = 0; o < pe->items.size(); o++)
            {
                cJSON* entryobj = cJSON_CreateObject();
                cJSON_AddItemToObject(entryobj, "name", cJSON_CreateString(HashToString(hashT, pe->items[o].key).c_str()));
                cJSON_AddItemToObject(entryobj, "type", cJSON_CreateString(Type_strings[(uint8_t)pe->items[o].value->type]));
                WriteJsonValueByBinFieldType(pe->items[o].value, hashT, entryobj, "data");
                cJSON_AddItemToArray(entryarr, entryobj);
            }
        }

        printf("Finised creating json file.\n");

        printf("Writing to file.\n");

        size_t argsize = strlen(argv[2]);
        char* name = new char [argsize + 8];
        memset(name, '\0', argsize + 8);
        memcpy(name, argv[2], argsize);
        strip_ext(name);
        memcpy(name + strlen(name), ".json", 5);

        FILE* file;
        errno_t err = fopen_s(&file, name, "wb");
        if (err)
        {
            char errMsg[255] = { '\0' };
            strerror_s(errMsg, 255, err);
            printf("ERROR: Cannot write file %s %s\n", name, errMsg);
            return 0;
        }

        char* out = cJSON_Print(root, 1);
        fwrite(out, strlen(out), 1, file);

        fclose(file);

        printf("Finised writing to file.\n");
    }
    else if (strcmp(argv[1], "-e") == 0)
    {
        FILE* file;
        errno_t err = fopen_s(&file, argv[2], "rb");
        if (err)
        {
            char errMsg[255] = { "\0" };
            strerror_s(errMsg, 255, errno);
            printf("ERROR: Cannot read file %s %s\n", argv[2], errMsg);
            return 0;
        }

        printf("Reading file: %s\n", argv[2]);
        fseek(file, 0, SEEK_END);
        size_t fsize = (size_t)ftell(file);
        fseek(file, 0, SEEK_SET);
        std::string fp(fsize + 1, '\0');
        myassert(fread(fp.data(), 1, fsize, file) != fsize)
        fclose(file);
        printf("Finised reading file\n");

        printf("Reading json file.\n");

        cJSON* obj, *obje;
        size_t i = 0, k = 0;
        cJSON* root = cJSON_ParseWithLength(fp.c_str(), fsize);
        if (root == NULL)
        {
            printf("ERROR: Cannot read file \"%s\" %d.", argv[2], global_error.position);
            return 1;
        }

        PacketBin packet;

        char* signature = (char*)cJSON_GetObjectItem(root, "Signature")->value;
        if (memcmp(signature, "PTCH", 4) == 0)
        {
            packet.m_isPatch = true;
        }

        packet.m_Version = *(uint32_t*)cJSON_GetObjectItem(root, "Version")->value;

        if (packet.m_Version >= 2)
        {
            cJSON* linked = cJSON_GetObjectItem(root, "Linked");
            size_t linkedCount = cJSON_GetArraySize(linked);
            if (linkedCount > 0)
            {
                packet.m_linkedList.reserve(linkedCount);
                for (i = 0, obj = linked->child; obj != NULL; obj = obj->next, i++)
                    packet.m_linkedList.emplace_back((char*)obj->value);
            }
        }

        Map* entriesMap = new Map;
        entriesMap->keyType = BinType::HASH;
        entriesMap->valueType = BinType::EMBEDDED;

        packet.m_entriesBin = new BinField;
        packet.m_entriesBin->type = BinType::MAP;
        packet.m_entriesBin->data->map = entriesMap;

        cJSON* entries = cJSON_GetObjectItem(root, "Entries");
        uint32_t entriesCount = cJSON_GetArraySize(entries);

        if (entriesCount > 0)
        {
            entriesMap->items.reserve(entriesCount);
            for (i = 0, obj = entries->child; obj != NULL; obj = obj->next, i++)
            {
                uint32_t entryKeyHash = HashFromString(obj->string);
                uint32_t fieldCount = cJSON_GetArraySize(obj->child);

                PointerOrEmbed* embed = new PointerOrEmbed;
                embed->name = HashFromString(obj->child->string);
                embed->items.reserve(fieldCount);

                BinField* embedValue = new BinField;
                embedValue->parent = packet.m_entriesBin;
                embedValue->type = BinType::EMBEDDED;
                embedValue->data->pe = embed;

                BinField* hashKey = new BinField;
                hashKey->parent = packet.m_entriesBin;
                hashKey->type = BinType::HASH;
                hashKey->data->ui32 = entryKeyHash;

                MapPair pair;
                pair.key = hashKey;
                pair.value = embedValue;
                entriesMap->items.emplace_back(pair);

                for (k = 0, obje = obj->child->child; obje != NULL; obje = obje->next, k++)
                {
                    EPField field;
                    BinType typebin = FindTypeByString((char*)cJSON_GetObjectItem(obje, "type")->value);
                    field.key = HashFromString((char*)cJSON_GetObjectItem(obje, "name")->value);
                    field.value = ReadValuerFomJsonValue(typebin, obje, 1);
                    embed->items.emplace_back(field);
                }
            }
        }

        printf("Finised reading json file.\n\n");

        size_t argsize = strlen(argv[2]);
        char* name = new char[argsize + 8];
        memset(name, '\0', argsize + 8);
        memcpy(name, argv[2], argsize);
        strip_ext(name);
        memcpy(name + strlen(name), ".bin", 5);

        packet.EncodeBin(name);
    }
    return 0;
}