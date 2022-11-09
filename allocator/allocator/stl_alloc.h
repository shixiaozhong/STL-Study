#pragma once

#ifndef __THROW_BAD_ALLOC
#  if defined(__STL_NO_BAD_ALLOC) || !defined(__STL_USE_EXCEPTIONS)
#    include <stdio.h>
#    include <stdlib.h>
#    define __THROW_BAD_ALLOC fprintf(stderr, "out of memory\n"); exit(1)
#  else /* Standard conforming out-of-memory handling */
#    include <new>
#    define __THROW_BAD_ALLOC throw std::bad_alloc()
#  endif
#endif


#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include<iostream>


namespace my_stl
{
	// 第一级空间配置器
	template<int inst>
	class __malloc_alloc_template
	{
	private:
		// out-of-memory
		static void* oom_malloc(size_t n);	// 处理malloc out-of-memory
		static void* oom_realloc(void*, size_t);	// 处理realloc out-of-memory
		static void (*__malloc_alloc_oom_handler)();	// 处理out-of- memory;

	public:
		// 申请空间
		static void* allocate(size_t n)
		{
			void* result = malloc(n);	// 第一级配置器直接malloc
			if (result == nullptr)
				result = oom_malloc(n);	// malloc失败调用对应的malloc失败处理函数
			return result;
		}

		// 释放空间
		static void* deallocate(void* p)
		{
			free(p);	// 第一级配置器直接free
		}

		// 调整空间
		static void* reallocate(void* p, size_t new_size)
		{
			void* result = realloc(p, new_size);
			if (result == nullptr)
				result = oom_realloc(p, new_size);	// realloc失败调用对应的realloc失败处理函数
			return result;
		}

		// 仿真实现c++的new_handler, 指定自己的out-of-memory handler
		// hook操作 将__malloc_alloc_oom_handler函数调用转向调用f
		static void (*__set_malloc_handler(void(*f)()))()	// 函数本质是 void(*)() __set_malloc_alloc_oom_handler(void(*f)())
		{													// 返回值为函数指针，参数也是函数指针
			void(*old)() = __malloc_alloc_oom_handler;
			__malloc_alloc_oom_handler = f;
			return old;
		}
	};// end-of-__malloc_alloc_template


	// malloc_alloc out-of-memory handling
	template<int inst>
	void(*__malloc_alloc_template<inst>::__malloc_alloc_oom_handler)() = nullptr;	// 置空

	// malloc out-memory-handler实现
	template<int inst>
	void* __malloc_alloc_template<inst>::oom_malloc(size_t n)
	{
		void(*my_malloc_handler)();
		void* result;
		for (;;)
		{
			my_malloc_handler = __malloc_alloc_oom_handler;
			if (my_malloc_handler == nullptr)
				throw __THROW_BAD_ALLOC;
			(*my_malloc_handler)();	// 调用内存处理函数
			result = malloc(n);	//继续申请空间
			if (result)
				return result;
		}
	}

	// realloc out-of-memory实现
	template<int inst>
	void* __malloc_alloc_template<inst>::oom_realloc(void* p, size_t n)
	{
		void(*my_realloc_handler)();
		void* result;
		for (;;)
		{
			my_realloc_handler = __malloc_alloc_oom_handler;
			if (my_realloc_handler == nullptr)
				throw std::bad_alloc;
			(*my_realloc_handler)();
			result = realloc(p, n);
			if (result)
				return result;
		}
	}

	// 以下直接将参数指定为0
	typedef __malloc_alloc_template<0> malloc_alloc;	// typedef一下特殊情况


	enum { __ALIGN = 8 };	// 小型区块的上调边界
	enum { __MAX_BYTES = 128 };	// 小型区块的上限
	enum { __NFREELISTS = __MAX_BYTES / __ALIGN };	// free——lists的数量

												// 第二级空间配置器
	// 第一参数用于多线程环境，但是本书不讨论多线程环境
	template<bool threads, int inst>
	class __default_alloc_template
	{
	private:
		union obj	// 自由链表节点
		{
			union obj* free_list_link;	// obj指针，连接后续节点
			char client_data[1];	// 
		};
	private:
		static obj* volatile free_lists[__NFREELISTS]; // 定义自由链表数组

