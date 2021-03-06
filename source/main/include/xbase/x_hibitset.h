#ifndef __X_HIERARCHICAL_BITSET_H__
#define __X_HIERARCHICAL_BITSET_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace xcore
{
    class alloc_t;

    // Number of bits and how much memory they consume
    // 32/1Kbit/32Kbit/1Mbit/32Mbit/1Gbit
    //  4/  128/   4KB/128KB/   4MB/128MB ( * ~1.5)

    // Example:
    // When you initialize this bitlist with maxbits = 512Kbit the
    // size_in_dwords() function will return 16913, or 67652 bytes.
    // level 0, bits= 524288, dwords= 16384, bytes= 65536
    // level 1, bits= 16384, dwords= 512, bytes= 2048
    // level 2, bits= 512, dwords= 16, bytes= 64
    // level 3, bits= 16, dwords= 1, bytes= 4
    // total: 65536 + 2048 + 64 + 4 = 67652 bytes

    class hibitset_t
    {
    public:
        inline hibitset_t() : m_numbits(0), m_maxlevel(0) {}

        void init(u32* bits, u32 maxbits);
        void init(alloc_t* alloc, u32 maxbits);

        void release(alloc_t* alloc);

        void reset();

        void set(u32 bit);
        void clr(u32 bit);

        bool is_set(u32 bit) const;
        bool is_full() const;

        bool find(u32& bit) const;             // First 0 or 1
        bool upper(u32 pivot, u32& bit) const; // First 0 or 1 equal to or great than @pivot
        bool lower(u32 pivot, u32& bit) const; // First 0 or 1 equal to or lesser than @pivot

        static u32 size_in_dwords(u32 maxbits);

        enum
        {
            AllBitsSet = 0xffffffff
        };

        // 7 levels maximum, this means a maximum of 7 * 5 = 2^35 = 34.359.738.368
        u32* m_levels[7];
        u32  m_numbits;
        s32  m_maxlevel;
    };
}; // namespace xcore

#endif /// __X_HIERARCHICAL_BITSET_H__