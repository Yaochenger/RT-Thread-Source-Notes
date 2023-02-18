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
/* 申请的小内存的块的信息 */
struct rt_small_mem_item
{
    /* 小内存对象的地址 */
    rt_ubase_t              pool_ptr;         /**< small memory object addr */
    /* 下一个空闲内存的地址偏移量 */
    rt_size_t               next;             /**< next free item */
    /* 上一个空闲内存的地址偏移量 */
    rt_size_t               prev;             /**< prev free item */
};

/**
 * Base structure of small memory object
 */
/* 整体的内存块的信息 */
struct rt_small_mem
{
    /* 继承自rt_memory */
    struct rt_memory            parent;                 /**< inherit from rt_memory */
    /* 内存堆的起始地址 */
    rt_uint8_t                 *heap_ptr;               /**< pointer to the heap */
    /* 内存堆结尾的数据头地址 */
    struct rt_small_mem_item   *heap_end;
    /* 指向第一个空闲的内存块的数据头 */
    struct rt_small_mem_item   *lfree;
    /* 可申请的最大内存 */
    rt_size_t                   mem_size_aligned;       /**< aligned memory size */
};

#define HEAP_MAGIC 0x1ea0
/* 允许分配的最小的字节数 */
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
/* 将允许申请的最小的存进行4字节的对齐 */
#define MIN_SIZE_ALIGNED     RT_ALIGN(MIN_SIZE, RT_ALIGN_SIZE)
#define SIZEOF_STRUCT_MEM    RT_ALIGN(sizeof(struct rt_small_mem_item), RT_ALIGN_SIZE)