		// 找到大于bytes的最小的8的倍数
		static size_t ROUND_UP(size_t bytes)
		{
			return (bytes + __ALIGN - 1) & (~(__ALIGN - 1));
		}

		// 找对应链表的索引
		static size_t FREELIST_INDEX(size_t bytes)
		{
			return (bytes + __ALIGN - 1) / __ALIGN - 1;
		}

		//返回一个大小为n的对象，并可能加入大小为n的区块到其他free list,缺省获得20，可能会更少
		static void* refill(size_t n);

		// 配置一大块空间，可容纳nobjs*size个区块，如果配置nobjs个区块失败，nobjs可能会发生改变
		static char* chunk_alloc(size_t size, int& nobjs);

		// chunk allocation state
		static char* start_free;	// 内存池起始位置， 只在chunk_alloc中发生变化
		static char* end_free;		// 内存池结束位置，旨在chunk_alloc中发生变化
		static size_t heap_size;	// 堆的大小
	public:
		// 申请内存
		static void* allocate(size_t n)
		{
			obj* volatile* my_free_list;	// 对应自由链表的头指针
			obj* result;					// 申请到地址指针

			// 大于__MAX_BYTES,去调用一级配置器
			if (n > size_t(__MAX_BYTES))
			{
				return malloc_alloc::allocate(n);
			}
			my_free_list = free_lists + FREELIST_INDEX(n); // 找到对应链表的首地址
			result = *my_free_list;		// 保存对应链表的头节点的值
			// 对应链表头为空
			if (result == nullptr)
			{
				void* r = refill(ROUND_UP(n)); // 如果对应链表为空，调用refill为链表重新填充空间，新空间来自内存池(通过chunk_alloc)
				return r;
			}
			else
			{
				return result;
			}
		}

		// 释放内存
		static void* deallocate(void* p, size_t size)
		{
			obj* q = (obj*)p;
			void** volatile my_free_list;
			// 超过__MAX_BYTES，交给一级配置器释放
			if (size > (size_t)__MAX_BYTES)
			{
				malloc_alloc::deallocate(p, size);
				return;
			}
			my_free_list = free_lists + FREELIST_INDEX(size);	// 获取对应链表的头节点的地址
			// 插入链表，头插
			q->free_list_link = *my_free_list;
			*my_free_list = q;
		}
		// 修改大小
		static void* reallocate(void* p, size_t old_sz, size_t new_sz);
	}; // end-of-__default_alloc-template



	// 实现refill
	template<bool threads, int inst>
	void* __default_alloc_template<threads, inst>::refill(size_t n)
	{
		int nobjs = 20;	// 缺省取得20个对象
		char* chunk = chunk_alloc(n, nobjs);	// 调用chunk_alloc从内存池中申请空间,chunk类型为char*
		obj* volatile* my_free_list;
		void* result;

		obj* current_obj, * next_obj;
		// 只申请到一个区块
		if (nobjs == 1)
			return chunk;
		my_free_list = free_lists + FREELIST_INDEX(n);	// 找到对应自由链表的地址
		result = (obj*)chunk;	// 返回一块空间给allocate，其他空间连接到链表上
		*my_free_list = next_obj = (obj*)(chunk + n); // +n指向第二个区块的首地址
		for (int i = 1;; i++)
		{
			current_obj = next_obj;
			next_obj = (obj*)((char*)next_obj + n);	// 向后走，next_obj指向下一个对象
			if (i == nobjs - 1)
			{
				current_obj->free_list_link = nullptr;
				break;
			}
			else
			{
				current_obj->free_list_link = next_obj;
			}
		}
		return result;
	}

