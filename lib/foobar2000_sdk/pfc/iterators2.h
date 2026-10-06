#pragma once

namespace pfc {
    template<typename array_t>
    struct iterator_array_t { typedef iterator_array_t<array_t> self_t;
        array_t * m_array;
        size_t m_index;
        auto & operator*() const { return (*m_array)[m_index]; }
        bool operator==(const self_t& other) { PFC_ASSERT(m_array == other.m_array); return m_index == other.m_index;}
        bool operator!=(const self_t& other) { PFC_ASSERT(m_array == other.m_array); return m_index != other.m_index;}
        void operator++() {++m_index;}
    };
    template<typename array_t>
    auto iterator_array( array_t * array, size_t index ) {return iterator_array_t<array_t>{array,index};}
    
    template<typename elem_t, typename array_t>
    struct iterator_array_typed_t { typedef iterator_array_typed_t<elem_t,array_t> self_t;
        array_t * m_array;
        size_t m_index;
        elem_t operator*() const { return (*m_array)[m_index]; }
        bool operator==(const self_t& other) { PFC_ASSERT(m_array == other.m_array); return m_index == other.m_index;}
        bool operator!=(const self_t& other) { PFC_ASSERT(m_array == other.m_array); return m_index != other.m_index;}
        void operator++() {++m_index;}
    };
    template<typename elem_t,typename array_t>
    auto iterator_array_typed( array_t * array, size_t index ) {return iterator_array_typed_t<elem_t, array_t>{array,index};}
}
