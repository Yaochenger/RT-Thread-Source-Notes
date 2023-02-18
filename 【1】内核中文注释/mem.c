/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2008-7-12      Bernard      the first version
 * 2010-06-09     Bernard      fix the end stub of heap
 *                             fix memory check in rt_realloc function
 * 2010-07-13     Bernard      fix RT_ALIGN issue found by kuronca
 * 2010-10-14     Bernard      fix rt_realloc issue when realloc a NULL pointer.
 * 2017-07-14     armink       fix rt_realloc issue when new size is 0
 * 2018-10-02     Bernard      Add 64bit support
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *         Simon Goldschmidt
 *
 */

#include <rthw.h>
#include <rtthread.h>

#if defined (RT_USING_SMALL_MEM)
 /**
  * memory item on the small mem
  */
/* �����С�ڴ�Ŀ����Ϣ */
struct rt_small_mem_item
{
    /* С�ڴ����ĵ�ַ */
    rt_ubase_t              pool_ptr;         /**< small memory object addr */
    /* ��һ�������ڴ�ĵ�ַƫ���� */
    rt_size_t               next;             /**< next free item */
    /* ��һ�������ڴ�ĵ�ַƫ���� */
    rt_size_t               prev;             /**< prev free item */
};

/**
 * Base structure of small memory object
 */
/* ������ڴ�����Ϣ */
struct rt_small_mem
{
    /* �̳���rt_memory */
    struct rt_memory            parent;                 /**< inherit from rt_memory */
    /* �ڴ�ѵ���ʼ��ַ */
    rt_uint8_t                 *heap_ptr;               /**< pointer to the heap */
    /* �ڴ�ѽ�β������ͷ��ַ */
    struct rt_small_mem_item   *heap_end;
    /* ָ���һ�����е��ڴ�������ͷ */
    struct rt_small_mem_item   *lfree;
    /* �����������ڴ� */
    rt_size_t                   mem_size_aligned;       /**< aligned memory size */
};

#define HEAP_MAGIC 0x1ea0
/* ����������С���ֽ��� */
#define MIN_SIZE 12

#define MEM_MASK             0xfffffffe
#define MEM_USED()         ((((rt_base_t)(small_mem)) & MEM_MASK) | 0x1)
#define MEM_FREED()        ((((rt_base_t)(small_mem)) & MEM_MASK) | 0x0)
#define MEM_ISUSED(_mem)   \
                      (((rt_base_t)(((struct rt_small_mem_item *)(_mem))->pool_ptr)) & (~MEM_MASK))
#define MEM_POOL(_mem)     \
    ((struct rt_small_mem *)(((rt_base_t)(((struct rt_small_mem_item *)(_mem))->pool_ptr)) & (MEM_MASK)))
#define MEM_SIZE(_heap, _mem)      \
    (((struct rt_small_mem_item *)(_mem))->next - ((rt_ubase_t)(_mem) - \
    (rt_ubase_t)((_heap)->heap_ptr)) - RT_ALIGN(sizeof(struct rt_small_mem_item), RT_ALIGN_SIZE))
/* �������������С�Ĵ����4�ֽڵĶ��� */
#define MIN_SIZE_ALIGNED     RT_ALIGN(MIN_SIZE, RT_ALIGN_SIZE)
#define SIZEOF_STRUCT_MEM    RT_ALIGN(sizeof(struct rt_small_mem_item), RT_ALIGN_SIZE)

