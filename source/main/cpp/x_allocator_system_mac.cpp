#include "xbase/x_target.h"
#ifdef TARGET_MAC

#include <stdlib.h>
#include <cstring>

#include "xbase/x_memory_std.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"

namespace xcore
{
	void * mac_aligned_malloc(size_t size, size_t alignment) 
	{
		void * p1;	// original block
		void ** p2;	// aligned block
		s32 offset = (alignment - 1) + (2 * sizeof(void *));
		if ((p1 = (void *)malloc(size + offset)) == NULL)
			return NULL;
		p2 = (void **)(((size_t)(p1) + offset) & ~(alignment - 1));
		p2[-1] = p1;
		p2[-2] = (void*)size;
		return p2;
	}

	void	mac_aligned_free(void* ptr)
	{
		free(((void**)ptr)[-1]);
	}

	void*	mac_aligned_realloc(void* ptr, size_t size, size_t alignment)
	{
		size_t old_size = (size_t)((void**)ptr)[-2];
		void* new_ptr = mac_aligned_malloc(size, alignment);
		memcpy(new_ptr, ptr, xmin(size, old_size));
		mac_aligned_free(ptr);
		return new_ptr;
	}

	class x_allocator_macos_system : public x_iallocator
	{
	public:
		x_allocator_macos_system()
			: mInitialized(0)
			, mDefaultAlignment(4)
			, mAllocationCount(0)
		{
		}

		void					init() 
		{
			mInitialized = 1;
			mAllocationCount = 0;
			mDefaultAlignment = 4;
		}

		bool					isInitialized()
		{
			return mInitialized == 1;
		}

		virtual const char*		name() const
		{
			return "macos system allocator";
		}

		virtual void*			allocate(xsize_t size, u32 alignment)
		{
			void* ptr = mac_aligned_malloc(alignment, size);
			++mAllocationCount;
			return ptr;
		}

		virtual void*			reallocate(void* ptr, xsize_t size, u32 alignment)
		{
			return mac_aligned_realloc(ptr, size, alignment);
		}

		virtual void			deallocate(void* ptr)
		{
			--mAllocationCount;
			mac_aligned_free(ptr);
		}

		virtual void			release()
		{
			ASSERTS(mAllocationCount==0, "ERROR: System Allocator is being released but still has allocations that are not freed");
			mInitialized = 0;
			mDefaultAlignment = 0;
			mAllocationCount = 0;
		}

		s32						mInitialized;
		s32						mDefaultAlignment;
		u64						mAllocationCount;
	};

	x_iallocator*		gCreateSystemAllocator()
	{
		static x_allocator_macos_system sSystemAllocator;
		if (!sSystemAllocator.isInitialized())
		{
			sSystemAllocator.init();
		}
		return &sSystemAllocator;
	}

};

#endif
