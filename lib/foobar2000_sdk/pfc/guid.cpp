#include "pfc-lite.h"
#include "guid.h"
#include "debug.h" // pfc::crash()
#include "string_base.h"

#ifdef _WIN32
#include <Objbase.h>
#else
#include "nix-objects.h"
#endif

/*
6B29FC40-CA47-1067-B31D-00DD010662DA
.
typedef struct _GUID {          // size is 16
    DWORD Data1;
    WORD   Data2;
    WORD   Data3;
    BYTE  Data4[8];
} GUID;

// {B296CF59-4D51-466f-8E0B-E57D3F91D908}
static const GUID <<name>> = 
{ 0xb296cf59, 0x4d51, 0x466f, { 0x8e, 0xb, 0xe5, 0x7d, 0x3f, 0x91, 0xd9, 0x8 } };

*/

namespace pfc {

static inline char print_hex_digit(unsigned val)
{
	static constexpr char table[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	PFC_ASSERT((val & ~0xF) == 0);
	return table[val];
}

static void print_hex(unsigned val,char * &out,unsigned bytes)
{
	unsigned n;
	for(n=0;n<bytes;n++)
	{
		unsigned char c = (unsigned char)((val >> ((bytes - 1 - n) << 3)) & 0xFF);
		*(out++) = print_hex_digit( c >> 4 );
		*(out++) = print_hex_digit( c & 0xF );
	}
	*out = 0;
}


pfc::string8 print_guid(const GUID & p_guid)
{
    char data[64];
	char * out = data;
	print_hex(p_guid.Data1,out,4);
	*(out++) = '-';
	print_hex(p_guid.Data2,out,2);
	*(out++) = '-';
	print_hex(p_guid.Data3,out,2);
	*(out++) = '-';
	print_hex(p_guid.Data4[0],out,1);
	print_hex(p_guid.Data4[1],out,1);
	*(out++) = '-';
	print_hex(p_guid.Data4[2],out,1);
	print_hex(p_guid.Data4[3],out,1);
	print_hex(p_guid.Data4[4],out,1);
	print_hex(p_guid.Data4[5],out,1);
	print_hex(p_guid.Data4[6],out,1);
	print_hex(p_guid.Data4[7],out,1);
	*out = 0;
    return data;
}


void print_hex_raw(const void * buffer,unsigned bytes,char * p_out)
{
	char * out = p_out;
	const unsigned char * in = (const unsigned char *) buffer;
	unsigned n;
	for(n=0;n<bytes;n++)
		print_hex(in[n],out,1);
}
    
    GUID createGUID() {
        GUID out;
#ifdef _WIN32
        if (FAILED(CoCreateGuid( & out ) ) ) crash();
#else
        pfc::nixGetRandomData( &out, sizeof(out) );
#endif
        return out;
    }

}



namespace pfc {
    pfc::string8 format_guid_cpp(const GUID & guid) {
        pfc::string8 s;
		s << "{0x" << pfc::format_hex(guid.Data1,8) << ", 0x" << pfc::format_hex(guid.Data2, 4) << ", 0x" << pfc::format_hex(guid.Data3,4) << ", {0x" << pfc::format_hex(guid.Data4[0],2);
		for(int n = 1; n < 8; ++n) {
			s << ", 0x" << pfc::format_hex(guid.Data4[n],2);
		}
		s << "}}";
        return s;
	}

	uint64_t halveGUID(const GUID & id) {
		static_assert(sizeof(id) == 2 * sizeof(uint64_t), "sanity" );
		union {
			GUID g;
			uint64_t u[2];
		} u;
		u.g = id;
		return u.u[0] ^ u.u[1];
	}


    [[maybe_unused]] static void test( ) {
        constexpr GUID guid1 = GUID_from_text("{B296CF59-4D51-466f-8E0B-E57D3F91D908}");
        constexpr GUID guid2 = { 0xb296cf59, 0x4d51, 0x466f, { 0x8e, 0xb, 0xe5, 0x7d, 0x3f, 0x91, 0xd9, 0x8 } };
        static_assert( guid1 == guid2 );
    }
}
