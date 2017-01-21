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

		//���ܹ��ṩ�ĳߴ�
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

		//����󷽿����ṩ�ĳߴ粻����������ǰ�����ṩһ����
		if (afterEndProvide < size)
		{
			size_t before_begin_provider = size - afterEndProvide;

			memcpy(buffer + begin - before_begin_provider, buffer + begin, end - begin);
			begin -= before_begin_provider;
		}
	}
	else
	{
		//���·���
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
		//��ǰ���������ƿ����ٸ���һЩ�ֽڣ���˴�ǰ������
		if (start > 0)
			memmove(buffer + begin + size, buffer + begin, start);

		begin += size;
	}
	else
	{
		//�Ӻ�����ǰ�渴��
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
		//Ŀ��λ����Ҫ�ƶ��������м䣬��Ŀ��λ�øպ��ڽ������������Ͳ���Ҫ������
		return false;
	}

	char* p = buffer + begin;
	if (topos < start)
	{
		//��������ǰ�ƶ���������topos��start֮������ݾͻᱻ�����ƶ����˵�start + size֮��ȥ

		//�Ƚ�������ݱ��ݵ����
		size_t moves = start - topos;
		char* tmp = fromEnd(moves);
		memcpy(tmp, p + topos, moves);

		//Ȼ��ָ��������ǰ��
		memcpy(p + topos, p + start, size);
		//�ٽ����ݵ��������������˵Ŀ�϶��
		memcpy(p + topos + size, tmp, moves);
		//���ȥ����α��ݵ������ڴ�
		removeEnd(moves);

		return true;
	}
	else if (endpos < end - begin)
	{
		//����һ�ֿ����ԣ�����topos���������ݵĺ��档�ж�һ�£�������ݵĽ�β������������ڴ�εĽ�β����Ҳ����Ҫ������

		//��Ҫ�ƶ������ݱ��ݵ�ĩβ
		size_t moves = topos - endpos;
		char* tmp = fromEnd(size);
		memcpy(tmp, p + start, size);
		//Ȼ��endpos��topos֮��������ƶ���start��
		memcpy(p + start, p + endpos, moves);
		//����ٽ���ʱ���ݸ��Ƶ��ƶ�����֮���topos��
		memcpy(p + start + moves, tmp, size);
		//���ȥ����α��ݵ������ڴ�
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
		// �����ڴ�
		if (maxbuffer >= size)
		{
			// ���гߴ��㹻���������·��䡣�����ȴӺ����ã����������˲������ٴ�ǰ��ȡ
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

		// ��Ҫ���·���
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