/* �ڴ��ͷ�ʱ�ڵ�ϲ� ���� */
static void plug_holes(struct rt_small_mem *m, struct rt_small_mem_item *mem)
{
    struct rt_small_mem_item *nmem; /* ��һ��С�ڴ�Ľڵ���Ϣ  */
    struct rt_small_mem_item *pmem; /* ��һ��С�ڴ�Ľڵ���Ϣ  */

    RT_ASSERT((rt_uint8_t *)mem >= m->heap_ptr);
    RT_ASSERT((rt_uint8_t *)mem < (rt_uint8_t *)m->heap_end);

    /* plug hole forward */
    nmem = (struct rt_small_mem_item *)&m->heap_ptr[mem->next];/* ��ȡ��ǰ�ڴ�����һ�������ڴ��ĵ�ַ */
    if (mem != nmem && !MEM_ISUSED(nmem) &&
        (rt_uint8_t *)nmem != (rt_uint8_t *)m->heap_end)
    {
        /* if mem->next is unused and not end of m->heap_ptr,
         * combine mem and mem->next
         */
        if (m->lfree == nmem)
        {
            m->lfree = mem;
        }
        nmem->pool_ptr = 0;
        mem->next = nmem->next;
        ((struct rt_small_mem_item *)&m->heap_ptr[nmem->next])->prev = (rt_uint8_t *)mem - m->heap_ptr;
    }

    /* plug hole backward */
    pmem = (struct rt_small_mem_item *)&m->heap_ptr[mem->prev];
    if (pmem != mem && !MEM_ISUSED(pmem))
    {
        /* if mem->prev is unused, combine mem and mem->prev */
        if (m->lfree == mem)
        {
            m->lfree = pmem;
        }
        mem->pool_ptr = 0;
        pmem->next = mem->next;
        ((struct rt_small_mem_item *)&m->heap_ptr[mem->next])->prev = (rt_uint8_t *)pmem - m->heap_ptr;
    }
}

/**
 * @brief This function will initialize small memory management algorithm.
 *
 * @param m the small memory management object.
 *
 * @param name is the name of the small memory management object.
 *
 * @param begin_addr the beginning address of memory.
 *
 * @param size is the size of the memory.
 *
 * @return Return a pointer to the memory object. When the return value is RT_NULL, it means the init failed.
 */
