#ifndef __TRACYFASTVECTOR_HPP__
#define __TRACYFASTVECTOR_HPP__

#include <assert.h>
#include <stddef.h>

#include "../common/TracyAlloc.hpp"
#include "../common/TracyForceInline.hpp"

namespace tracy
{

template<typename T>
class FastVector
{
public:
    using iterator = T*;
    using const_iterator = const T*;

    FastVector( size_t capacity )
        : m_ptr( (T*)tracy_malloc( sizeof( T ) * capacity ) )
        , m_write( m_ptr )
        , m_end( m_ptr + capacity )
    {
        assert( capacity != 0 );
    }

    FastVector( const FastVector& ) = delete;
    FastVector( FastVector&& ) = delete;

    ~FastVector()
    {
        tracy_free( m_ptr );
    }

    FastVector& operator=( const FastVector& ) = delete;
    FastVector& operator=( FastVector&& ) = delete;

    bool empty() const { return m_ptr == m_write; }
    size_t size() const { return m_write - m_ptr; }

    T* data() { return m_ptr; }
    const T* data() const { return m_ptr; };

    T* begin() { return m_ptr; }
    const T* begin() const { return m_ptr; }
    T* end() { return m_write; }
    const T* end() const { return m_write; }

    T& front() { assert( !empty() ); return m_ptr[0]; }
    const T& front() const { assert( !empty() ); return m_ptr[0]; }

    T& back() { assert( !empty() ); return m_write[-1]; }
    const T& back() const { assert( !empty() ); return m_write[-1]; }

    T& operator[]( size_t idx ) { return m_ptr[idx]; }
    const T& operator[]( size_t idx ) const { return m_ptr[idx]; }

    T* push_next()
    {
        if( m_write == m_end ) AllocMore();
        return m_write++;
    }

    T* prepare_next()
    {
        if( m_write == m_end ) AllocMore();
        return m_write;
    }

    void commit_next()
    {
        m_write++;
    }

    void clear()
    {
        m_write = m_ptr;
    }

    void swap( FastVector& vec )
    {
        const auto ptr1 = m_ptr;
        const auto ptr2 = vec.m_ptr;
        const auto write1 = m_write;
        const auto write2 = vec.m_write;
        const auto end1 = m_end;
        const auto end2 = vec.m_end;

        m_ptr = ptr2;
        vec.m_ptr = ptr1;
        m_write = write2;
        vec.m_write = write1;
        m_end = end2;
        vec.m_end = end1;
    }

private:
    tracy_no_inline void AllocMore()
    {
        const auto cap = size_t( m_end - m_ptr ) * 2;
        const auto size = size_t( m_write - m_ptr );
        T* ptr = (T*)tracy_malloc( sizeof( T ) * cap );
        memcpy( ptr, m_ptr, size * sizeof( T ) );
        tracy_free_fast( m_ptr );
        m_ptr = ptr;
        m_write = m_ptr + size;
        m_end = m_ptr + cap;
    }

    T* m_ptr;
    T* m_write;
    T* m_end;
};

}

#endif
