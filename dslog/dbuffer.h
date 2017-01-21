#pragma once

class DBuffer
{
public:
	class IAlocator
	{
	public:
		virtual void* alloc(DBuffer* pBuf, size_t& maxBuffer, size_t newRequest) = 0;
		virtual void dealloc(DBuffer* pBuf, void* p) = 0;
	};

public:
	inline DBuffer(IAlocator* p = 0)
		: buffer(0)
		, pAlloc(p)
		, maxbuffer(0), begin(0), end(0)
		, flags(0)
	{
	}
	//!直接使用一段已经存在的内存
	/*!
	* 在只进行读操作的时候，这样做可以省去一次内存的COPY过程，而在进行第一次写操作的时候，该段内存会被复制，所以参数给出的
	* 操作内存是不会被破坏的
	* \param mem 用来读的已存在内存
	* \param size 该段内存的实际尺寸
	*/
	inline DBuffer(void* mem, size_t size, IAlocator* p = 0)
		: buffer((char*)mem)
		, pAlloc(p)
		, maxbuffer(size), begin(0), end(0)
		, flags(1)
	{
	}
	//!预分配内存
	inline DBuffer(size_t size, IAlocator* p = 0)
		: buffer(0)
		, pAlloc(p)
		, maxbuffer(0), begin(0), end(0)
		, flags(0)
	{
		reserve(size);
	}
	~DBuffer();

	//!设置分配器
	inline void setAlloator(IAlocator* p) { pAlloc = p; }

	//!从前面分配一段内存
	char* fromBegin(size_t size);
	//!从后面分配一段内存
	char* fromEnd(size_t size);
	//!从中间分配一段内存
	char* fromMiddle(size_t start, size_t size);

	//!从前面移掉一段内存
	void removeBegin(size_t size);
	//!从后面移掉一段内存
	void removeEnd(size_t size);
	//!从中间移掉一段内存
	void removeMiddle(size_t start, size_t size);

	//!将一段数据移动到指定的位置
	/*!
	* \param start 开始数据的位置
	* \param size 要移动多少字节
	* \param to 移动到的位置，表示从该位置开始填充要移动的数据。为-1则会移动到最后面
	* \return 返回false表示没有做任何移动操作。这可能是参数给出的数据不对，或者根本就不需要移动现在就已经是在这个位置上了。返回true则表示进行了移动操作
	*/
	bool moveData(size_t start, size_t size, size_t to);

	//!直接设置内存块的尺寸同时会裁剪/扩展最大尺寸（返回内存是否重新分配了）
	bool resize(size_t size, size_t reserved = 0);
	//!预留一段内存，但不改变size()（返回内存是否重新分配了）
	bool reserve(size_t size);
	//!重新分配内存，其实getbuf_frombegin、getbuf_fromend等函数，最后均是使用本函数来实现的具体功能
	bool reallocate(size_t newsize, size_t before, size_t after);
	//!释放内存
	void deallocate();

	//!将内存赋于另外一个DBuffer对象
	/*!
	* 赋于之后，本对象就不会再执行内存释放操作了。也就是说将内存“移”给了被赋于者，释放的工作，就需要被赋予者来完成了
	*/
	void assignTo(DBuffer& to);
	//!从另外一个DBuffer解出内存并挂接
	void attach(DBuffer& from);

	//!清空内存（不释放）
	void clear()
	{
		begin = end = 0;
	}

	//!设置当空闲内存量大于某个阀值时，将会重新分配内存
	inline void setUnusedUpper(size_t size)
	{
		flags = (flags & ~1) | (size << 1);
	}

	//!挂接内存（只读模式)
	void attachMem(const void* p, size_t size);
	//!解出内存
	/*!
	* 解出内存即返回数据首地址和数据字节数，并重置本类数据，就和没有分配过内存一样。也就是说，内存的释放就需要外部自行管理了，在析构时，本对象也将不会
	* 再去做任何内存的工作
	* \param pnSize 如果不为NULL，则返回数据尺寸
	* \return 返回数据首地址内存
	*/
	char* detachMem(size_t* pnSize);

	//!释放由detachMem函数解出的内存（不在同一个Scope中的内存释放是不安全的）
	static void freeDetachMem(void* mem);

	//!判断指针是否在有效范围内（同时可以由参数2返回偏移量
	inline bool checkPointerAddress(const void* p, size_t* pnOffset) const
	{
		size_t off = (const char*)p - buffer;
		if (off >= begin && off < end)
		{
			if (pnOffset)
				*pnOffset = off - begin;
			return true;
		}
		return false;
	}

	//!向后追加数据
	inline void append(const void* src, size_t s)
	{
		if (s)
		{
			char* p = fromEnd(s);
			memcpy(p, src, s);
		}
	}
	//!向后追加字符
	inline void append(char c, size_t repeat = 1)
	{
		char* p = fromEnd(repeat);
		for (size_t i = 0; i < repeat; ++ i)
			p[i] = c;
	}
	//!向后追加字符串含\0结尾
	inline void appendstr(const char* s, size_t len)
	{
		char* p = fromEnd(len + 1);
		if (len) memcpy(p, s, len);
		p[len] = 0;
	}
	//!向前追加数据
	inline void prepend(const void* src, size_t s)
	{
		char* p = fromBegin(s);
		memcpy(p, src, s);
	}

	//!获取数据
	inline char* data() const { return buffer + begin; }
	//!获取字符串数据
	inline char* c_str() { if (!buffer) return 0;  buffer[end] = 0; return buffer + begin; }
	//!获得数据尺寸
	inline size_t size() const { return end - begin; }
	//!获取已分配的内存量
	inline size_t maxsize() const { return maxbuffer; }
	//!获取最大可能的空闲内存量
	inline size_t unusedsize() const { return maxbuffer - end + begin; }

	//!直接增加一个字符
	inline DBuffer& operator + (char c)
	{
		*fromEnd(1) = c;
		return *this;
	}
	//!增加一个16位整数
	inline DBuffer& operator + (uint16_t v)
	{
		*(uint16_t*)fromEnd(sizeof(uint16_t)) = v;
		return *this;
	}
	//!增加一个32位整数
	inline DBuffer& operator + (uint32_t v)
	{
		*(uint32_t*)fromEnd(sizeof(uint32_t)) = v;
		return *this;
	}
	//!增加浮点数
	inline DBuffer& operator + (double v)
	{
		char dbl[40] = { 0 };
		size_t l = sprintf(dbl, "%f", v);
		memcpy(fromEnd(l), dbl, l);
		return *this;
	}
	//!增加一个字符串
	inline DBuffer& operator + (const char* s)
	{
		size_t l = strlen(s);
		char* p = fromEnd(l);
		memcpy(p, s, l);
		return *this;
	}

	inline bool _unusedUpperChk() const
	{
		size_t u = flags >> 1;
		return (!u || u > maxbuffer - end + begin);
	}

private:
	void _reallocAndCopy(size_t maxBuffer, size_t newRequest, size_t copysize);

	char		*buffer;
	IAlocator	*pAlloc;
	size_t		maxbuffer, begin, end;
	size_t		flags;
} ;