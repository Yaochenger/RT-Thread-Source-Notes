/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-03-28     Bernard      first version
 * 2006-04-29     Bernard      implement thread timer
 * 2006-04-30     Bernard      added THREAD_DEBUG
 * 2006-05-27     Bernard      fixed the rt_thread_yield bug
 * 2006-06-03     Bernard      fixed the thread timer init bug
 * 2006-08-10     Bernard      fixed the timer bug in thread_sleep
 * 2006-09-03     Bernard      changed rt_timer_delete to rt_timer_detach
 * 2006-09-03     Bernard      implement rt_thread_detach
 * 2008-02-16     Bernard      fixed the rt_thread_timeout bug
 * 2010-03-21     Bernard      change the errno of rt_thread_delay/sleep to
 *                             RT_EOK.
 * 2010-11-10     Bernard      add cleanup callback function in thread exit.
 * 2011-09-01     Bernard      fixed rt_thread_exit issue when the current
 *                             thread preempted, which reported by Jiaxing Lee.
 * 2011-09-08     Bernard      fixed the scheduling issue in rt_thread_startup.
 * 2012-12-29     Bernard      fixed compiling warning.
 * 2016-08-09     ArdaFu       add thread suspend and resume hook.
 * 2017-04-10     armink       fixed the rt_thread_delete and rt_thread_detach
 *                             bug when thread has not startup.
 * 2018-11-22     Jesven       yield is same to rt_schedule
 *                             add support for tasks bound to cpu
 * 2021-02-24     Meco Man     rearrange rt_thread_control() - schedule the thread when close it
 */

#include <rthw.h>
#include <rtthread.h>

#ifdef RT_USING_HOOK
static void (*rt_thread_suspend_hook)(rt_thread_t thread);
static void (*rt_thread_resume_hook) (rt_thread_t thread);
static void (*rt_thread_inited_hook) (rt_thread_t thread);

/**
 * @ingroup Hook
 * This function sets a hook function when the system suspend a thread.
 *
 * @param hook the specified hook function
 *
 * @note the hook function must be simple and never be blocked or suspend.
 */
void rt_thread_suspend_sethook(void (*hook)(rt_thread_t thread))
{
    rt_thread_suspend_hook = hook;
}

/**
 * @ingroup Hook
 * This function sets a hook function when the system resume a thread.
 *
 * @param hook the specified hook function
 *
 * @note the hook function must be simple and never be blocked or suspend.
 */
void rt_thread_resume_sethook(void (*hook)(rt_thread_t thread))
{
    rt_thread_resume_hook = hook;
}

/**
 * @ingroup Hook
 * This function sets a hook function when a thread is initialized.
 *
 * @param hook the specified hook function
 */
void rt_thread_inited_sethook(void (*hook)(rt_thread_t thread))
{
    rt_thread_inited_hook = hook;
}

#endif /* RT_USING_HOOK */

/* must be invoke witch rt_hw_interrupt_disable */
static void _rt_thread_cleanup_execute(rt_thread_t thread)
{
    register rt_base_t level;

    level = rt_hw_interrupt_disable();

    /* invoke thread cleanup */
    if (thread->cleanup != RT_NULL)
        thread->cleanup(thread);

    rt_hw_interrupt_enable(level);
}

static void _rt_thread_exit(void)
{
    struct rt_thread *thread;
    register rt_base_t level;

    /* get current thread */
    thread = rt_thread_self();

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    _rt_thread_cleanup_execute(thread);

    /* remove from schedule */
    rt_schedule_remove_thread(thread);
    /* change stat */
    thread->stat = RT_THREAD_CLOSE;

    /* remove it from timer list */
    rt_timer_detach(&thread->thread_timer);

    if (rt_object_is_systemobject((rt_object_t)thread) == RT_TRUE)
    {
        rt_object_detach((rt_object_t)thread);
    }
    else
    {
        /* insert to defunct thread list */
        rt_thread_defunct_enqueue(thread);
    }

    /* switch to next task */
    rt_schedule();

    /* enable interrupt */
    rt_hw_interrupt_enable(level);
}

