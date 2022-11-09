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
	// ��һ���ռ�������
	template<int inst>
	class __malloc_alloc_template
	{
	private:
		// out-of-memory
		static void* oom_malloc(size_t n);	// ����malloc out-of-memory
		static void* oom_realloc(void*, size_t);	// ����realloc out-of-memory
		static void (*__malloc_alloc_oom_handler)();	// ����out-of- memory;

	public:
		// ����ռ�
		static void* allocate(size_t n)
		{
			void* result = malloc(n);	// ��һ��������ֱ��malloc
			if (result == nullptr)
				result = oom_malloc(n);	// mallocʧ�ܵ��ö�Ӧ��mallocʧ�ܴ�����
			return result;
		}

		// �ͷſռ�
		static void* deallocate(void* p)
		{
			free(p);	// ��һ��������ֱ��free
		}

		// �����ռ�
		static void* reallocate(void* p, size_t new_size)
		{
			void* result = realloc(p, new_size);
			if (result == nullptr)
				result = oom_realloc(p, new_size);	// reallocʧ�ܵ��ö�Ӧ��reallocʧ�ܴ�����
			return result;
		}

		// ����ʵ��c++��new_handler, ָ���Լ���out-of-memory handler
		// hook���� ��__malloc_alloc_oom_handler��������ת�����f
		static void (*__set_malloc_handler(void(*f)()))()	// ���������� void(*)() __set_malloc_alloc_oom_handler(void(*f)())
		{													// ����ֵΪ����ָ�룬����Ҳ�Ǻ���ָ��
			void(*old)() = __malloc_alloc_oom_handler;
			__malloc_alloc_oom_handler = f;
			return old;
		}
	};// end-of-__malloc_alloc_template


	// malloc_alloc out-of-memory handling
	template<int inst>
	void(*__malloc_alloc_template<inst>::__malloc_alloc_oom_handler)() = nullptr;	// �ÿ�

	// malloc out-memory-handlerʵ��
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
			(*my_malloc_handler)();	// �����ڴ洦����
			result = malloc(n);	//��������ռ�
			if (result)
				return result;
		}
	}

	// realloc out-of-memoryʵ��
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

	// ����ֱ�ӽ�����ָ��Ϊ0
	typedef __malloc_alloc_template<0> malloc_alloc;	// typedefһ���������


	enum { __ALIGN = 8 };	// С��������ϵ��߽�
	enum { __MAX_BYTES = 128 };	// С�����������
	enum { __NFREELISTS = __MAX_BYTES / __ALIGN };	// free����lists������

												// �ڶ����ռ�������
	// ��һ�������ڶ��̻߳��������Ǳ��鲻���۶��̻߳���
	template<bool threads, int inst>
	class __default_alloc_template
	{
	private:
		union obj	// ��������ڵ�
		{
			union obj* free_list_link;	// objָ�룬���Ӻ����ڵ�
			char client_data[1];	// 
		};
	private:
		static obj* volatile free_lists[__NFREELISTS]; // ����������������

		// �ҵ�����bytes����С��8�ı���
		static size_t ROUND_UP(size_t bytes)
		{
			return (bytes + __ALIGN - 1) & (~(__ALIGN - 1));
		}

		// �Ҷ�Ӧ���������
		static size_t FREELIST_INDEX(size_t bytes)
		{
			return (bytes + __ALIGN - 1) / __ALIGN - 1;
		}

		//����һ����СΪn�Ķ��󣬲����ܼ����СΪn�����鵽����free list,ȱʡ���20�����ܻ����
		static void* refill(size_t n);

		// ����һ���ռ䣬������nobjs*size�����飬�������nobjs������ʧ�ܣ�nobjs���ܻᷢ���ı�
		static char* chunk_alloc(size_t size, int& nobjs);

		// chunk allocation state
		static char* start_free;	// �ڴ����ʼλ�ã� ֻ��chunk_alloc�з����仯
		static char* end_free;		// �ڴ�ؽ���λ�ã�ּ��chunk_alloc�з����仯
		static size_t heap_size;	// �ѵĴ�С
	public:
		// �����ڴ�
		static void* allocate(size_t n)
		{
			obj* volatile* my_free_list;	// ��Ӧ���������ͷָ��
			obj* result;					// ���뵽��ַָ��

			// ����__MAX_BYTES,ȥ����һ��������
			if (n > size_t(__MAX_BYTES))
			{
				return malloc_alloc::allocate(n);
			}
			my_free_list = free_lists + FREELIST_INDEX(n); // �ҵ���Ӧ������׵�ַ
			result = *my_free_list;		// �����Ӧ�����ͷ�ڵ��ֵ
			// ��Ӧ����ͷΪ��
			if (result == nullptr)
			{
				void* r = refill(ROUND_UP(n)); // �����Ӧ����Ϊ�գ�����refillΪ�����������ռ䣬�¿ռ������ڴ��(ͨ��chunk_alloc)
				return r;
			}
			else
			{
				return result;
			}
		}

		// �ͷ��ڴ�
		static void* deallocate(void* p, size_t size)
		{
			obj* q = (obj*)p;
			void** volatile my_free_list;
			// ����__MAX_BYTES������һ���������ͷ�
			if (size > (size_t)__MAX_BYTES)
			{
				malloc_alloc::deallocate(p, size);
				return;
			}
			my_free_list = free_lists + FREELIST_INDEX(size);	// ��ȡ��Ӧ�����ͷ�ڵ�ĵ�ַ
			// ��������ͷ��
			q->free_list_link = *my_free_list;
			*my_free_list = q;
		}
		// �޸Ĵ�С
		static void* reallocate(void* p, size_t old_sz, size_t new_sz);
	}; // end-of-__default_alloc-template



	// ʵ��refill
	template<bool threads, int inst>
	void* __default_alloc_template<threads, inst>::refill(size_t n)
	{
		int nobjs = 20;	// ȱʡȡ��20������
		char* chunk = chunk_alloc(n, nobjs);	// ����chunk_alloc���ڴ��������ռ�,chunk����Ϊchar*
		obj* volatile* my_free_list;
		void* result;

		obj* current_obj, * next_obj;
		// ֻ���뵽һ������
		if (nobjs == 1)
			return chunk;
		my_free_list = free_lists + FREELIST_INDEX(n);	// �ҵ���Ӧ��������ĵ�ַ
		result = (obj*)chunk;	// ����һ��ռ��allocate�������ռ����ӵ�������
		*my_free_list = next_obj = (obj*)(chunk + n); // +nָ��ڶ���������׵�ַ
		for (int i = 1;; i++)
		{
			current_obj = next_obj;
			next_obj = (obj*)((char*)next_obj + n);	// ����ߣ�next_objָ����һ������
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

	// ʵ��chunk_alloc
	template<bool threads, int inst>
	char* __default_alloc_template<threads, inst>::chunk_alloc(size_t size, int& nobjs)
	{
		char* result;
		size_t total_bytes = nobjs * size;	//	��Ҫ������ֽ���
		size_t bytes_left = end_free - start_free; // ʣ����ֽ���

		// ʣ����ֽ������ڵ���������ֽ���
		if (bytes_left >= total_bytes)
		{
			result = start_free;
			start_free += total_bytes;	// �����ڴ����ʼ��ַ
			return result;
		}
		// ʣ����ֽ�������ȫ���ռ�,�������ٹ�һ������
		else if (bytes_left >= size)
		{
			nobjs = (int)bytes_left / size;
			total_bytes = nobjs * size;	// �������뵽�����ֽ���
			result = start_free;
			start_free += total_bytes;	// �����ڴ����ʼ��ַ
			return result;
		}
		// ʣ����ֽ�����һ�����鶼�������䣬�Ƚ��ڴ���еĿռ�ȫ�������꣬�����������ռ�
		else
		{
			size_t bytes_to_get = total_bytes + ROUND_UP(heap_size >> 4);
			// ���ڴ����ʣ��Ŀռ��������������
			if (bytes_left > 0)
			{
				obj* volatile* my_free_list = free_lists + FREELIST_INDEX(bytes_left);
				((obj*)start_free)->free_list_link = *my_free_list;
				*my_free_list = (obj*)start_free;
			}

			//���������ռ�
			start_free = (char*)malloc(bytes_to_get);

			// �Ӷ�������ռ�ʧ��
			if (start_free == nullptr)
			{
				obj* volatile* my_free_list;
				obj* p;
				// ��ѯ������������������������ݣ���ʹ���������������
				for (size_t i = size; i <= (size_t)__MAX_BYTES; i += (size_t)__ALIGN)
				{
					my_free_list = free_lists + FREELIST_INDEX(i);
					p = *my_free_list;
					if (p)
					{
						*my_free_list = p->free_list_link;
						start_free = (char*)p;
						end_free = start_free + size;
						return chunk_alloc(size, nobjs);	// �ռ��ѷ���ã��ٴε���chunk_alloc������Դ
					}
				}
				// ��������ҲΪ�գ���һ����Դ��û���ˣ�����Ҳ���벻����Դ
				end_free = nullptr;
				start_free = (char*)malloc_alloc::allocate(bytes_to_get); //����һ������������ռ䣬��Ҫ�ǵ���һ���������е��ڴ治�㴦����
			}

			// ����ɹ�
			heap_size += bytes_to_get;				// ���¶Ѵ�С
			end_free = start_free + bytes_to_get;	// �����ڴ��ָ��
			return chunk_alloc(size, nobjs);	// �ռ��ѷ���ã��ٴε���chunk_alloc������Դ
		}
	}

	// ʵ��reallocate
	template<bool threads, int inst>
	static void* __default_alloc_template<threads, inst>::reallocate(void* p, size_t old_sz, size_t new_sz)
	{
		void* result;
		size_t copy_sz;	// 
		if (old_sz > (size_t)__MAX_BYTES && new_sz > (size_t)__MAX_BYTES)	// ���������С���Ǵ���MAX_BYTES��ֱ�ӵ���realloc
			return realloc(p, new_sz);

		if (ROUND_UP(old_sz) == ROUND_UP(new_sz))	// ��������ռ�Ĵ�С������ͬһ�����䣬ֱ�ӷ���p
			return p;

		result = allocate(new_sz);						// ����new_sz��С�Ŀռ�
		copy_sz = new_sz > old_sz ? old_sz : new_sz;	// ��Ҫ�������ݵĴ�С
		memcpy(result, p, copy_sz);						// ��ԭ��p�е����ݿ������¿ռ���
		deallocate(p, old_sz);							// �ͷŵ�������
		return result;
	}


	// ��static data member�������ֵ��ʼ��
	template<bool threads, int inst>
	char* __default_alloc_template<threads, inst>::start_free = nullptr;
	template<bool threads, int inst>
	char* __default_alloc_template<threads, inst>::end_free = nullptr;
	template<bool threads, int inst>
	size_t __default_alloc_template<threads, inst>::heap_size = 0;
	template<bool threads, int inst>
	typename __default_alloc_template<threads, inst>::obj* volatile __default_alloc_template<threads, inst>::free_lists[__NFREELISTS] = { 0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0 };
	// �Ӹ�typename�Ϳ��Ա���ͨ��
}
