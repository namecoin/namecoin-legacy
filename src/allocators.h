// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_ALLOCATORS_H
#define BITCOIN_ALLOCATORS_H

#include <string.h>
#include <string>
#include <boost/thread/mutex.hpp>
#include <map>
#include <openssl/crypto.h> // for OPENSSL_cleanse()

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// This is used to attempt to keep keying material out of swap
// Note that VirtualLock does not provide this as a guarantee on Windows,
// but, in practice, memory that has been VirtualLock'd almost never gets written to
// the pagefile except in rare circumstances where memory is extremely low.
#else
#include <sys/mman.h>
#include <limits.h> // for PAGESIZE
#include <unistd.h> // for sysconf
#endif


//
// Allocator that clears its contents before deletion.
//
template<typename T>
struct zero_after_free_allocator : public std::allocator<T>
{
    // MSVC8 default copy constructor is broken
    typedef std::allocator<T> base;
    typedef typename base::size_type size_type;
    typedef typename base::difference_type  difference_type;
    typedef typename base::pointer pointer;
    typedef typename base::const_pointer const_pointer;
    typedef typename base::reference reference;
    typedef typename base::const_reference const_reference;
    typedef typename base::value_type value_type;
    zero_after_free_allocator() throw() {}
    zero_after_free_allocator(const zero_after_free_allocator& a) throw() : base(a) {}
    template <typename U>
    zero_after_free_allocator(const zero_after_free_allocator<U>& a) throw() : base(a) {}
    ~zero_after_free_allocator() throw() {}
    template<typename _Other> struct rebind
    { typedef zero_after_free_allocator<_Other> other; };

    void deallocate(T* p, std::size_t n)
    {  
        if (p != NULL)
            OPENSSL_cleanse(p, sizeof(T) * n);
        std::allocator<T>::deallocate(p, n);
    }
};


#endif
