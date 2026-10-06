#pragma once

#include <stdint.h>
typedef int64_t t_int64;
typedef uint64_t t_uint64;
typedef int32_t t_int32;
typedef uint32_t t_uint32;
typedef int16_t t_int16;
typedef uint16_t t_uint16;
typedef int8_t t_int8;
typedef uint8_t t_uint8;

typedef int t_int;
typedef unsigned int t_uint;

typedef float t_float32;
typedef double t_float64;



namespace pfc {
	template<unsigned t_bytes>
	class sized_int_t;

	template<> class sized_int_t<1> {
	public:
		typedef t_uint8 t_unsigned;
		typedef t_int8 t_signed;
	};

	template<> class sized_int_t<2> {
	public:
		typedef t_uint16 t_unsigned;
		typedef t_int16 t_signed;
	};

	template<> class sized_int_t<4> {
	public:
		typedef t_uint32 t_unsigned;
		typedef t_int32 t_signed;
	};

	template<> class sized_int_t<8> {
	public:
		typedef t_uint64 t_unsigned;
		typedef t_int64 t_signed;
	};
}

typedef size_t t_size;
typedef pfc::sized_int_t< sizeof(size_t) >::t_signed t_ssize;


inline t_size MulDiv_Size(t_size x,t_size y,t_size z) {return (t_size) ( ((t_uint64)x * (t_uint64)y) / (t_uint64)z );}

// Use of this is *discouraged*, define provided for backwards compat
#define pfc_infinite (~0)

namespace pfc {
	constexpr t_uint16 infinite16 = UINT16_MAX;
    constexpr t_uint32 infinite32 = UINT32_MAX;
    constexpr t_uint64 infinite64 = UINT64_MAX;
	constexpr t_size infinite_size = SIZE_MAX;
};