/* С�ڴ�����ʼ�� *//* */
rt_smem_t rt_smem_init(const char    *name,
                     void          *begin_addr,
                     rt_size_t      size)
{
    /* С�ڴ����Ϣ */
    struct rt_small_mem_item *mem;
    /* ������ڴ����Ϣ */
    struct rt_small_mem *small_mem;
    /* */
    rt_ubase_t start_addr, begin_align, end_align, mem_size;
    /* ������ڴ��ĵ�ַ ���׵�ַ����4�ֽڶ��� */
    small_mem = (struct rt_small_mem *)RT_ALIGN((rt_ubase_t)begin_addr, RT_ALIGN_SIZE);
    /* ����һ���� ȥ��small_memͷ  */
    /* �ų�small_mem����ͷ�� ʵ�ʿ���ʹ�õ��ڴ�ʹ�õ�ַ */
    start_addr = (rt_ubase_t)small_mem + sizeof(*small_mem);

    /* ��ʵ�ʵ�ʹ���ڴ��ַ����4�ֽڶ��� */
    begin_align = RT_ALIGN((rt_ubase_t)start_addr, RT_ALIGN_SIZE);
    /* ��ʵ�ʵ�ʹ���ڴ�Ľ�����ַ�����ֽڶ��� */
    end_align   = RT_ALIGN_DOWN((rt_ubase_t)begin_addr + size, RT_ALIGN_SIZE);

    /* �ж϶����ĵ�ַ�Ƿ���ȷ
     * 1 �жϽ�����ַ�Ƿ������������ͷ�Ĵ�С
     * 2 �жϽ�����ַ����ʼ��ַ�Ƿ���������������ͷ */
    if ((end_align > (2 * SIZEOF_STRUCT_MEM)) && ((end_align - 2 * SIZEOF_STRUCT_MEM) >= start_addr))
    {
        /* �����ַ������ʹ�õ��ڴ�Ĵ�С
         * ������ַ -��ʼ��ַ -2������ͷ����һ�����ݿ������Դͷ ���ڴ�ĩ�˵�����ͷ ��
         * ��ʼ��������ʵ�ʿ���ʹ�õĵ�ַ*/
        mem_size = end_align - begin_align - 2 * SIZEOF_STRUCT_MEM;
    }
    else
    {
        /* �ڴ��ʼ��ʧ��  ��������ʼ��ַ�������ַ */
        rt_kprintf("mem init, error begin address 0x%x, and end address 0x%x\n",
                   (rt_ubase_t)begin_addr, (rt_ubase_t)begin_addr + size);

        return RT_NULL;
    }
    /* ��ʼ���ڴ� */
    rt_memset(small_mem, 0, sizeof(*small_mem));
    /* ��ʼ���ڴ���� */
    rt_object_init(&(small_mem->parent.parent), RT_Object_Class_Memory, name);
    /* �����㷨���� */
    small_mem->parent.algorithm = "small";
    /* �����ڴ���ʼ��ַ ��ȥrt_small_memͷ */
    small_mem->parent.address = begin_align;
    /* �����ڴ����� ��ȥrt_small_memͷ ��һ��item������ͷ �ڴ������item����ͷ  */
    small_mem->parent.total = mem_size;
    /* �����ڴ����� ��ȥrt_small_memͷ ��һ��item������ͷ �ڴ������item����ͷ  ��ʼ���������ڴ��ʹ���� */
    small_mem->mem_size_aligned = mem_size;
    /* ָ��ѵ���ʼ��ַ  */
    small_mem->heap_ptr = (rt_uint8_t *)begin_align;

    RT_DEBUG_LOG(RT_DEBUG_MEM, ("mem init, heap begin address 0x%x, size %d\n",
                                (rt_ubase_t)small_mem->heap_ptr, small_mem->mem_size_aligned));
    /* �ѵ���ʼ��ַ Ҳ�ǵ�һ�����п�����ͷ����ʼ��ַ  */
    mem        = (struct rt_small_mem_item *)small_mem->heap_ptr;
    /*
     * #define MEM_FREED()        ((((rt_base_t)(small_mem)) & 0xfffffffe) | 0x0)*/
    /* ��ʼ��С�ڴ���������ͷ ��ǵ�һ������ͷ�ǿ��е� */
    mem->pool_ptr = MEM_FREED();
    /*ָ�����һ��С�ڴ���������ͷ�ĵ�ַƫ����(����)����ʼʱ��û�� ֱ��ָ�����������ͷ*/
    mem->next  = small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM;
    mem->prev  = 0;

    /* ��ʼ���ѽ���������ͷ */
    small_mem->heap_end        = (struct rt_small_mem_item *)&small_mem->heap_ptr[mem->next];
    /* �ѽ���������ͷ ������������ ���Ա��Ϊ��ʹ�� */
    small_mem->heap_end->pool_ptr = MEM_USED();
    /* ����Ϊ�ѽ���������ͷ�ĵ�ַ */
    small_mem->heap_end->next  = small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM;
    /* ����Ϊ�ѽ���������ͷ�ĵ�ַ */
    small_mem->heap_end->prev  = small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM;

    /* lfreeָ���һ�����е�С�ڴ�� ��ʼ��Ϊ��һ���������ݵ�����ͷ */
    small_mem->lfree = (struct rt_small_mem_item *)small_mem->heap_ptr;

    return &small_mem->parent;
}
RTM_EXPORT(rt_smem_init);

/**
 * @brief This function will remove a small mem from the system.
 *
 * @param m the small memory management object.
 *
 * @return RT_EOK
 */
/*�˹��ܽ���ϵͳ��ɾ��һ��С�ڴ� */
rt_err_t rt_smem_detach(rt_smem_t m)
{
    RT_ASSERT(m != RT_NULL);
    RT_ASSERT(rt_object_get_type(&m->parent) == RT_Object_Class_Memory);
    RT_ASSERT(rt_object_is_systemobject(&m->parent));
    /* ��С�ڴ�����㷨�Ķ������� */
    rt_object_detach(&(m->parent));

    return RT_EOK;
}
RTM_EXPORT(rt_smem_detach);

/**
 * @addtogroup MM
 */

/**@{*/

/**
 * @brief Allocate a block of memory with a minimum of 'size' bytes.
 *
 * @param m the small memory management object.
 *
 * @param size is the minimum size of the requested block in bytes.
 *
 * @return the pointer to allocated memory or NULL if no free memory was found.
 */