static rt_err_t _rt_thread_init(struct rt_thread *thread,
                                const char       *name,
                                void (*entry)(void *parameter),
                                void             *parameter,
                                void             *stack_start,
                                rt_uint32_t       stack_size,
                                rt_uint8_t        priority,
                                rt_uint32_t       tick)
{
    /* init thread list */
    rt_list_init(&(thread->tlist));

    thread->entry = (void *)entry;
    thread->parameter = parameter;

    /* stack init */
    thread->stack_addr = stack_start;
    thread->stack_size = stack_size;

    /* init thread stack */
    rt_memset(thread->stack_addr, '#', thread->stack_size);
#ifdef ARCH_CPU_STACK_GROWS_UPWARD
    thread->sp = (void *)rt_hw_stack_init(thread->entry, thread->parameter,
                                          (void *)((char *)thread->stack_addr),
                                          (void *)_rt_thread_exit);
#else
    thread->sp = (void *)rt_hw_stack_init(thread->entry, thread->parameter,
                                          (rt_uint8_t *)((char *)thread->stack_addr + thread->stack_size - sizeof(rt_ubase_t)),
                                          (void *)_rt_thread_exit);
#endif /* ARCH_CPU_STACK_GROWS_UPWARD */

    /* priority init */
    RT_ASSERT(priority < RT_THREAD_PRIORITY_MAX);
    thread->init_priority    = priority;
    thread->current_priority = priority;

    thread->number_mask = 0;
#if RT_THREAD_PRIORITY_MAX > 32
    thread->number = 0;
    thread->high_mask = 0;
#endif /* RT_THREAD_PRIORITY_MAX > 32 */

    /* tick init */
    thread->init_tick      = tick;
    thread->remaining_tick = tick;

    /* error and flags */
    thread->error = RT_EOK;
    thread->stat  = RT_THREAD_INIT;

#ifdef RT_USING_SMP
    /* not bind on any cpu */
    thread->bind_cpu = RT_CPUS_NR;
    thread->oncpu = RT_CPU_DETACHED;

    /* lock init */
    thread->scheduler_lock_nest = 0;
    thread->cpus_lock_nest = 0;
    thread->critical_lock_nest = 0;
#endif /* RT_USING_SMP */

    /* initialize cleanup function and user data */
    thread->cleanup   = 0;
    thread->user_data = 0;

    /* initialize thread timer */
    rt_timer_init(&(thread->thread_timer),
                  thread->name,
                  rt_thread_timeout,
                  thread,
                  0,
                  RT_TIMER_FLAG_ONE_SHOT);

    /* initialize signal */
#ifdef RT_USING_SIGNALS
    thread->sig_mask    = 0x00;
    thread->sig_pending = 0x00;

#ifndef RT_USING_SMP
    thread->sig_ret     = RT_NULL;
#endif /* RT_USING_SMP */
    thread->sig_vectors = RT_NULL;
    thread->si_list     = RT_NULL;
#endif /* RT_USING_SIGNALS */

#ifdef RT_USING_LWP
    thread->lwp = RT_NULL;
#endif /* RT_USING_LWP */

#ifdef RT_USING_CPU_USAGE
    thread->duration_tick = 0;
#endif

    RT_OBJECT_HOOK_CALL(rt_thread_inited_hook, (thread));

    return RT_EOK;
}

/**
 * @addtogroup Thread
 */

/**@{*/

/**
 * This function will initialize a thread, normally it's used to initialize a
 * static thread object.
 *
 * @param thread the static thread object
 * @param name the name of thread, which shall be unique
 * @param entry the entry function of thread
 * @param parameter the parameter of thread enter function
 * @param stack_start the start address of thread stack
 * @param stack_size the size of thread stack
 * @param priority the priority of thread
 * @param tick the time slice if there are same priority thread
 *
 * @return the operation status, RT_EOK on OK, -RT_ERROR on error
 */