	// 实现chunk_alloc
	template<bool threads, int inst>
	char* __default_alloc_template<threads, inst>::chunk_alloc(size_t size, int& nobjs)
	{
		char* result;
		size_t total_bytes = nobjs * size;	//	需要申请的字节数
		size_t bytes_left = end_free - start_free; // 剩余的字节数

		// 剩余的字节数大于等于申请的字节数
		if (bytes_left >= total_bytes)
		{
			result = start_free;
			start_free += total_bytes;	// 更新内存池起始地址
			return result;
		}
		// 剩余的字节数不够全部空间,但是至少够一个区块
		else if (bytes_left >= size)
		{
			nobjs = (int)bytes_left / size;
			total_bytes = nobjs * size;	// 可以申请到的总字节数
			result = start_free;
			start_free += total_bytes;	// 更新内存池起始地址
			return result;
		}
		// 剩余的字节数连一个区块都不够分配，先将内存池中的空间全部利用完，再向堆中申请空间
		else
		{
			size_t bytes_to_get = total_bytes + ROUND_UP(heap_size >> 4);
			// 将内存池中剩余的空间编入自由链表中
			if (bytes_left > 0)
			{
				obj* volatile* my_free_list = free_lists + FREELIST_INDEX(bytes_left);
				((obj*)start_free)->free_list_link = *my_free_list;
				*my_free_list = (obj*)start_free;
			}

			//向堆中申请空间
			start_free = (char*)malloc(bytes_to_get);

			// 从堆中申请空间失败
			if (start_free == nullptr)
			{
				obj* volatile* my_free_list;
				obj* p;
				// 查询其他链表，如果其他链表有数据，就使用其他链表的区块
				for (size_t i = size; i <= (size_t)__MAX_BYTES; i += (size_t)__ALIGN)
				{
					my_free_list = free_lists + FREELIST_INDEX(i);
					p = *my_free_list;
					if (p)
					{
						*my_free_list = p->free_list_link;
						start_free = (char*)p;
						end_free = start_free + size;
						return chunk_alloc(size, nobjs);	// 空间已分配好，再次调用chunk_alloc申请资源
					}
				}
				// 其他链表也为空，即一点资源都没有了，堆上也申请不到资源
				end_free = nullptr;
				start_free = (char*)malloc_alloc::allocate(bytes_to_get); //调用一级配置器申请空间，主要是调用一级配置器中的内存不足处理函数
			}

			// 申请成功
			heap_size += bytes_to_get;				// 更新堆大小
			end_free = start_free + bytes_to_get;	// 更新内存池指针
			return chunk_alloc(size, nobjs);	// 空间已分配好，再次调用chunk_alloc申请资源
		}
	}

	// 实现reallocate
	template<bool threads, int inst>
	static void* __default_alloc_template<threads, inst>::reallocate(void* p, size_t old_sz, size_t new_sz)
	{
		void* result;
		size_t copy_sz;	// 
		if (old_sz > (size_t)__MAX_BYTES && new_sz > (size_t)__MAX_BYTES)	// 如果两个大小都是大于MAX_BYTES，直接调用realloc
			return realloc(p, new_sz);

		if (ROUND_UP(old_sz) == ROUND_UP(new_sz))	// 如果两个空间的大小都是在同一个区间，直接返回p
			return p;

		result = allocate(new_sz);						// 申请new_sz大小的空间
		copy_sz = new_sz > old_sz ? old_sz : new_sz;	// 需要拷贝数据的大小
		memcpy(result, p, copy_sz);						// 将原在p中的数据拷贝到新空间中
		deallocate(p, old_sz);							// 释放掉旧数据
		return result;
	}


	// 对static data member定义和数值初始化
	template<bool threads, int inst>
	char* __default_alloc_template<threads, inst>::start_free = nullptr;
	template<bool threads, int inst>
	char* __default_alloc_template<threads, inst>::end_free = nullptr;
	template<bool threads, int inst>
	size_t __default_alloc_template<threads, inst>::heap_size = 0;
	template<bool threads, int inst>
	typename __default_alloc_template<threads, inst>::obj* volatile __default_alloc_template<threads, inst>::free_lists[__NFREELISTS] = { 0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0 };
	// 加个typename就可以编译通过
}