/* ʹ��С�ڴ��㷨�����ڴ� */
void *rt_smem_alloc(rt_smem_t m, rt_size_t size)
{
    rt_size_t ptr, ptr2;
    struct rt_small_mem_item *mem, *mem2;
    struct rt_small_mem *small_mem;
    /* ����������ڴ� */
    if (size == 0)
        return RT_NULL;

    RT_ASSERT(m != RT_NULL);
    RT_ASSERT(rt_object_get_type(&m->parent) == RT_Object_Class_Memory);
    RT_ASSERT(rt_object_is_systemobject(&m->parent));

    /* ������������ڴ�Ĵ�С����4�ֽڶ��� */
    if (size != RT_ALIGN(size, RT_ALIGN_SIZE))
    {
        /* ����������ִ�� �������Ͳ�ִ�� */
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("malloc size %d, but align to %d\n",
                                    size, RT_ALIGN(size, RT_ALIGN_SIZE)));
    }
    else
    {
        /* ����������ִ�� �������Ͳ�ִ�� */
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("malloc size %d\n", size));
    }
    /* ����С�ڴ���������ͷ����ֵ�� */
    small_mem = (struct rt_small_mem *)m;
    /* ����������ڴ�Ĵ�С ������������Ĵ�С����4�ֽڶ��� */
    size = RT_ALIGN(size, RT_ALIGN_SIZE);
    /* ÿ�����ݿ�ĳ��ȱ�������ΪMIN_SIZE_ALIGNED Ĭ��12���ֽ� */
    if (size < MIN_SIZE_ALIGNED)
        size = MIN_SIZE_ALIGNED;
    /* �ж�������ڴ�Ĵ�С �Ƿ񳬹� mem_size_alignedΪ�ɹ�������������  */
    if (size > small_mem->mem_size_aligned)
    {
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("no memory\n"));

        return RT_NULL;
    }
    /*
         * δ���� �ɹ�������������
         * ����ÿһ��item */
    /* (1)ptr = (rt_uint8_t *)small_mem->lfree - small_mem->heap_ptr
     * 1 ��һ�����п������ͷ��ַ -  ����ʼ��ַ
     * 2 �� ptr С�ڵ��� ʣ���ڴ�
     * 3 ������һ�� С�ڴ���� */

    /* (2)ptr <= small_mem->mem_size_aligned - size
     * ����������ڼ��ô�������ڴ��Ƿ�ᳬ��ʣ����ڴ� */

    /* (3)ptr = ((struct rt_small_mem_item *)&small_mem->heap_ptr[ptr])->next
     * ��ɨ���ָ��������һ��С�ڴ���ƿ�  */

    for (ptr = (rt_uint8_t *)small_mem->lfree - small_mem->heap_ptr;
         ptr <= small_mem->mem_size_aligned - size;
         ptr = ((struct rt_small_mem_item *)&small_mem->heap_ptr[ptr])->next)
    {
        /* ��һ������ ��һ��С�ڴ��Ϊ����� *//* ptr=0 */
        mem = (struct rt_small_mem_item *)&small_mem->heap_ptr[ptr];
        /* �ҵ����п� ����������п�Ĵ�СҪ������Ĵ� ִ�з���
         * mem->next = small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM */
        /* �����ǰ��item��������ڴ���Ҫ����Ŀռ������������Ϣ�Ŀռ���Ǿͷ������� */
        if ((!MEM_ISUSED(mem)) && (mem->next - (ptr + SIZEOF_STRUCT_MEM)) >= size)
        {
            /* �����ǰ��item��������ڴ���Ҫ����Ŀռ������������Ϣ�Ŀռ��Ķ�  */
            if (mem->next - (ptr + SIZEOF_STRUCT_MEM) >= (size + SIZEOF_STRUCT_MEM + MIN_SIZE_ALIGNED))
            {
                /* �ָ� */
                /*ptr2 ָ��ǰ��Ϣ���ʵ���ڴ���ģ���һ�����п�Ҫд���Ӧ����Ϣ�飩*/
                ptr2 = ptr + SIZEOF_STRUCT_MEM + size;
                /* �ղŵ��ڴ�鱻ʹ���� ����һ�����ڴ�� mem2 */
                mem2       = (struct rt_small_mem_item *)&small_mem->heap_ptr[ptr2];
                /* ״̬���Ϊ���� */
                mem2->pool_ptr = MEM_FREED();
                /* mem2ָ�����һ���ڴ�� ָ��֮ǰmemָ����ڴ�� */
                mem2->next = mem->next;
                /* ǰһ����ַָ�� ֮ǰ���ڴ��*/
                mem2->prev = ptr;

                /* mem��֮ǰָ���mem->next ָ��ptr2 */
                mem->next = ptr2;
                /* ��*/
                if (mem2->next != small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM)
                {
                    ((struct rt_small_mem_item *)&small_mem->heap_ptr[mem2->next])->prev = ptr2;
                }
                /* ��¼��ʹ�õ��ڴ��С  */
                small_mem->parent.used += (size + SIZEOF_STRUCT_MEM);
                if (small_mem->parent.max < small_mem->parent.used)
                    small_mem->parent.max = small_mem->parent.used;
            }
            else
            {
                /* ���ָ� ֱ�ӷ���*/
                small_mem->parent.used += mem->next - ((rt_uint8_t *)mem - small_mem->heap_ptr);
                if (small_mem->parent.max < small_mem->parent.used)
                    small_mem->parent.max = small_mem->parent.used;
            }
        /* ���֮ǰ�Ŀ����ڴ��ѱ�ʹ�� */
        mem->pool_ptr = MEM_USED();

        if (mem == small_mem->lfree)
        {
            /* ����mem֮�����һ�����п鲢������Ϳ���ָ�� */
            while (MEM_ISUSED(small_mem->lfree) && small_mem->lfree != small_mem->heap_end)
                small_mem->lfree = (struct rt_small_mem_item *)&small_mem->heap_ptr[small_mem->lfree->next];

            RT_ASSERT(((small_mem->lfree == small_mem->heap_end) || (!MEM_ISUSED(small_mem->lfree))));
        }
        RT_ASSERT((rt_ubase_t)mem + SIZEOF_STRUCT_MEM + size <= (rt_ubase_t)small_mem->heap_end);
        RT_ASSERT((rt_ubase_t)((rt_uint8_t *)mem + SIZEOF_STRUCT_MEM) % RT_ALIGN_SIZE == 0);
        RT_ASSERT((((rt_ubase_t)mem) & (RT_ALIGN_SIZE - 1)) == 0);

        RT_DEBUG_LOG(RT_DEBUG_MEM,
                     ("allocate memory at 0x%x, size: %d\n",
                      (rt_ubase_t)((rt_uint8_t *)mem + SIZEOF_STRUCT_MEM),
                      (rt_ubase_t)(mem->next - ((rt_uint8_t *)mem - small_mem->heap_ptr))));

        /* ���س�mem�ṹ֮����ڴ����� */
        return (rt_uint8_t *)mem + SIZEOF_STRUCT_MEM;
        }
    }
    return RT_NULL;
}
RTM_EXPORT(rt_smem_alloc);

