#pragma once

#include "primitives.h"

namespace pfc {

    constexpr inline uint32_t _guid_read_hex(char c) {
        if (c >= '0' && c <= '9') return (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') return 0xa + (uint32_t)(c - 'a');
        else if (c >= 'A' && c <= 'F') return 0xa + (uint32_t)(c - 'A');
        else return UINT32_MAX;
    }

    constexpr inline GUID GUID_from_text(const char * text) {
        GUID ret = {};
        if (*text=='{') text++;
        for( unsigned i = 0; i < 8; ++ i ) {
            auto v = _guid_read_hex(*text++); if (v > 15) return {};
            ret.Data1 <<= 4;
            ret.Data1 |= v;
        }
        while(*text=='-') text++;
        for( unsigned i = 0; i < 4; ++ i ) {
            auto v = _guid_read_hex(*text++); if (v > 15) return {};
            ret.Data2 <<= 4;
            ret.Data2 |= (uint16_t)v;
        }
        while(*text=='-') text++;
        for( unsigned i = 0; i < 4; ++ i ) {
            auto v = _guid_read_hex(*text++); if (v > 15) return {};
            ret.Data3 <<= 4;
            ret.Data3 |= (uint16_t)v;
        }
        while(*text=='-') text++;
        for( unsigned i = 0; i < 2; ++ i ) {
            auto hi = _guid_read_hex(*text++); if (hi > 15) return {};
            auto lo = _guid_read_hex(*text++); if (lo > 15) return {};
            ret.Data4[i] = (uint8_t)hi << 4 | (uint8_t)lo;
        }
        while(*text=='-') text++;
        for( unsigned i = 0; i < 6; ++ i ) {
            auto hi = _guid_read_hex(*text++); if (hi > 15) return {};
            auto lo = _guid_read_hex(*text++); if (lo > 15) return {};
            ret.Data4[2+i] = (uint8_t)hi << 4 | (uint8_t)lo;
        }
        return ret;
    }

	inline int guid_compare(const GUID & g1,const GUID & g2) {return memcmp(&g1,&g2,sizeof(GUID));}

	constexpr inline bool guid_equal(const GUID & g1,const GUID & g2) {return (g1 == g2) ? true : false;}
	template<> inline int compare_t<GUID,GUID>(const GUID & p_item1,const GUID & p_item2) {return guid_compare(p_item1,p_item2);}

	static constexpr GUID guid_null = {};

	void print_hex_raw(const void * buffer,unsigned bytes,char * p_out);

	inline GUID makeGUID(t_uint32 Data1, t_uint16 Data2, t_uint16 Data3, t_uint8 Data4_1, t_uint8 Data4_2, t_uint8 Data4_3, t_uint8 Data4_4, t_uint8 Data4_5, t_uint8 Data4_6, t_uint8 Data4_7, t_uint8 Data4_8) {
		GUID guid = { Data1, Data2, Data3, {Data4_1, Data4_2, Data4_3, Data4_4, Data4_5, Data4_6, Data4_7, Data4_8 } };
		return guid;
	}
	inline GUID xorGUID(const GUID & v1, const GUID & v2) {
		GUID temp; memxor(&temp, &v1, &v2, sizeof(GUID)); return temp;
	}
    
    string8 format_guid_cpp( const GUID & );
    string8 print_guid( const GUID & );

    GUID createGUID();
	uint64_t halveGUID( const GUID & );
    
    struct predicateGUID {
        inline bool operator() ( const GUID & v1, const GUID & v2 ) const {return guid_compare(v1, v2) > 0;}
    };

}
