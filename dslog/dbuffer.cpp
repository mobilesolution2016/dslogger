#include "stdafx.h"
#include "dbuffer.h"

DBuffer::~DBuffer()
{
	deallocate();
}

char* DBuffer::fromBegin(size_t size)
{
	size_t cur = end - begin;
	if (maxbuffer - cur < size)
	{
		reallocate(end - begin, size, maxbuffer - end);
	}
	else if (begin < size)
	{
		memmove(buffer + size, buffer + begin, cur);
		begin = size;
		end += size;
	}

	begin -= size;
	return buffer + begin;
}

char* DBuffer::fromEnd(size_t size)
{
	size_t cur = end - begin;
	if (maxbuffer - cur < size)
	{
		reallocate(end - begin, 0, size);
	}
	else if (maxbuffer - end < size)
	{
		if (cur > 0)
			memcpy(buffer, buffer + begin, cur);
		end -= begin;
		begin = 0;
	}

	char* ret = buffer + end;
	end += size;

	return ret;
}

char* DBuffer::fromMiddle(size_t start, size_t size)
{
	if (size < 1)
		return 0;

	if (start >= end - begin)
		return fromEnd(size);

	if (end - begin + size <= maxbuffer)
	{
		size_t copy, afterEndProvide = maxbuffer - end;

		//后方能够提供的尺寸
		if (afterEndProvide > 0)
		{
			copy = end - begin - start;
			if (copy > 0)
			{
				char* pStart = buffer + begin + start;
				memmove(pStart + size, pStart, copy);
			}

			end += std::min(afterEndProvide, size);
		}

		//如果后方可以提供的尺寸不够，则再由前方来提供一部分
		if (afterEndProvide < size)
		{
			size_t before_begin_provider = size - afterEndProvide;

			memcpy(buffer + begin - before_begin_provider, buffer + begin, end - begin);
			begin -= before_begin_provider;
		}
	}
	else
	{
		//重新分配
		char* newbuffer;
		size_t newsize = end - begin + size;

		if (pAlloc)
		{
			newbuffer = (char*)pAlloc->alloc(this, maxbuffer, newsize);
		}
		else
		{
			maxbuffer = newsize << 1;
			newbuffer = (char*)malloc(maxbuffer);
		}

		if (start > 0)
			memcpy(newbuffer, buffer + begin, start);
		memcpy(newbuffer + start + size, buffer + begin + start, end - begin - start);

		if ((flags & 0x1) == 0 && buffer)
		{
			if (pAlloc)
				pAlloc->dealloc(this, buffer);
			else
				free(buffer);
		}
		buffer = newbuffer;

		begin = 0;
		flags &= ~1;
		end = newsize;
	}

	return buffer + begin + start;
}


void DBuffer::removeBegin(size_t size)
{
	if (size >= end - begin)
	{
		end = begin = 0;
	}
	else if (size > 0)
	{
		if (maxbuffer == 0)
			resize(end - begin + 16);

		if (size > end - begin)
			size = end - begin;

		begin += size;
	}
}

void DBuffer::removeEnd(size_t size)
{
	if (size >= end - begin)
	{
		end = begin;
	}
	else if (size > 0)
	{
		if (maxbuffer == 0)
			resize(end - begin + 16);

		if (size > end - begin)
			size = end - begin;

		end -= size;
	}
}

void DBuffer::removeMiddle(size_t start, size_t size)
{
	size_t curSize = end - begin;
	if (size == 0 || start + size > curSize)
		return;

	if (maxbuffer == 0)
		resize(end - begin + 16);

	size_t backward_copy = curSize - start - size;
	if (start < backward_copy)
	{
		//从前面往后面推可以少复制一些字节，因此从前往后推
		if (start > 0)
			memmove(buffer + begin + size, buffer + begin, start);

		begin += size;
	}
	else
	{
		//从后面往前面复制
		if (backward_copy)
			memcpy(buffer + begin + start, buffer + begin + start + size, backward_copy);

		end -= size;
	}
}

bool DBuffer::moveData(size_t start, size_t size, size_t to)
{
	size_t topos = to;
	if (topos == -1 || topos > end)
		topos = end;

	if (start == topos)
		return false;

	size_t endpos = start + size;
	if (topos > start && topos <= endpos)
	{
		//目标位置在要移动的数据中间，或目标位置刚好在结束处，这样就不需要处理了
		return false;
	}

	char* p = buffer + begin;
	if (topos < start)
	{
		//将数据向前移动，这样从topos到start之间的数据就会被挤到移动完了的start + size之后去

		//先将这段数据备份到最后
		size_t moves = start - topos;
		char* tmp = fromEnd(moves);
		memcpy(tmp, p + topos, moves);

		//然后将指定的数据前移
		memcpy(p + topos, p + start, size);
		//再将备份的数据贴到移完了的空隙上
		memcpy(p + topos + size, tmp, moves);
		//最后去掉这段备份的数据内存
		removeEnd(moves);

		return true;
	}
	else if (endpos < end - begin)
	{
		//另外一种可能性，就是topos在整段数据的后面。判断一下，如果数据的结尾本身就是整个内存段的结尾，那也不需要处理了

		//将要移动的数据备份到末尾
		size_t moves = topos - endpos;
		char* tmp = fromEnd(size);
		memcpy(tmp, p + start, size);
		//然后将endpos到topos之间的数据移动到start处
		memcpy(p + start, p + endpos, moves);
		//最后再将临时数据复制到移动完了之后的topos处
		memcpy(p + start + moves, tmp, size);
		//最后去掉这段备份的数据内存
		removeEnd(size);

		return true;
	}

	return false;
}