/**
 * @brief This function will change the size of previously allocated memory block.
 *
 * @param m the small memory management object.
 *
 * @param rmem is the pointer to memory allocated by rt_mem_alloc.
 *
 * @param newsize is the required new size.
 *
 * @return the changed memory block address.
 */
void *rt_smem_realloc(rt_smem_t m, void *rmem, rt_size_t newsize)
{
    rt_size_t size;
    rt_size_t ptr, ptr2;
    struct rt_small_mem_item *mem, *mem2;
    struct rt_small_mem *small_mem;
    void *nmem;

    RT_ASSERT(m != RT_NULL);
    RT_ASSERT(rt_object_get_type(&m->parent) == RT_Object_Class_Memory);
    RT_ASSERT(rt_object_is_systemobject(&m->parent));

    small_mem = (struct rt_small_mem *)m;
    /* alignment size */
    newsize = RT_ALIGN(newsize, RT_ALIGN_SIZE);
    if (newsize > small_mem->mem_size_aligned)
    {
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("realloc: out of memory\n"));

        return RT_NULL;
    }
    else if (newsize == 0)
    {
        rt_smem_free(rmem);
        return RT_NULL;
    }

    /* allocate a new memory block */
    if (rmem == RT_NULL)
        return rt_smem_alloc(&small_mem->parent, newsize);

    RT_ASSERT((((rt_ubase_t)rmem) & (RT_ALIGN_SIZE - 1)) == 0);
    RT_ASSERT((rt_uint8_t *)rmem >= (rt_uint8_t *)small_mem->heap_ptr);
    RT_ASSERT((rt_uint8_t *)rmem < (rt_uint8_t *)small_mem->heap_end);

    mem = (struct rt_small_mem_item *)((rt_uint8_t *)rmem - SIZEOF_STRUCT_MEM);

    /* current memory block size */
    ptr = (rt_uint8_t *)mem - small_mem->heap_ptr;
    size = mem->next - ptr - SIZEOF_STRUCT_MEM;
    if (size == newsize)
    {
        /* the size is the same as */
        return rmem;
    }

    if (newsize + SIZEOF_STRUCT_MEM + MIN_SIZE < size)
    {
        /* split memory block */
        small_mem->parent.used -= (size - newsize);

        ptr2 = ptr + SIZEOF_STRUCT_MEM + newsize;
        mem2 = (struct rt_small_mem_item *)&small_mem->heap_ptr[ptr2];
        mem2->pool_ptr = MEM_FREED();
        mem2->next = mem->next;
        mem2->prev = ptr;
        mem->next = ptr2;
        if (mem2->next != small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM)
        {
            ((struct rt_small_mem_item *)&small_mem->heap_ptr[mem2->next])->prev = ptr2;
        }

        if (mem2 < small_mem->lfree)
        {
            /* the splited struct is now the lowest */
            small_mem->lfree = mem2;
        }

        plug_holes(small_mem, mem2);

        return rmem;
    }

    /* expand memory */
    nmem = rt_smem_alloc(&small_mem->parent, newsize);
    if (nmem != RT_NULL) /* check memory */
    {
        rt_memcpy(nmem, rmem, size < newsize ? size : newsize);
        rt_smem_free(rmem);
    }

    return nmem;
}
RTM_EXPORT(rt_smem_realloc);