/* 线程初始化 */
rt_err_t rt_thread_init(struct rt_thread *thread,       /* 线程句柄   */
                        const char       *name,         /* 线程名称 */
                        void (*entry)(void *parameter), /* 线程入口  */
                        void             *parameter,    /* 线程入口参数  */
                        void             *stack_start,  /* 线程栈起始地址 */
                        rt_uint32_t       stack_size,   /* 线程栈大小 */
                        rt_uint8_t        priority,     /* 线程优先级 */
                        rt_uint32_t       tick)         /* 线程时间片 */
{
    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(stack_start != RT_NULL);

    /* initialize thread object */
    rt_object_init((rt_object_t)thread, RT_Object_Class_Thread, name); /* 初始化线程对象 */

    return _rt_thread_init(thread,      /* 线程句柄   */
                           name,        /* 线程名称 */
                           entry,       /* 线程入口  */
                           parameter,   /* 线程入口参数  */
                           stack_start, /* 线程栈起始地址 */
                           stack_size,  /* 线程栈大小 */
                           priority,    /* 线程优先级 */
                           tick);       /* 线程时间片 */
}
RTM_EXPORT(rt_thread_init);

/**
 * This function will return self thread object
 *
 * @return the self thread object
 */
rt_thread_t rt_thread_self(void) /* 获取当前线程句柄 */
{
    extern rt_thread_t rt_current_thread;/* 声明 */
    return rt_current_thread;/* 返回当前线程句柄 */
}
RTM_EXPORT(rt_thread_self);

/**
 * This function will start a thread and put it to system ready queue
 *
 * @param thread the thread to be started
 *
 * @return the operation status, RT_EOK on OK, -RT_ERROR on error
 */
rt_err_t rt_thread_startup(rt_thread_t thread)
{
    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_INIT);
    RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);

    /* set current priority to initialize priority */ /* 初始化线程当前的优先级 */
    thread->current_priority = thread->init_priority;

    /* calculate priority attribute */ /* 计算优先级属性  */
    thread->number_mask = 1L << thread->current_priority; /* 设置线程优先级掩码 */

    RT_DEBUG_LOG(RT_DEBUG_THREAD, ("startup a thread:%s with priority:%d\n",
                                   thread->name, thread->init_priority));
    /* change thread stat */ /* 初始化线程状态 */
    thread->stat = RT_THREAD_SUSPEND;
    /* then resume it */ /* 启动线程 */
    rt_thread_resume(thread);
    if (rt_thread_self() != RT_NULL)
    {
        /* do a scheduling */
        rt_schedule(); /*触发一次调度 */
    }

    return RT_EOK;
}
RTM_EXPORT(rt_thread_startup);

/**
 * This function will detach a thread. The thread object will be removed from
 * thread queue and detached/deleted from system object management.
 *
 * @param thread the thread to be deleted
 *
 * @return the operation status, RT_EOK on OK, -RT_ERROR on error
 */
rt_err_t rt_thread_detach(rt_thread_t thread) /* 线程脱离函数 */
{
    rt_base_t lock;

    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);
    RT_ASSERT(rt_object_is_systemobject((rt_object_t)thread));

    if ((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_CLOSE) /* 线程状态  */
        return RT_EOK;

    if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_INIT) /* 若线程不是初始状态 */
    {
        /* remove from schedule */
        rt_schedule_remove_thread(thread);/* 将线程从就绪链表中移除 */
    }

    _rt_thread_cleanup_execute(thread); /* 清除线程资源  */

    /* release thread timer */
    rt_timer_detach(&(thread->thread_timer));/* 移除线程定时器 */

    /* change stat */
    thread->stat = RT_THREAD_CLOSE;/* 设置线程状态  */

    if (rt_object_is_systemobject((rt_object_t)thread) == RT_TRUE) /* 判断线程是否位静态对象  */
    {
        rt_object_detach((rt_object_t)thread);/* 将线程对象移除  */
    }
    else
    {
        /* disable interrupt */
        lock = rt_hw_interrupt_disable(); /* 关全局中断 */
        /* insert to defunct thread list */
        rt_thread_defunct_enqueue(thread);/* 加入僵尸线程 */
        /* enable interrupt */
        rt_hw_interrupt_enable(lock); /* 使能全局中断 */
    }

    return RT_EOK;
}
RTM_EXPORT(rt_thread_detach);