bool DBuffer::resize(size_t size, size_t reserved)
{
	size_t cursize = end - begin;
	if (size > cursize)
	{
		// 增长内存
		if (maxbuffer >= size)
		{
			// 现有尺寸足够，无须重新分配。于是先从后面让，后面让完了不够，再从前面取
			size_t inc = size - cursize;
			size_t after = maxbuffer - end;
			size_t offer = std::min(after, inc);

			inc -= offer;
			end += offer;

			if (inc > 0)
			{
				memcpy(buffer + begin - inc, buffer + begin, end - begin);
				begin -= inc;
			}

			return false;
		}

		// 须要重新分配
		maxbuffer = std::max(size, reserved);
		_reallocAndCopy(maxbuffer, size, cursize);

		end = size;
		return true;
	}
	else
	{
		end -= cursize - size;
	}

	return false;
}

bool DBuffer::reserve(size_t size)
{
	if (maxbuffer >= size)
		return false;

	char* newbuffer;

	if (pAlloc)
	{
		newbuffer = (char*)pAlloc->alloc(this, maxbuffer, size);
	}
	else
	{
		maxbuffer = size;
		newbuffer = (char*)malloc(maxbuffer);
	}

	if (end > begin)
		memcpy(newbuffer + begin, buffer + begin, end - begin);
	if ((flags & 0x1) == 0 && buffer)
	{
		if (pAlloc)
			pAlloc->dealloc(this, buffer);
		else
			free(buffer);
	}

	buffer = newbuffer;
	flags &= ~1;

	return true;
}

bool DBuffer::reallocate(size_t newsize, size_t before, size_t after)
{
	if (buffer == 0 || maxbuffer < newsize + before + after)
	{
		char* newbuffer;

		if (pAlloc)
		{
			newbuffer = (char*)pAlloc->alloc(this, maxbuffer, newsize + before + after);
		}
		else
		{
			maxbuffer = std::max((size_t)256, (newsize + before + after) * 2);
			newbuffer = (char*)malloc(maxbuffer);
		}

		if (end > begin && buffer)
			memcpy(newbuffer + before, buffer + begin, end - begin);
		if ((flags & 0x1) == 0 && buffer)
		{
			if (pAlloc)
				pAlloc->dealloc(this, buffer);
			else
				free(buffer);
		}

		buffer = newbuffer;
		begin = before;
		end = begin + newsize;
		flags &= ~1;

		return true;
	}
	else
	{
		begin = before;
		end = begin + newsize;
	}

	return false;
}

void DBuffer::deallocate()
{
	if ((flags & 0x1) == 0 && buffer)
	{
		if (pAlloc)
			pAlloc->dealloc(this, buffer);
		else
			free(buffer);
	}

	flags &= ~1;
	buffer = 0;
	begin = end = maxbuffer = 0;
}

void DBuffer::assignTo(DBuffer& to)
{
	to = *this;
	this->flags |= 1;
}

void DBuffer::attachMem(const void* p, size_t size)
{
	deallocate();

	flags |= 1;
	buffer = (char*)const_cast<void*>(p);
	begin = 0;
	end = maxbuffer = size;
}

char* DBuffer::detachMem(size_t* pnSize)
{
	char* ret;
	if (pnSize)
		*pnSize = end - begin;

	if ((flags & 0x1) == 1)
	{
		char* copy = (char*)malloc(end - begin);
		memcpy(copy, buffer + begin, end - begin);
		ret = copy;
	}

	ret = buffer;

	flags &= ~1;
	buffer = 0;
	maxbuffer = begin = end = 0;

	return ret;
}

void DBuffer::attach(DBuffer& from)
{
	if ((flags & 0x1) == 0 && buffer)
	{
		if (pAlloc)
			pAlloc->dealloc(this, buffer);
		else
			free(buffer);
	}

	buffer = from.buffer;
	pAlloc = from.pAlloc;
	maxbuffer = from.maxbuffer;
	begin = from.begin;
	end = from.end;
	flags = from.flags;

	from.flags &= ~1;
	from.buffer = 0;
	from.maxbuffer = from.begin = from.end = 0;
}

void DBuffer::freeDetachMem(void* mem)
{
	free(mem);
}

void DBuffer::_reallocAndCopy(size_t maxBuffer, size_t newRequest, size_t copysize)
{
	char* newBuf = 0;
	if (pAlloc)
		newBuf = (char*)pAlloc->alloc(this, maxBuffer, newRequest);
	else if (maxBuffer)
		newBuf = (char*)malloc(maxBuffer);

	if (copysize)
		memcpy(newBuf, buffer, copysize);

	if ((flags & 0x1) == 0 && buffer)
	{
		if (pAlloc)
			pAlloc->dealloc(this, buffer);
		else
			free(buffer);
	}

	buffer = newBuf;
	flags &= ~1;

	begin = 0;
	end = copysize;
}