/**
 * @brief This function will release the previously allocated memory block by
 *        rt_mem_alloc. The released memory block is taken back to system heap.
 *
 * @param rmem the address of memory which will be released.
 */
/* С�ڴ��ͷ� */ /* */
void rt_smem_free(void *rmem)
{
    /* С�ڴ�ڵ�  */
    struct rt_small_mem_item *mem;

    struct rt_small_mem *small_mem;

    if (rmem == RT_NULL)
        return;

    RT_ASSERT((((rt_ubase_t)rmem) & (RT_ALIGN_SIZE - 1)) == 0);

    /* Get the corresponding struct rt_small_mem_item ... */
    mem = (struct rt_small_mem_item *)((rt_uint8_t *)rmem - SIZEOF_STRUCT_MEM);
    /* ... which has to be in a used state ... */
    small_mem = MEM_POOL(mem);
    RT_ASSERT(small_mem != RT_NULL);
    RT_ASSERT(MEM_ISUSED(mem));
    RT_ASSERT(rt_object_get_type(&small_mem->parent.parent) == RT_Object_Class_Memory);
    RT_ASSERT(rt_object_is_systemobject(&small_mem->parent.parent));
    RT_ASSERT((rt_uint8_t *)rmem >= (rt_uint8_t *)small_mem->heap_ptr &&
              (rt_uint8_t *)rmem < (rt_uint8_t *)small_mem->heap_end);
    RT_ASSERT(MEM_POOL(&small_mem->heap_ptr[mem->next]) == small_mem);

    RT_DEBUG_LOG(RT_DEBUG_MEM,
                 ("release memory 0x%x, size: %d\n",
                  (rt_ubase_t)rmem,
                  (rt_ubase_t)(mem->next - ((rt_uint8_t *)mem - small_mem->heap_ptr))));

    /* ... and is now unused. */
    mem->pool_ptr = MEM_FREED();

    if (mem < small_mem->lfree)
    {
        /* the newly freed struct is now the lowest */
        small_mem->lfree = mem;
    }

    small_mem->parent.used -= (mem->next - ((rt_uint8_t *)mem - small_mem->heap_ptr));

    /* finally, see if prev or next are free also */
    plug_holes(small_mem, mem);
}
RTM_EXPORT(rt_smem_free);

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif /* RT_USING_FINSH */

#endif /* defined (RT_USING_SMALL_MEM) */

/**@}*/