/* 内存释放时节点合并 函数 */
static void plug_holes(struct rt_small_mem *m, struct rt_small_mem_item *mem)
{
    struct rt_small_mem_item *nmem; /* 下一个小内存的节点信息  */
    struct rt_small_mem_item *pmem; /* 上一个小内存的节点信息  */

    RT_ASSERT((rt_uint8_t *)mem >= m->heap_ptr);
    RT_ASSERT((rt_uint8_t *)mem < (rt_uint8_t *)m->heap_end);

    /* plug hole forward */
    nmem = (struct rt_small_mem_item *)&m->heap_ptr[mem->next];/* 获取当前内存块的下一个空闲内存块的地址 */
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
/* 小内存管理初始化 *//* */
rt_smem_t rt_smem_init(const char    *name,
                     void          *begin_addr,
                     rt_size_t      size)
{
    /* 小内存块信息 */
    struct rt_small_mem_item *mem;
    /* 整体的内存块信息 */
    struct rt_small_mem *small_mem;
    /* */
    rt_ubase_t start_addr, begin_align, end_align, mem_size;
    /* 整体的内存块的地址 对首地址进行4字节对齐 */
    small_mem = (struct rt_small_mem *)RT_ALIGN((rt_ubase_t)begin_addr, RT_ALIGN_SIZE);
    /* 《第一步》 去除small_mem头  */
    /* 排除small_mem数据头后 实际可以使用的内存使用地址 */
    start_addr = (rt_ubase_t)small_mem + sizeof(*small_mem);

    /* 将实际的使用内存地址进行4字节对齐 */
    begin_align = RT_ALIGN((rt_ubase_t)start_addr, RT_ALIGN_SIZE);
    /* 将实际的使用内存的结束地址进行字节对齐 */
    end_align   = RT_ALIGN_DOWN((rt_ubase_t)begin_addr + size, RT_ALIGN_SIZE);

    /* 判断对齐后的地址是否正确
     * 1 判断结束地址是否大于俩个数据头的大小
     * 2 判断结束地址到起始地址是否能容纳俩个数据头 */
    if ((end_align > (2 * SIZEOF_STRUCT_MEM)) && ((end_align - 2 * SIZEOF_STRUCT_MEM) >= start_addr))
    {
        /* 计算地址对齐后可使用的内存的大小
         * 结束地址 -起始地址 -2个数据头（第一个数据块的数据源头 与内存末端的数据头 ）
         * 初始化后最终实际可以使用的地址*/
        mem_size = end_align - begin_align - 2 * SIZEOF_STRUCT_MEM;
    }
    else
    {
        /* 内存初始化失败  并返回起始地址与结束地址 */
        rt_kprintf("mem init, error begin address 0x%x, and end address 0x%x\n",
                   (rt_ubase_t)begin_addr, (rt_ubase_t)begin_addr + size);

        return RT_NULL;
    }
    /* 初始化内存 */
    rt_memset(small_mem, 0, sizeof(*small_mem));
    /* 初始化内存对象 */
    rt_object_init(&(small_mem->parent.parent), RT_Object_Class_Memory, name);
    /* 设置算法名称 */
    small_mem->parent.algorithm = "small";
    /* 设置内存起始地址 除去rt_small_mem头 */
    small_mem->parent.address = begin_align;
    /* 设置内存容量 除去rt_small_mem头 第一个item的数据头 内存结束的item数据头  */
    small_mem->parent.total = mem_size;
    /* 设置内存容量 除去rt_small_mem头 第一个item的数据头 内存结束的item数据头  初始化后最大的内存可使用量 */
    small_mem->mem_size_aligned = mem_size;
    /* 指向堆的起始地址  */
    small_mem->heap_ptr = (rt_uint8_t *)begin_align;

    RT_DEBUG_LOG(RT_DEBUG_MEM, ("mem init, heap begin address 0x%x, size %d\n",
                                (rt_ubase_t)small_mem->heap_ptr, small_mem->mem_size_aligned));
    /* 堆的起始地址 也是第一个空闲块数据头的起始地址  */
    mem        = (struct rt_small_mem_item *)small_mem->heap_ptr;
    /*
     * #define MEM_FREED()        ((((rt_base_t)(small_mem)) & 0xfffffffe) | 0x0)*/
    /* 初始化小内存对象的数据头 标记第一个数据头是空闲的 */
    mem->pool_ptr = MEM_FREED();
    /*指向的下一个小内存对象的数据头的地址偏移量(阐述)：初始时还没有 直接指向结束的数据头*/
    mem->next  = small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM;
    mem->prev  = 0;

    /* 初始化堆结束的数据头 */
    small_mem->heap_end        = (struct rt_small_mem_item *)&small_mem->heap_ptr[mem->next];
    /* 堆结束的数据头 不含空闲数据 所以标记为已使用 */
    small_mem->heap_end->pool_ptr = MEM_USED();
    /* 设置为堆结束的数据头的地址 */
    small_mem->heap_end->next  = small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM;
    /* 设置为堆结束的数据头的地址 */
    small_mem->heap_end->prev  = small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM;

    /* lfree指向第一个空闲的小内存块 初始化为第一个空闲数据的数据头 */
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
/*此功能将从系统中删除一个小内存 */
rt_err_t rt_smem_detach(rt_smem_t m)
{
    RT_ASSERT(m != RT_NULL);
    RT_ASSERT(rt_object_get_type(&m->parent) == RT_Object_Class_Memory);
    RT_ASSERT(rt_object_is_systemobject(&m->parent));
    /* 将小内存管理算法的对象脱离 */
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
/* 使用小内存算法分配内存 */
void *rt_smem_alloc(rt_smem_t m, rt_size_t size)
{
    rt_size_t ptr, ptr2;
    struct rt_small_mem_item *mem, *mem2;
    struct rt_small_mem *small_mem;
    /* 期望分配的内存 */
    if (size == 0)
        return RT_NULL;

    RT_ASSERT(m != RT_NULL);
    RT_ASSERT(rt_object_get_type(&m->parent) == RT_Object_Class_Memory);
    RT_ASSERT(rt_object_is_systemobject(&m->parent));

    /* 将期望分配的内存的大小进行4字节对齐 */
    if (size != RT_ALIGN(size, RT_ALIGN_SIZE))
    {
        /* 开启调试则执行 不开启就不执行 */
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("malloc size %d, but align to %d\n",
                                    size, RT_ALIGN(size, RT_ALIGN_SIZE)));
    }
    else
    {
        /* 开启调试则执行 不开启就不执行 */
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("malloc size %d\n", size));
    }
    /* 设置小内存对象的数据头（赋值） */
    small_mem = (struct rt_small_mem *)m;
    /* 期望分配的内存的大小 并将期望分配的大小进行4字节对齐 */
    size = RT_ALIGN(size, RT_ALIGN_SIZE);
    /* 每个数据块的长度必须至少为MIN_SIZE_ALIGNED 默认12个字节 */
    if (size < MIN_SIZE_ALIGNED)
        size = MIN_SIZE_ALIGNED;
    /* 判断申请的内存的大小 是否超过 mem_size_aligned为可供申请的最大容量  */
    if (size > small_mem->mem_size_aligned)
    {
        RT_DEBUG_LOG(RT_DEBUG_MEM, ("no memory\n"));

        return RT_NULL;
    }
    /*
         * 未超过 可供申请的最大容量
         * 遍历每一个item */
    /* (1)ptr = (rt_uint8_t *)small_mem->lfree - small_mem->heap_ptr
     * 1 第一个空闲块的数据头地址 -  堆起始地址
     * 2 若 ptr 小于等于 剩余内存
     * 3 查找下一个 小内存对象 */

    /* (2)ptr <= small_mem->mem_size_aligned - size
     * 该条语句用于检查该次申请的内存是否会超出剩余的内存 */

    /* (3)ptr = ((struct rt_small_mem_item *)&small_mem->heap_ptr[ptr])->next
     * 将扫描的指针移向下一个小内存控制块  */

    for (ptr = (rt_uint8_t *)small_mem->lfree - small_mem->heap_ptr;
         ptr <= small_mem->mem_size_aligned - size;
         ptr = ((struct rt_small_mem_item *)&small_mem->heap_ptr[ptr])->next)
    {
        /* 第一次申请 第一个小内存块为申请块 *//* ptr=0 */
        mem = (struct rt_small_mem_item *)&small_mem->heap_ptr[ptr];
        /* 找到空闲块 并且这个空闲块的大小要比申请的大 执行分配
         * mem->next = small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM */
        /* 如果当前的item后面跟的内存块比要申请的空间加其他描述信息的空间大，那就符合条件 */
        if ((!MEM_ISUSED(mem)) && (mem->next - (ptr + SIZEOF_STRUCT_MEM)) >= size)
        {
            /* 如果当前的item后面跟的内存块比要申请的空间加其他描述信息的空间大的多  */
            if (mem->next - (ptr + SIZEOF_STRUCT_MEM) >= (size + SIZEOF_STRUCT_MEM + MIN_SIZE_ALIGNED))
            {
                /* 分割 */
                /*ptr2 指向当前信息块加实际内存块后的（下一个空闲块要写入对应的信息块）*/
                ptr2 = ptr + SIZEOF_STRUCT_MEM + size;
                /* 刚才的内存块被使用了 创建一个新内存块 mem2 */
                mem2       = (struct rt_small_mem_item *)&small_mem->heap_ptr[ptr2];
                /* 状态标记为空闲 */
                mem2->pool_ptr = MEM_FREED();
                /* mem2指向的下一个内存块 指向之前mem指向的内存块 */
                mem2->next = mem->next;
                /* 前一个地址指向 之前的内存块*/
                mem2->prev = ptr;

                /* mem由之前指向的mem->next 指向ptr2 */
                mem->next = ptr2;
                /* 若*/
                if (mem2->next != small_mem->mem_size_aligned + SIZEOF_STRUCT_MEM)
                {
                    ((struct rt_small_mem_item *)&small_mem->heap_ptr[mem2->next])->prev = ptr2;
                }
                /* 记录已使用的内存大小  */
                small_mem->parent.used += (size + SIZEOF_STRUCT_MEM);
                if (small_mem->parent.max < small_mem->parent.used)
                    small_mem->parent.max = small_mem->parent.used;
            }
            else
            {
                /* 不分割 直接分配*/
                small_mem->parent.used += mem->next - ((rt_uint8_t *)mem - small_mem->heap_ptr);
                if (small_mem->parent.max < small_mem->parent.used)
                    small_mem->parent.max = small_mem->parent.used;
            }
        /* 标记之前的空闲内存已被使用 */
        mem->pool_ptr = MEM_USED();

        if (mem == small_mem->lfree)
        {
            /* 查找mem之后的下一个空闲块并更新最低空闲指针 */
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

        /* 返回除mem结构之外的内存数据 */
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
/* 小内存释放 */ /* */
void rt_smem_free(void *rmem)
{
    /* 小内存节点  */
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
