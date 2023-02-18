/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-03-23     Bernard      the first version
 * 2010-11-10     Bernard      add cleanup callback function in thread exit.
 * 2012-12-29     Bernard      fix compiling warning.
 * 2013-12-21     Grissiom     let rt_thread_idle_excute loop until there is no
 *                             dead thread.
 * 2016-08-09     ArdaFu       add method to get the handler of the idle thread.
 * 2018-02-07     Bernard      lock scheduler to protect tid->cleanup.
 * 2018-07-14     armink       add idle hook list
 * 2018-11-22     Jesven       add per cpu idle task
 *                             combine the code of primary and secondary cpu
 * 2021-11-15     THEWON       Remove duplicate work between idle and _thread_exit
 */

#include <rthw.h>
#include <rtthread.h>

#define _CPUS_NR                1
/*
 * #define RT_LIST_OBJECT_INIT(object) { &(object), &(object) }
 */
/*初始化僵尸线程链表节点*/
static rt_list_t _rt_thread_defunct = RT_LIST_OBJECT_INIT(_rt_thread_defunct);
/* 空闲线程 ID*/
static struct rt_thread idle_thread[_CPUS_NR];
rt_align(RT_ALIGN_SIZE)
/* 空闲线程栈 */
static rt_uint8_t idle_thread_stack[_CPUS_NR][IDLE_THREAD_STACK_SIZE];


static void (*idle_hook_list[RT_IDLE_HOOK_LIST_SIZE])(void);

/**
 * @brief This function sets a hook function to idle thread loop. When the system performs
 *        idle loop, this hook function should be invoked.
 *
 * @param hook the specified hook function.
 *
 * @return RT_EOK: set OK.
 *         -RT_EFULL: hook list is full.
 *
 * @note the hook function must be simple and never be blocked or suspend.
 */
rt_err_t rt_thread_idle_sethook(void (*hook)(void))
{
    rt_size_t i;
    rt_base_t level;
    rt_err_t ret = -RT_EFULL;

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    for (i = 0; i < RT_IDLE_HOOK_LIST_SIZE; i++)
    {
        if (idle_hook_list[i] == RT_NULL)
        {
            idle_hook_list[i] = hook;
            ret = RT_EOK;
            break;
        }
    }
    /* enable interrupt */
    rt_hw_interrupt_enable(level);

    return ret;
}

/**
 * @brief delete the idle hook on hook list.
 *
 * @param hook the specified hook function.
 *
 * @return RT_EOK: delete OK.
 *         -RT_ENOSYS: hook was not found.
 */
rt_err_t rt_thread_idle_delhook(void (*hook)(void))
{
    rt_size_t i;
    rt_base_t level;
    rt_err_t ret = -RT_ENOSYS;

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    for (i = 0; i < RT_IDLE_HOOK_LIST_SIZE; i++)
    {
        if (idle_hook_list[i] == hook)
        {
            idle_hook_list[i] = RT_NULL;
            ret = RT_EOK;
            break;
        }
    }
    /* enable interrupt */
    rt_hw_interrupt_enable(level);

    return ret;
}

#endif /* RT_USING_IDLE_HOOK */

/**
 * @brief Enqueue a thread to defunct queue.
 *
 * @note It must be called between rt_hw_interrupt_disable and rt_hw_interrupt_enable
 */
/* 将线程链表节点插入僵尸线程链表 */
void rt_thread_defunct_enqueue(rt_thread_t thread)
{
    rt_list_insert_after(&_rt_thread_defunct, &thread->tlist);
}

/**
 * @brief Dequeue a thread from defunct queue.
 */