#ifdef RT_USING_HEAP
/**
 * This function will create a thread object and allocate thread object memory
 * and stack.
 *
 * @param name the name of thread, which shall be unique
 * @param entry the entry function of thread
 * @param parameter the parameter of thread enter function
 * @param stack_size the size of thread stack
 * @param priority the priority of thread
 * @param tick the time slice if there are same priority thread
 *
 * @return the created thread object
 */
rt_thread_t rt_thread_create(const char *name,              /* 线程名称 */
                             void (*entry)(void *parameter),/* 线程入口函数 */
                             void       *parameter,         /* 线程入口参数 */
                             rt_uint32_t stack_size,        /* 线程栈大小 */
                             rt_uint8_t  priority,          /* 线程优先级 */
                             rt_uint32_t tick)              /* 线程时间片 */
{
    struct rt_thread *thread;/* 线程句柄 */
    void *stack_start;/* */

    thread = (struct rt_thread *)rt_object_allocate(RT_Object_Class_Thread,
                                                    name);/* 创建线程对象  */
    if (thread == RT_NULL) /* 创建线程对象失败 */
        return RT_NULL;

    stack_start = (void *)RT_KERNEL_MALLOC(stack_size);/* 动态分配内存 */
    if (stack_start == RT_NULL)/* 内存申请失败 */
    {
        /* allocate stack failure */
        rt_object_delete((rt_object_t)thread);/* 删除线程对象 */

        return RT_NULL;
    }

    _rt_thread_init(thread,/* 初始化线程 */
                    name,
                    entry,
                    parameter,
                    stack_start,
                    stack_size,
                    priority,
                    tick);

    return thread;
}
RTM_EXPORT(rt_thread_create);

/**
 * This function will delete a thread. The thread object will be removed from
 * thread queue and deleted from system object management in the idle thread.
 *
 * @param thread the thread to be deleted
 *
 * @return the operation status, RT_EOK on OK, -RT_ERROR on error
 */
rt_err_t rt_thread_delete(rt_thread_t thread) /* 删除线程 */
{
    rt_base_t lock;

    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);
    RT_ASSERT(rt_object_is_systemobject((rt_object_t)thread) == RT_FALSE);

    if ((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_CLOSE)/* 检查线程状态是否为关闭状态 关闭了就不用删除了 */
        return RT_EOK;

    if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_INIT)/* 判断线程状态是否为初始化状态 不是从就绪链表中移除 */
    {
        /* remove from schedule */
        rt_schedule_remove_thread(thread);/* 从就绪链表中移除 */
    }

    _rt_thread_cleanup_execute(thread);/* 清除线程资源 */

    /* release thread timer */
    rt_timer_detach(&(thread->thread_timer));/* 释放定时器 */

    /* disable interrupt */
    lock = rt_hw_interrupt_disable();/* 关全局中断 */

    /* change stat */
    thread->stat = RT_THREAD_CLOSE;/* 设置线程状态为关闭 */

    /* insert to defunct thread list */
    rt_thread_defunct_enqueue(thread);/* 将线程加入僵尸线程 */

    /* enable interrupt */
    rt_hw_interrupt_enable(lock);/* 全局中断使能 */

    return RT_EOK;
}
RTM_EXPORT(rt_thread_delete);
#endif /* RT_USING_HEAP */

/**
 * This function will let current thread yield processor, and scheduler will
 * choose a highest thread to run. After yield processor, the current thread
 * is still in READY state.
 *
 * @return RT_EOK
 */
