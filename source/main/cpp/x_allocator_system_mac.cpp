#include "xbase/x_target.h"
#ifdef TARGET_MAC

#    include <stdlib.h>
#    include <cstring>
#    include <stdio.h>

#    include "xbase/x_memory.h"
#    include "xbase/x_integer.h"
#    include "xbase/x_allocator.h"

namespace xcore
{
    void* mac_aligned_malloc(size_t size, size_t alignment)
    {
        if (alignment < (2 * sizeof(void*)))
        {
            alignment = (2 * sizeof(void*));
        }

        void*     p1; // original block
        void**    p2; // aligned block
        s32 const offset = alignment + (2 * sizeof(void*));
        if ((p1 = malloc(size + offset)) == NULL)
            return NULL;
        p2     = (void**)(((uptr)(p1) + offset) & ~(alignment - 1));
        p2[-1] = p1;
        p2[-2] = (void*)size;
        if (((uptr)p2 & (alignment - 1)) != 0)
        {
            printf("ERROR in mac_aligned_malloc\n");
        }
        return (void*)p2;
    }

    void mac_aligned_free(void* ptr) { free(((void**)ptr)[-1]); }

    void* mac_aligned_realloc(void* ptr, size_t size, size_t alignment)
    {
        size_t old_size = (size_t)((void**)ptr)[-2];
        void*  new_ptr  = mac_aligned_malloc(size, alignment);
        memcpy(new_ptr, ptr, xmin(size, old_size));
        mac_aligned_free(ptr);
        return new_ptr;
    }

    class x_allocator_macos_system : public alloc_t
    {
    public:
        x_allocator_macos_system() : mInitialized(0), mDefaultAlignment(4), mAllocationCount(0) {}

        void init()
        {
            mInitialized      = 1;
            mAllocationCount  = 0;
            mDefaultAlignment = 4;
        }

        bool isInitialized() { return mInitialized == 1; }

        virtual void* v_allocate(u32 size, u32 alignment)
        {
            void* ptr = mac_aligned_malloc(size, alignment);
            ++mAllocationCount;
            return ptr;
        }

        virtual u32 v_deallocate(void* ptr)
        {
            --mAllocationCount;
            mac_aligned_free(ptr);
            return 0;
        }

        virtual void v_release()
        {
            ASSERTS(mAllocationCount == 0, "ERROR: System Allocator is being released but still has allocations that are not freed");
            mInitialized      = 0;
            mDefaultAlignment = 0;
            mAllocationCount  = 0;
        }

        s32 mInitialized;
        s32 mDefaultAlignment;
        u64 mAllocationCount;
    };

    x_allocator_macos_system sSystemAllocator;

    void alloc_t::init_system()
    {
        if (sSystemAllocator.isInitialized())
        {
            sSystemAllocator.init();
        }
    }

    alloc_t* alloc_t::get_system() { return &sSystemAllocator; }

}; // namespace xcore

#endif
