#ifndef _BINREADER_H_
#define _BINREADER_H_

#include <inttypes.h>

#include <unordered_map>
#include <algorithm>
#include <string>

#include "Myassert.h"

#include "Hashtable.h"

enum class BinType : uint8_t
{
	NONE,
	BOOLB,
	SInt8, UInt8, SInt16, UInt16, SInt32, UInt32, SInt64, UInt64,
	Float32, VEC2, VEC3, VEC4, MTX44,
	RGBA,
	STRING,
	HASH, WADENTRYLINK, 
	CONTAINER, STRUCT,
	POINTER, EMBEDDED,
	LINK,
	OPTION,
	MAP,
	FLAG
};

BinType Uint8ToType(uint8_t type);
uint8_t TypeToUint8(BinType type);
BinType FindTypeByString(const char* string);

static const char* Type_strings[] = {
	"None",
	"Bool",
	"SInt8", "UInt8", "SInt16", "UInt16", "SInt32", "UInt32", "SInt64", "UInt64",
	"Float32", "Vector2", "Vector3", "Vector4", "Matrix4x4",
	"RGBA",
	"String", 
	"Hash", "WadEntryLink", 
	"Container", "Struct",
	"Pointer", "Embedded",
	"Link", 
	"Option",
	"Map",
	"Flag"
};

struct ContainerOrStructOrOption;
struct PointerOrEmbed;
struct Map;

struct BinData
{
	union
	{
		bool b;
		uint32_t ui32;
		uint64_t ui64 = 0;

		union {
			float* floatv;
			uint8_t* rgba;
			char* string;

			ContainerOrStructOrOption *cso;
			PointerOrEmbed *pe;
			Map *map;
		};
	};
};

struct BinField
{
	BinType type = BinType::NONE;
	BinData *data = new BinData;

	BinField *parent = nullptr;
};

struct EPField
{
	uint32_t key = 0;
	BinField *value = nullptr;
};

struct MapPair
{
	BinField *key = nullptr;
	BinField *value = nullptr;
};

struct ContainerOrStructOrOption
{
	BinType valueType = BinType::NONE;
	std::vector<BinField*> items;
};

struct PointerOrEmbed
{
	uint32_t name = 0;
	std::vector<EPField> items;
};

struct Map
{
	BinType keyType = BinType::NONE;
	BinType valueType = BinType::NONE;
	std::vector<MapPair> items;
};

static const size_t Type_size[] = {
	0, //NONE
	1, //BOOLB
	1, 1, 2, 2, 4, 4, 8, 8, //SInt8, UInt8, SInt16, UInt16, SInt32, UInt32, SInt64, UInt64
	4, 8, 12, 16, 64, //Float32, VEC2, VEC3, VEC4, MTX44
	4, //RGBA
	0, //STRING
	4, 8, //HASH, WADENTRYLINK
	sizeof(ContainerOrStructOrOption), sizeof(ContainerOrStructOrOption),
	sizeof(PointerOrEmbed), sizeof(PointerOrEmbed),
	4, //LINK
	sizeof(ContainerOrStructOrOption),
	sizeof(Map),
	1 //FLAG
};

const uint32_t patchFNV = 0xf9100aa9; // FNV1Hash("patch")
const uint32_t pathFNV = 0x84874d36; // FNV1Hash("path")
const uint32_t valueFNV = 0x425ed3ca;  // FNV1Hash("value")

class PacketBin
{
public:
	bool m_isPatch = false;
	uint32_t m_Version = 0;
	uint64_t m_Unknown = 0;
	BinField *m_entriesBin = nullptr;
	BinField *m_patchesBin = nullptr;
	std::vector<std::string> m_linkedList;

	PacketBin() {}
	~PacketBin() {}

	int EncodeBin(char* filePath);
	int DecodeBin(char* filePath, HashTable& hasht);
};

class CharMemVector
{
public:
	size_t m_Pointer = 0;
	std::vector<uint8_t> m_Array;

	CharMemVector() {}
	~CharMemVector() {}

	template<typename T>
	void MemWrite(T value)
	{
		MemWrite((void*)&value, sizeof(T));
	}

	void MemWrite(void *value, size_t size)
	{
		m_Array.insert(m_Array.end(), (uint8_t*)value, (uint8_t*)value + size);
	}


	template<typename T>
	T MemRead()
	{
		return *(T*)MemRead(sizeof(T));
	}

	void *MemRead(size_t bytes)
	{
		uint8_t* buffer = new uint8_t[bytes];
		MemRead(buffer, bytes);
		return buffer;
	}

	void MemRead(void *buffer, size_t bytes)
	{
		auto arrayIt = m_Array.begin() + m_Pointer;
		std::_Adl_verify_range(arrayIt, arrayIt + bytes);

		myassert(memcpy(buffer, m_Array.data() + m_Pointer, bytes) != buffer)
		m_Pointer += bytes;
	}
};

#endif //_BINREADER_H_