rt_err_t rt_thread_yield(void)
{
    struct rt_thread *thread; /* */
    rt_base_t lock;

    thread = rt_thread_self();/* 获取当前运行的线程 */
    lock = rt_hw_interrupt_disable();/* 关全局中断 */
    thread->remaining_tick = thread->init_tick;/* 初始化线程时间片 */
    thread->stat |= RT_THREAD_STAT_YIELD;/* 设置线程状态 */
    rt_schedule();/* 线程调度 */
    rt_hw_interrupt_enable(lock);/* 使能全局中断 */

    return RT_EOK;
}
RTM_EXPORT(rt_thread_yield);

/**
 * This function will let current thread sleep for some ticks.
 *
 * @param tick the sleep ticks
 *
 * @return RT_EOK
 */
rt_err_t rt_thread_sleep(rt_tick_t tick)/* 线程休眠指定tick */
{
    register rt_base_t temp;/* 临时寄存器参数 */
    struct rt_thread *thread; /* 线程句柄 */

    /* set to current thread */
    thread = rt_thread_self(); /* 获取当前线程句柄 */
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);

    /* disable interrupt */
    temp = rt_hw_interrupt_disable();/* 关全局中断  */

    /* suspend thread */
    rt_thread_suspend(thread);/* 挂起线程 */

    /* reset the timeout of thread timer and start it */
    rt_timer_control(&(thread->thread_timer), RT_TIMER_CTRL_SET_TIME, &tick); /* 设置定时器时间  */
    rt_timer_start(&(thread->thread_timer));/* 启动定时器 */

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);/* 使能全局中断 */

    rt_schedule();/* 线程调度 */

    /* clear error number of this thread to RT_EOK */ /* 设置线程error状态 */
    if (thread->error == -RT_ETIMEOUT)
        thread->error = RT_EOK;

    return RT_EOK;
}

/**
 * This function will let current thread delay for some ticks.
 *
 * @param tick the delay ticks
 *
 * @return RT_EOK
 */
rt_err_t rt_thread_delay(rt_tick_t tick) /* 线程状态延时  */
{
    return rt_thread_sleep(tick);
}
RTM_EXPORT(rt_thread_delay);

/**
 * This function will let current thread delay until (*tick + inc_tick).
 *
 * @param tick the tick of last wakeup.
 * @param inc_tick the increment tick
 *
 * @return RT_EOK
 */
rt_err_t rt_thread_delay_until(rt_tick_t *tick, rt_tick_t inc_tick)
{
    register rt_base_t level;
    struct rt_thread *thread;
    rt_tick_t cur_tick;

    RT_ASSERT(tick != RT_NULL);

    /* set to current thread */
    thread = rt_thread_self();
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);

    /* disable interrupt */
    level = rt_hw_interrupt_disable();

    cur_tick = rt_tick_get();
    if (cur_tick - *tick < inc_tick)
    {
        rt_tick_t left_tick;

        *tick += inc_tick;
        left_tick = *tick - cur_tick;

        /* suspend thread */
        rt_thread_suspend(thread);

        /* reset the timeout of thread timer and start it */
        rt_timer_control(&(thread->thread_timer), RT_TIMER_CTRL_SET_TIME, &left_tick);
        rt_timer_start(&(thread->thread_timer));

        /* enable interrupt */
        rt_hw_interrupt_enable(level);

        rt_schedule();

        /* clear error number of this thread to RT_EOK */
        if (thread->error == -RT_ETIMEOUT)
        {
            thread->error = RT_EOK;
        }
    }
    else
    {
        *tick = cur_tick;
        rt_hw_interrupt_enable(level);
    }

    return RT_EOK;
}
RTM_EXPORT(rt_thread_delay_until);

/**
 * This function will let current thread delay for some milliseconds.
 *
 * @param ms the delay ms time
 *
 * @return RT_EOK
 */
