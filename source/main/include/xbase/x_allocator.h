#ifndef __XBASE_ALLOCATOR_H__
#define __XBASE_ALLOCATOR_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

#include "xbase/x_debug.h"
#include "xbase/x_memory.h"

namespace xcore
{
    class buffer_t;

    class alloc_t
    {
    public:
        static void    init_system();
        static alloc_t* get_system();

        void* allocate(u32 size, u32 alignment) { return v_allocate(size, alignment); }
        void* allocate(u32 size) { return v_allocate(size, sizeof(void*)); }
        u32   deallocate(void* ptr) { return v_deallocate(ptr); }
        void  release() { v_release(); }

        template <typename T, typename... Args> T* construct(Args... args)
        {
            void* mem    = v_allocate(sizeof(T), sizeof(void*));
            T*    object = new (mem) T(args...);
            return object;
        }

        template <typename T, typename... Args> T* placement(u32 EXTRA = 0, Args... args)
        {
            void* mem    = v_allocate(sizeof(T) + EXTRA, sizeof(void*));
            T*    object = new (mem) T(args...);
            return object;
        }

        template <typename T> void destruct(T* p)
        {
            p->~T();
            v_deallocate(p);
        }

    protected:
        virtual void* v_allocate(u32 size, u32 align) = 0; // Allocate memory with alignment
        virtual u32   v_deallocate(void* p)           = 0; // Deallocate/Free memory
        virtual void  v_release()                     = 0;

        virtual ~alloc_t() {}
    };

	// helper functions
	inline void* x_advance_ptr(void* ptr, u64 size) { return (void*)((uptr)ptr + size); }
    inline void* x_align_ptr(void* ptr, u32 alignment) { return (void*)(((uptr)ptr + (alignment - 1)) & ~((uptr)alignment - 1)); }
    inline uptr  x_diff_ptr(void* ptr, void* next_ptr) { return (uptr)((uptr)next_ptr - (uptr)ptr); }
	inline bool  x_is_in_range(void* buffer, u64 size, void* ptr) { uptr begin = (uptr)buffer; uptr end = begin + size; uptr cursor = (uptr)ptr; return cursor >= begin && cursor < end; }

    // fixed-size allocator
    class fsa_t
    {
    public:
        inline u32   size() const { return v_size(); }
        inline void* allocate() { return v_allocate(); }
        inline u32   deallocate(void* ptr) { return v_deallocate(ptr); }
        inline void  release() { v_release(); }

        template <typename T, typename... Args> T* construct(Args... args)
        {
            ASSERT(sizeof(T) <= size());
            void* mem    = v_allocate();
            T*    object = new (mem) T(args...);
            return object;
        }

        template <typename T> void destruct(T* p)
        {
            p->~T();
            v_deallocate(p);
        }

    protected:
        virtual u32   v_size() const      = 0;
        virtual void* v_allocate()        = 0;
        virtual u32   v_deallocate(void*) = 0;
        virtual void  v_release()         = 0;

        virtual ~fsa_t() {}
    };

	class fsa_to_alloc_t : public fsa_t
	{
	public:
		fsa_to_alloc_t() : m_allocator(nullptr), m_size(0) {}
		fsa_to_alloc_t(u32 size, alloc_t* allocator) : m_allocator(allocator), m_size(size) {}

	protected:
		virtual u32   v_size() const       { return m_size; }
		virtual void* v_allocate()         { return m_allocator->allocate(m_size, sizeof(void*)); }
		virtual u32   v_deallocate(void*p) { return m_allocator->deallocate(p); }
		virtual void  v_release()          { return m_allocator->release(); }

	private:
		alloc_t*	m_allocator;
		u32 m_size;
	};

    // The dexer interface, 'pointer to index' and 'index to pointer'
    class dexer_t
    {
    public:
        inline void* idx2ptr(u32 index) const { return v_idx2ptr(index); }
        inline u32   ptr2idx(void* ptr) const { return v_ptr2idx(ptr); }

        template <typename T> inline T*  idx2obj(u32 index) const { return static_cast<T*>(v_idx2ptr(index)); }
        template <typename T> inline u32 obj2idx(T* ptr) const { return v_ptr2idx(ptr); }

    protected:
        virtual void* v_idx2ptr(u32 index) const = 0;
        virtual u32   v_ptr2idx(void* ptr) const = 0;
    };

    class fsadexed_t : public fsa_t, public dexer_t
    {
    public:
        fsadexed_t() {}
        ~fsadexed_t() {}
    };

    // Global new and delete
    template <typename T, typename... Args> T* xnew(Args... args)
    {
        void* mem    = alloc_t::get_system()->allocate(sizeof(T));
        T*    object = new (mem) T(args...);
        return object;
    }

    template <typename T> void xdelete(T* p)
    {
        p->~T();
        alloc_t::get_system()->deallocate(p);
    }

    class alloc_buffer_t : public alloc_t
    {
        static inline xbyte* align_ptr(xbyte* ptr, uptr align) { return (xbyte*)(((uptr)ptr + (align - 1)) & ~(align - 1)); }

