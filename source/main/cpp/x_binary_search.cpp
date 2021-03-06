#include "xbase/x_debug.h"
#include "xbase/x_binary_search.h"

namespace xcore
{
	u32		x_BinarySearch(const void* inItem, const void* inData, u32 inLength, compare_predicate_fn predicate)
	{
		// Do a binary search on the data to obtain the index of the found item
		s32 r;
		r = predicate(inItem, inData, 0);
		if (r == -1)
			return -1;
		else if (r == 0)
			return 0;

		r = predicate(inItem, inData, inLength-1);
		if (r == 1)
			return -1;
		else if (r == 0)
			return inLength-1;

		s32 bounds[2] = {0, (s32)inLength};
		s32 mid = bounds[1] / 2;
		while (true)
		{
			r = predicate(inItem, inData, mid);
			if (r == 0)
			{
				if (mid>0 && (predicate(inItem, inData, mid-1)==0))
				{
					r = -1;
				}
				else
				{
					break;
				}
			}

			bounds[(1-r)>>1] = mid;
			u32 const distance = ((bounds[1] - bounds[0]) / 2);
			if (distance == 0)
			{
				mid -= (1-r)>>1;
				break;
			}
			mid = bounds[0] + distance;
		}
		return mid;
	}

	u32		x_LowerBound(const void* inItem, const void* inData, u32 inLength, compare_predicate_fn predicate)
	{
		// Do a binary search on the data to obtain the lowest index of a lesser-or-equal (LTE) item
		s32 r;
		r = predicate(inItem, inData, 0);
		if (r == -1)
			return 0;
		else if (r == 0)
			return 0;

		r = predicate(inItem, inData, inLength-1);
		if (r == 1)
			return inLength;

		u32 bounds[2] = {0, inLength};
		u32 mid = bounds[1] / 2;
		while (true)
		{
			r = predicate(inItem, inData, mid);
			if (r == 0)
			{
				if (mid>0 && (predicate(inItem, inData, mid-1)==0))
				{
					r = -1;
				}
				else
				{
					break;
				}
			}

			bounds[(1-r)>>1] = mid;
			u32 const distance = ((bounds[1] - bounds[0]) / 2);
			if (distance == 0)
			{
				mid -= (1-r)>>1;
				break;
			}
			mid = bounds[0] + distance;
		}

		return mid;
	}

	u32		x_UpperBound(const void* inItem, const void* inData, u32 inLength, compare_predicate_fn predicate)
	{
		// Do a binary search on the data to obtain the highest index of a greater-or-equal (GTE) item
		s32 r;
		r = predicate(inItem, inData, 0);
		if (r == -1)
			return 0;

		r = predicate(inItem, inData, inLength-1);
		if (r == 1)
			return inLength-1;
		else if (r == 0)
			return inLength-1;

		u32 bounds[2] = {0, inLength};
		u32 mid = bounds[1] / 2;
		while (true)
		{
			r = predicate(inItem, inData, mid);
			if (r == 0)
			{
				if ((mid+1)<inLength && (predicate(inItem, inData, mid+1)==0))
				{
					r = 1;
				}
				else
				{
					break;
				}
			}

			bounds[(1-r)>>1] = mid;
			u32 const distance = ((bounds[1] - bounds[0]) / 2);
			if (distance == 0)
			{
				mid -= (1-r)>>1;
				break;
			}
			mid = bounds[0] + distance;
		}

		return mid;
	}

};