rt_err_t rt_thread_mdelay(rt_int32_t ms) /* ms级延时 */
{
    rt_tick_t tick;

    tick = rt_tick_from_millisecond(ms);

    return rt_thread_sleep(tick);
}
RTM_EXPORT(rt_thread_mdelay);

/**
 * This function will control thread behaviors according to control command.
 *
 * @param thread the specified thread to be controlled
 * @param cmd the control command, which includes
 *  RT_THREAD_CTRL_CHANGE_PRIORITY for changing priority level of thread;
 *  RT_THREAD_CTRL_STARTUP for starting a thread;
 *  RT_THREAD_CTRL_CLOSE for delete a thread;
 *  RT_THREAD_CTRL_BIND_CPU for bind the thread to a CPU.
 * @param arg the argument of control command
 *
 * @return RT_EOK
 */
rt_err_t rt_thread_control(rt_thread_t thread, int cmd, void *arg)/* 控制线程 */
{
    register rt_base_t temp;

    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);

    switch (cmd)/* 判断命令 */
    {
        case RT_THREAD_CTRL_CHANGE_PRIORITY: /* 重置优先级 */
        {
            /* disable interrupt */
            temp = rt_hw_interrupt_disable();/* 关全局中断 */

            /* for ready thread, change queue */
            if ((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_READY) /* 若线程处于就绪态 */
            {
                /* remove thread from schedule queue first */
                rt_schedule_remove_thread(thread); /* 将线程从就绪链表中移除 */

                /* change thread priority */
                thread->current_priority = *(rt_uint8_t *)arg; /* 修改线程优先级 */

                /* recalculate priority attribute */
                thread->number_mask = 1 << thread->current_priority; /* 重新计算线程优先级掩码 */

                /* insert thread to schedule queue again */
                rt_schedule_insert_thread(thread);/* 将线程插入就绪链表 */
            }
            else
            {
                thread->current_priority = *(rt_uint8_t *)arg;

                /* recalculate priority attribute */
                thread->number_mask = 1 << thread->current_priority;/* 重新计算线程优先级掩码 */
            }

            /* enable interrupt */
            rt_hw_interrupt_enable(temp);/* 使能全局中断  */
            break;
        }

        case RT_THREAD_CTRL_STARTUP:/* 启动线程  */
        {
            return rt_thread_startup(thread);
        }

        case RT_THREAD_CTRL_CLOSE:/* 关闭线程  */
        {
            rt_err_t rt_err;

            if (rt_object_is_systemobject((rt_object_t)thread) == RT_TRUE)/* 查询线程是否是线程对象 */
            {
                rt_err = rt_thread_detach(thread);/* 将线程脱离线线程对象链表 */
            }
    #ifdef RT_USING_HEAP
            else
            {
                rt_err = rt_thread_delete(thread);/* 删除线程*/
            }
    #endif /* RT_USING_HEAP */
            rt_schedule();/* 进行调度 */
            return rt_err;
        }
        default:
            break;
    }

    return RT_EOK;
}
RTM_EXPORT(rt_thread_control);

/**
 * This function will suspend the specified thread.
 *
 * @param thread the thread to be suspended
 *
 * @return the operation status, RT_EOK on OK, -RT_ERROR on error
 *
 * @note if suspend self thread, after this function call, the
 * rt_schedule() must be invoked.
 */