        xbyte* m_base;
        xbyte* m_ptr;
        xbyte* m_end;
        s64    m_cnt;

    public:
        alloc_buffer_t(buffer_t& storage);

    protected:
        virtual void* v_allocate(u32 size, u32 align)
        {
            if (m_ptr < m_end && align_ptr(m_ptr + size, align) <= m_end)
            {
                xbyte* storage = m_ptr;
                m_ptr          = align_ptr(m_ptr + size, sizeof(void*));
                m_cnt += 1;
                return storage;
            }
            return nullptr;
        }

        virtual u32 v_deallocate(void* p)
        {
            if (p != nullptr)
            {
                ASSERT(m_cnt > 0);
                m_cnt -= 1;
                if (m_cnt == 0)
                    m_ptr = m_base;
            }
            return 0;
        }

        virtual void v_release()
        {
            m_base = nullptr;
            m_ptr  = nullptr;
            m_end  = nullptr;
        }
    };

    // Allocate a one or more objects in-place
    class allocinplace_t : public alloc_t
    {
        xbyte* m_base;
        xbyte* m_data;
        u64    m_size;

    public:
        inline allocinplace_t(xbyte* data, u64 size)
            : m_base(data)
            , m_data(data)
            , m_size(size)
        {
        }

    protected:
        virtual void* v_allocate(u32 size, u32 align);
        virtual u32   v_deallocate(void* p);
        virtual void  v_release();
    };

    template <u32 L> class inplace_t
    {
        enum
        {
            SIZE = (L + 3) / 4
        };
        u64 m_memory[SIZE];

    public:
        inline inplace_t() {}
        allocinplace_t allocator() const { return allocinplace_t((xbyte*)m_memory, (u64)SIZE); }

        template <class T> inline T* object() { return static_cast<T*>(m_memory); }
    };

#define XCORE_CLASS_PLACEMENT_NEW_DELETE                                    \
    void* operator new(xcore::xsize_t num_bytes, void* mem) { return mem; } \
    void  operator delete(void* mem, void*) {}                              \
    void* operator new(xcore::xsize_t num_bytes) noexcept { return NULL; }           \
    void  operator delete(void* mem) {}

#define XCORE_CLASS_NEW_DELETE(get_allocator_func, align)                                                     \
    void* operator new(xcore::xsize_t num_bytes, void* mem) { return mem; }                                   \
    void  operator delete(void* mem, void*) {}                                                                \
    void* operator new(xcore::xsize_t num_bytes) { ASSERT(num_bytes < (xcore::xsize_t)2*1024*1024*1024); return get_allocator_func()->allocate((u32)num_bytes, align); } \
    void  operator delete(void* mem) { get_allocator_func()->deallocate(mem); }

#define XCORE_CLASS_ARRAY_NEW_DELETE(get_allocator_func, align)                                                 \
    void* operator new[](xcore::xsize_t num_bytes) { ASSERT(num_bytes < (xcore::xsize_t)2*1024*1024*1024); return get_allocator_func()->allocate((u32)num_bytes, align); } \
    void  operator delete[](void* mem) { get_allocator_func()->deallocate(mem); }

    template <class T> class cdtor_default_t
    {
    public:
        inline T* construct(void* mem) const { return static_cast<T*>(mem); }
        inline T* copy_construct(void* mem, T const* other) const
        {
            T* item = (T*)(mem);
            *item   = *other;
            return item;
        }
        inline void destruct(T* obj) const {}
    };

    template <class T> class cdtor_placement_new_t
    {
    public:
        inline T*   construct(void* mem) const { return new (mem) T(); }
        inline T*   copy_construct(void* mem, T const* other) const { return new (mem) T(*other); }
        inline void destruct(T* obj) const { obj->~T(); }
    };

    class dexed_array_t : public dexer_t
    {
    public:
        dexed_array_t()
            : m_data(nullptr)
            , m_sizeof(0)
            , m_countof(0)
        {
        }
        dexed_array_t(void* array_item, u32 sizeof_item, u32 countof_item);

        XCORE_CLASS_PLACEMENT_NEW_DELETE
    protected:
        virtual void* v_idx2ptr(u32 index) const;
        virtual u32   v_ptr2idx(void* ptr) const;

    private:
        void* m_data;
        u32   m_sizeof;
        u32   m_countof;
    };

    class fsadexed_array_t : public fsadexed_t
    {
    public:
        fsadexed_array_t();
        fsadexed_array_t(void* array_item, u32 sizeof_item, u32 countof_item);
        ~fsadexed_array_t() {}

        XCORE_CLASS_PLACEMENT_NEW_DELETE

    protected:
        virtual void* v_allocate();
        virtual u32   v_deallocate(void*);
        virtual u32   v_size() const;
        virtual void* v_idx2ptr(u32 index) const;
        virtual u32   v_ptr2idx(void* ptr) const;
        virtual void  v_release();

    private:
        void* m_data;
        u32   m_sizeof;
        u32   m_countof;
        u32   m_freelist;
        u32   m_freeindex;
    };

}; // namespace xcore

#endif ///< __XBASE_ALLOCATOR_H__