/* 移除第一个僵尸线程 返回其线程句柄*//* */
rt_thread_t rt_thread_defunct_dequeue(void)
{
    /* 关中断返回值 */
    rt_base_t level;
    /* 线程句柄 */
    rt_thread_t thread = RT_NULL;
    /* 将僵尸线程链表首地址赋值给临时链表节点 */
    rt_list_t *l = &_rt_thread_defunct;
    /* 链表非空 存在僵尸线程 */
    if (l->next != l)
    {
        /* 获取僵尸线程 线程句柄 */
        thread = rt_list_entry(l->next,struct rt_thread,tlist);
        /* 关中断 */
        level = rt_hw_interrupt_disable();
        /* 将僵尸线程从链表中移除 */
        rt_list_remove(&(thread->tlist));
        /* 关中断 */
        rt_hw_interrupt_enable(level);
    }/* 返回僵尸线程的线程句柄 */
    return thread;
}

/**
 * @brief This function will perform system background job when system idle.
 */
/* 僵尸回调函数 *//* */
static void rt_defunct_execute(void)
{
    /* Loop until there is no dead thread. So one call to rt_defunct_execute
     * will do all the cleanups. */
    /* 会循环执行下述程序 调用一次就会清除所有的僵尸线程 */
    while (1)
    {
        rt_thread_t thread;/* 临时线程句柄  */
        rt_bool_t object_is_systemobject;
        /* 函数指针*/
        void (*cleanup)(struct rt_thread *tid);
        /* 获得僵尸线程的线程句柄 */
        thread = rt_thread_defunct_dequeue();
        /* 线程存在*/
        if (thread == RT_NULL)
        {
            break;
        }
        /* 调用线程的clean函数 */
        cleanup = thread->cleanup;

        object_is_systemobject = rt_object_is_systemobject((rt_object_t)thread);
        if (object_is_systemobject == RT_TRUE)
        {
            /* 分离对象 */
            rt_object_detach((rt_object_t)thread);
        }
        /* 调用线程的clean函数 */
        if (cleanup != RT_NULL)
        {
            cleanup(thread);
        }
#ifdef RT_USING_HEAP
        /* if need free, delete it */
        if (object_is_systemobject == RT_FALSE)
        {
            /* 释放线程栈 */
            RT_KERNEL_FREE(thread->stack_addr);
            /* 删除线程对象 */
            rt_object_delete((rt_object_t)thread);
        }
#endif
    }
}
/* 空闲线程入口函数 */
static void idle_thread_entry(void *parameter)
{
    while (1)
    {
#ifdef RT_USING_IDLE_HOOK
        rt_size_t i;
        /* 函数指针 */
        void (*idle_hook)(void);
        /* 轮询钩子函数 */
        for (i = 0; i < RT_IDLE_HOOK_LIST_SIZE; i++)
        {
            /* 赋值钩子函数 */
            idle_hook = idle_hook_list[i];
            if (idle_hook != RT_NULL)
            {
                /* 执行钩子函数 */
                idle_hook();
            }
        }
#endif /* RT_USING_IDLE_HOOK */

#ifndef RT_USING_SMP
        rt_defunct_execute();
#endif /* RT_USING_SMP */
    }
}

/**
 * @brief This function will initialize idle thread, then start it.
 *
 * @note this function must be invoked when system init.
 */
/* 创建空闲线程 */
void rt_thread_idle_init(void)
{
    rt_ubase_t i;
    /* 空闲线程名称 */
    char idle_thread_name[RT_NAME_MAX];
    /* 轮询传创建空闲线程 */
    for (i = 0; i < _CPUS_NR; i++)
    {
        rt_sprintf(idle_thread_name, "tidle%d", i);
        rt_thread_init(&idle_thread[i],
                idle_thread_name,
                idle_thread_entry,
                RT_NULL,
                &idle_thread_stack[i][0],
                sizeof(idle_thread_stack[i]),
                RT_THREAD_PRIORITY_MAX - 1,
                32);
        /* 启动线程 */
        rt_thread_startup(&idle_thread[i]);
    }
}

/**
 * @brief This function will get the handler of the idle thread.
 */
/* 获取空闲线程地址 */
rt_thread_t rt_thread_idle_gethandler(void)
{

    int id = 0;
    /* 强转为 rt_thread_t  */
    return (rt_thread_t)(&idle_thread[id]);
}