rt_err_t rt_thread_suspend(rt_thread_t thread)/* 挂起线程 */
{
    register rt_base_t stat;
    register rt_base_t temp;

    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);/* */

    RT_DEBUG_LOG(RT_DEBUG_THREAD, ("thread suspend:  %s\n", thread->name));

    stat = thread->stat & RT_THREAD_STAT_MASK;/* 获取线程状态 */
    if ((stat != RT_THREAD_READY) && (stat != RT_THREAD_RUNNING))/* */
    {
        RT_DEBUG_LOG(RT_DEBUG_THREAD, ("thread suspend: thread disorder, 0x%2x\n",
                                       thread->stat));
        return -RT_ERROR;
    }

    /* disable interrupt *//* 关全局中断 */
    temp = rt_hw_interrupt_disable();
    if (stat == RT_THREAD_RUNNING)
    {
        /* not suspend running status thread on other core */
        RT_ASSERT(thread == rt_thread_self());
    }

    /* change thread stat */
    rt_schedule_remove_thread(thread);/* 将线程从就绪链表中移除 */
    thread->stat = RT_THREAD_SUSPEND | (thread->stat & ~RT_THREAD_STAT_MASK); /* 设置线程状态为挂起 */

    /* stop thread timer anyway */
    rt_timer_stop(&(thread->thread_timer));/* 停止线程定时器 */

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);/* 使能全局中断  */

    RT_OBJECT_HOOK_CALL(rt_thread_suspend_hook, (thread));
    return RT_EOK;
}
RTM_EXPORT(rt_thread_suspend);

/**
 * This function will resume a thread and put it to system ready queue.
 *
 * @param thread the thread to be resumed
 *
 * @return the operation status, RT_EOK on OK, -RT_ERROR on error
 */
rt_err_t rt_thread_resume(rt_thread_t thread)/* 恢复线程 */
{
    register rt_base_t temp;

    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);

    RT_DEBUG_LOG(RT_DEBUG_THREAD, ("thread resume:  %s\n", thread->name));

    if ((thread->stat & RT_THREAD_STAT_MASK) != RT_THREAD_SUSPEND)
    {
        RT_DEBUG_LOG(RT_DEBUG_THREAD, ("thread resume: thread disorder, %d\n",
                                       thread->stat));

        return -RT_ERROR;
    }

    /* disable interrupt *//* 关全局中断 */
    temp = rt_hw_interrupt_disable();

    /* remove from suspend list */
    rt_list_remove(&(thread->tlist)); /* 将线程从挂起线程移除 */

    rt_timer_stop(&thread->thread_timer);/* 停止线程定时器  */

    /* insert to schedule ready list */
    rt_schedule_insert_thread(thread);/* 将线程插入就绪链表 */

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);/* 使能全局中断 */

    RT_OBJECT_HOOK_CALL(rt_thread_resume_hook, (thread));
    return RT_EOK;
}
RTM_EXPORT(rt_thread_resume);

/**
 * This function is the timeout function for thread, normally which is invoked
 * when thread is timeout to wait some resource.
 *
 * @param parameter the parameter of thread timeout function
 */
void rt_thread_timeout(void *parameter)/* 定时器超时函数 */
{
    struct rt_thread *thread;
    register rt_base_t temp;

    thread = (struct rt_thread *)parameter;

    /* thread check */
    RT_ASSERT(thread != RT_NULL);
    RT_ASSERT((thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_SUSPEND);
    RT_ASSERT(rt_object_get_type((rt_object_t)thread) == RT_Object_Class_Thread);

    /* disable interrupt */
    temp = rt_hw_interrupt_disable(); /* 关全局中断 */

    /* set error number */
    thread->error = -RT_ETIMEOUT;/* 设置线程状态 */

    /* remove from suspend list */
    rt_list_remove(&(thread->tlist));/* 将线程从挂起线程链表移除  */

    /* insert to schedule ready list */
    rt_schedule_insert_thread(thread);/* 将线程插入就绪链表 */

    /* enable interrupt */
    rt_hw_interrupt_enable(temp);/* 使能全局中断 */

    /* do schedule */
    rt_schedule();/* 线程调度 */
}
RTM_EXPORT(rt_thread_timeout);

/**
 * This function will find the specified thread.
 *
 * @param name the name of thread finding
 *
 * @return the found thread
 *
 * @note please don't invoke this function in interrupt status.
 */
rt_thread_t rt_thread_find(char *name)/* 通过名称查找线程对象 */
{
    return (rt_thread_t)rt_object_find(name, RT_Object_Class_Thread);
}
RTM_EXPORT(rt_thread_find);

/**@}*/
