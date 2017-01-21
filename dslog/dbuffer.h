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
	//!ֱ��ʹ��һ���Ѿ����ڵ��ڴ�
	/*!
	* ��ֻ���ж�������ʱ������������ʡȥһ���ڴ��COPY���̣����ڽ��е�һ��д������ʱ�򣬸ö��ڴ�ᱻ���ƣ����Բ���������
	* �����ڴ��ǲ��ᱻ�ƻ���
	* \param mem ���������Ѵ����ڴ�
	* \param size �ö��ڴ��ʵ�ʳߴ�
	*/
	inline DBuffer(void* mem, size_t size, IAlocator* p = 0)
		: buffer((char*)mem)
		, pAlloc(p)
		, maxbuffer(size), begin(0), end(0)
		, flags(1)
	{
	}
	//!Ԥ�����ڴ�
	inline DBuffer(size_t size, IAlocator* p = 0)
		: buffer(0)
		, pAlloc(p)
		, maxbuffer(0), begin(0), end(0)
		, flags(0)
	{
		reserve(size);
	}
	~DBuffer();

	//!���÷�����
	inline void setAlloator(IAlocator* p) { pAlloc = p; }

	//!��ǰ�����һ���ڴ�
	char* fromBegin(size_t size);
	//!�Ӻ������һ���ڴ�
	char* fromEnd(size_t size);
	//!���м����һ���ڴ�
	char* fromMiddle(size_t start, size_t size);

	//!��ǰ���Ƶ�һ���ڴ�
	void removeBegin(size_t size);
	//!�Ӻ����Ƶ�һ���ڴ�
	void removeEnd(size_t size);
	//!���м��Ƶ�һ���ڴ�
	void removeMiddle(size_t start, size_t size);

	//!��һ�������ƶ���ָ����λ��
	/*!
	* \param start ��ʼ���ݵ�λ��
	* \param size Ҫ�ƶ������ֽ�
	* \param to �ƶ�����λ�ã���ʾ�Ӹ�λ�ÿ�ʼ���Ҫ�ƶ������ݡ�Ϊ-1����ƶ��������
	* \return ����false��ʾû�����κ��ƶ�������������ǲ������������ݲ��ԣ����߸����Ͳ���Ҫ�ƶ����ھ��Ѿ��������λ�����ˡ�����true���ʾ�������ƶ�����
	*/
	bool moveData(size_t start, size_t size, size_t to);

	//!ֱ�������ڴ��ĳߴ�ͬʱ��ü�/��չ���ߴ磨�����ڴ��Ƿ����·����ˣ�
	bool resize(size_t size, size_t reserved = 0);
	//!Ԥ��һ���ڴ棬�����ı�size()�������ڴ��Ƿ����·����ˣ�
	bool reserve(size_t size);
	//!���·����ڴ棬��ʵgetbuf_frombegin��getbuf_fromend�Ⱥ�����������ʹ�ñ�������ʵ�ֵľ��幦��
	bool reallocate(size_t newsize, size_t before, size_t after);
	//!�ͷ��ڴ�
	void deallocate();

	//!���ڴ渳������һ��DBuffer����
	/*!
	* ����֮�󣬱�����Ͳ�����ִ���ڴ��ͷŲ����ˡ�Ҳ����˵���ڴ桰�ơ����˱������ߣ��ͷŵĹ���������Ҫ���������������
	*/
	void assignTo(DBuffer& to);
	//!������һ��DBuffer����ڴ沢�ҽ�
	void attach(DBuffer& from);

	//!����ڴ棨���ͷţ�
	void clear()
	{
		begin = end = 0;
	}

	//!���õ������ڴ�������ĳ����ֵʱ���������·����ڴ�
	inline void setUnusedUpper(size_t size)
	{
		flags = (flags & ~1) | (size << 1);
	}

	//!�ҽ��ڴ棨ֻ��ģʽ)
	void attachMem(const void* p, size_t size);
	//!����ڴ�
	/*!
	* ����ڴ漴���������׵�ַ�������ֽ����������ñ������ݣ��ͺ�û�з�����ڴ�һ����Ҳ����˵���ڴ���ͷž���Ҫ�ⲿ���й����ˣ�������ʱ��������Ҳ������
	* ��ȥ���κ��ڴ�Ĺ���
	* \param pnSize �����ΪNULL���򷵻����ݳߴ�
	* \return ���������׵�ַ�ڴ�
	*/
	char* detachMem(size_t* pnSize);

	//!�ͷ���detachMem����������ڴ棨����ͬһ��Scope�е��ڴ��ͷ��ǲ���ȫ�ģ�
	static void freeDetachMem(void* mem);

	//!�ж�ָ���Ƿ�����Ч��Χ�ڣ�ͬʱ�����ɲ���2����ƫ����
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

	//!���׷������
	inline void append(const void* src, size_t s)
	{
		if (s)
		{
			char* p = fromEnd(s);
			memcpy(p, src, s);
		}
	}
	//!���׷���ַ�
	inline void append(char c, size_t repeat = 1)
	{
		char* p = fromEnd(repeat);
		for (size_t i = 0; i < repeat; ++ i)
			p[i] = c;
	}
	//!���׷���ַ�����\0��β
	inline void appendstr(const char* s, size_t len)
	{
		char* p = fromEnd(len + 1);
		if (len) memcpy(p, s, len);
		p[len] = 0;
	}
	//!��ǰ׷������
	inline void prepend(const void* src, size_t s)
	{
		char* p = fromBegin(s);
		memcpy(p, src, s);
	}

	//!��ȡ����
	inline char* data() const { return buffer + begin; }
	//!��ȡ�ַ�������
	inline char* c_str() { if (!buffer) return 0;  buffer[end] = 0; return buffer + begin; }
	//!������ݳߴ�
	inline size_t size() const { return end - begin; }
	//!��ȡ�ѷ�����ڴ���
	inline size_t maxsize() const { return maxbuffer; }
	//!��ȡ�����ܵĿ����ڴ���
	inline size_t unusedsize() const { return maxbuffer - end + begin; }

	//!ֱ������һ���ַ�
	inline DBuffer& operator + (char c)
	{
		*fromEnd(1) = c;
		return *this;
	}
	//!����һ��16λ����
	inline DBuffer& operator + (uint16_t v)
	{
		*(uint16_t*)fromEnd(sizeof(uint16_t)) = v;
		return *this;
	}
	//!����һ��32λ����
	inline DBuffer& operator + (uint32_t v)
	{
		*(uint32_t*)fromEnd(sizeof(uint32_t)) = v;
		return *this;
	}
	//!���Ӹ�����
	inline DBuffer& operator + (double v)
	{
		char dbl[40] = { 0 };
		size_t l = sprintf(dbl, "%f", v);
		memcpy(fromEnd(l), dbl, l);
		return *this;
	}
	//!����һ���ַ���
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