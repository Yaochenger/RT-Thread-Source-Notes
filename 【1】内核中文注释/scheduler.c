/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-03-17     Bernard      the first version
 * 2006-04-28     Bernard      fix the scheduler algorthm
 * 2006-04-30     Bernard      add SCHEDULER_DEBUG
 * 2006-05-27     Bernard      fix the scheduler algorthm for same priority
 *                             thread schedule
 * 2006-06-04     Bernard      rewrite the scheduler algorithm
 * 2006-08-03     Bernard      add hook support
 * 2006-09-05     Bernard      add 32 priority level support
 * 2006-09-24     Bernard      add rt_system_scheduler_start function
 * 2009-09-16     Bernard      fix _rt_scheduler_stack_check
 * 2010-04-11     yi.qiu       add module feature
 * 2010-07-13     Bernard      fix the maximal number of rt_scheduler_lock_nest
 *                             issue found by kuronca
 * 2010-12-13     Bernard      add defunct list initialization even if not use heap.
 * 2011-05-10     Bernard      clean scheduler debug log.
 * 2013-12-21     Grissiom     add rt_critical_level
 * 2018-11-22     Jesven       remove the current task from ready queue
 *                             add per cpu ready queue
 *                             add _get_highest_priority_thread to find highest priority task
 *                             rt_schedule_insert_thread won't insert current task to ready queue
 *                             in smp version, rt_hw_context_switch_interrupt maybe switch to
 *                             new task directly
 *
 */

#include <rtthread.h>
#include <rthw.h>
rt_list_t rt_thread_priority_table[RT_THREAD_PRIORITY_MAX];/* 线程链表节点数组 就绪的线程均会挂载到该链表数组下 */
rt_uint32_t rt_thread_ready_priority_group;

#ifndef RT_USING_SMP
extern volatile rt_uint8_t rt_interrupt_nest; /* 中断嵌套深度 */
static rt_int16_t rt_scheduler_lock_nest;     /* 调度器上锁的深度 */
struct rt_thread *rt_current_thread = RT_NULL;/* 当前运行的线程 */
rt_uint8_t rt_current_priority; /* 当前运行的线程的优先级  */
#endif /* RT_USING_SMP */

#ifdef RT_USING_HOOK
static void (*rt_scheduler_hook)(struct rt_thread *from, struct rt_thread *to);
static void (*rt_scheduler_switch_hook)(struct rt_thread *tid);

/**
 * @addtogroup Hook
 */

/**@{*/

/**
 * This function will set a hook function, which will be invoked when thread
 * switch happens.
 *
 * @param hook the hook function
 */
void
rt_scheduler_sethook(void (*hook)(struct rt_thread *from, struct rt_thread *to))
{
    rt_scheduler_hook = hook;
}

void
rt_scheduler_switch_sethook(void (*hook)(struct rt_thread *tid))
{
    rt_scheduler_switch_hook = hook;
}

/**@}*/
#endif /* RT_USING_HOOK */

/*
 * get the highest priority thread in ready queue
 */
static struct rt_thread* _get_highest_priority_thread(rt_ubase_t *highest_prio)
{
    register struct rt_thread *highest_priority_thread;/* 最高优先级线程 */
    register rt_ubase_t highest_ready_priority;/* 就绪的最高优先级 */

    highest_ready_priority = __rt_ffs(rt_thread_ready_priority_group) - 1; /* 就绪的最高优先级 */

    /* 最高优先级线程对象 */
    highest_priority_thread = rt_list_entry(rt_thread_priority_table[highest_ready_priority].next,
                              struct rt_thread,
                              tlist);

    *highest_prio = highest_ready_priority; /* 就绪的最高优先级赋值 */

    return highest_priority_thread; /* 返回最高优先级线程对象 */
}

/**
 * @ingroup SystemInit
 * This function will initialize the system scheduler
 */
void rt_system_scheduler_init(void)
{
    register rt_base_t offset; /* 临时变量 */

#ifndef RT_USING_SMP
    rt_scheduler_lock_nest = 0;/* 调度器锁 */
#endif /* RT_USING_SMP */

    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("start scheduler: max priority 0x%02x\n",
                                      RT_THREAD_PRIORITY_MAX));

    /* 初始化任务对象链表 */
    for (offset = 0; offset < RT_THREAD_PRIORITY_MAX; offset ++)
    {
        rt_list_init(&rt_thread_priority_table[offset]);
    }
    /* 初始化就绪优先级查询组 */
    rt_thread_ready_priority_group = 0;
}

/**
 * @ingroup SystemInit
 * This function will startup scheduler. It will select one thread
 * with the highest priority level, then switch to it.
 */
void rt_system_scheduler_start(void)
{
    register struct rt_thread *to_thread; /* 要切换的线程  */
    rt_ubase_t highest_ready_priority; /* 保存就绪的最高优先级的变量 */

    to_thread = _get_highest_priority_thread(&highest_ready_priority); /* 就绪最高优先级的线程对象 */

    rt_current_thread = to_thread; /* 当前就绪 即将运行的最高优先级线程 */

    rt_schedule_remove_thread(to_thread); /* 从就绪链表中移除 */
    to_thread->stat = RT_THREAD_RUNNING;  /* 将状态设置为运行 */

    rt_hw_context_switch_to((rt_ubase_t)&to_thread->sp); /* 启动第一个线程 */

    /* never come back */
}

/**
 * @addtogroup Thread
 */

/**@{*/


#ifdef RT_USING_SMP

#else
/**
 * This function will perform one schedule. It will select one thread
 * with the highest priority level, and switch to it immediately.
 */
void rt_schedule(void)
{
    rt_base_t level;/* 关中断前的机器状态 */
    struct rt_thread *to_thread; /* 要切换去的线程 */
    struct rt_thread *from_thread;/* 被切换的线程 */

    /* 关全局中断  */
    level = rt_hw_interrupt_disable();

    /* 检查调度器是否上锁 */
    if (rt_scheduler_lock_nest == 0)
    {
        /* 就绪的线程的最高优先级 */
        rt_ubase_t highest_ready_priority;
        /* 存在就绪的线程 */
        if (rt_thread_ready_priority_group != 0)
        {
            /* 需要将from线程插入就绪链表的标志  */
            int need_insert_from_thread = 0;
            /* 查询当前就绪的最高优先级的线程 并赋值给to_thread*/
            to_thread = _get_highest_priority_thread(&highest_ready_priority);
            /* 当前运行的的线程的状态仍为运行态*/
            if ((rt_current_thread->stat & RT_THREAD_STAT_MASK) == RT_THREAD_RUNNING)
            {
                /* 当前线程的优先级最高 */
                if (rt_current_thread->current_priority < highest_ready_priority)
                {
                    /* 不进行线程切换 */
                    to_thread = rt_current_thread;
                }/* 当前线程的优先级等于刚就绪的线程的最高优先级   且当前线程的时间片还未运行结束 */
                else if (rt_current_thread->current_priority == highest_ready_priority && (rt_current_thread->stat & RT_THREAD_STAT_YIELD_MASK) == 0)
                {
                    /* 不进行切换 被切换的线程仍然是当前运行的线程 */
                    to_thread = rt_current_thread;
                }
                else/* 当前运行的线程的优先级低于就绪的线程的最高优先级 需要切换 */
                {
                    need_insert_from_thread = 1;
                }/* 当前线程的状态设置为需要切换 */
                rt_current_thread->stat &= ~RT_THREAD_STAT_YIELD_MASK;
            }
            /* 需要进行线程切换 */
            if (to_thread != rt_current_thread)
            {
                /* 当前线程的优先级设置为就绪的最高优先级 */
                rt_current_priority = (rt_uint8_t)highest_ready_priority;
                /* 当前运行的线程成为被切换的线程 */
                from_thread         = rt_current_thread;
                /* 当前准备运行的线程 设置为要切换的线程 */
                rt_current_thread   = to_thread;

                RT_OBJECT_HOOK_CALL(rt_scheduler_hook, (from_thread, to_thread));
                /* 将之前运行的线程插入到就绪线程的末尾 */
                if (need_insert_from_thread)
                {
                    rt_schedule_insert_thread(from_thread);
                }
                /* 将即将运行的线程从就绪链表移除 */
                rt_schedule_remove_thread(to_thread);
                /* 将即将运行的线程的状态设置为运行 */
                to_thread->stat = RT_THREAD_RUNNING | (to_thread->stat & ~RT_THREAD_STAT_MASK);

                /* 切换至新的线程  */
                RT_DEBUG_LOG(RT_DEBUG_SCHEDULER,
                        ("[%d]switch to priority#%d "
                         "thread:%.*s(sp:0x%08x), "
                         "from thread:%.*s(sp: 0x%08x)\n",
                         rt_interrupt_nest, highest_ready_priority,
                         RT_NAME_MAX, to_thread->name, to_thread->sp,
                         RT_NAME_MAX, from_thread->name, from_thread->sp));
                /* 未在中断环境中 */
                if (rt_interrupt_nest == 0)
                {
                    extern void rt_thread_handle_sig(rt_bool_t clean_state);

                    RT_OBJECT_HOOK_CALL(rt_scheduler_switch_hook, (from_thread));
                    /* 使用线程与线程间切换函数 */
                    rt_hw_context_switch((rt_ubase_t)&from_thread->sp,
                            (rt_ubase_t)&to_thread->sp);

                    /* enable interrupt */
                    rt_hw_interrupt_enable(level);

                    goto __exit;
                }
                else
                {
                    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("switch in interrupt\n"));
                    /* 使用中断与线程间切换的线程代码 */
                    rt_hw_context_switch_interrupt((rt_ubase_t)&from_thread->sp,
                            (rt_ubase_t)&to_thread->sp);
                }
            }/* 不需要进行线程切换 */
            else
            {
                /* 将当前线程从就绪链表中移除 */
                rt_schedule_remove_thread(rt_current_thread);
                rt_current_thread->stat = RT_THREAD_RUNNING | (rt_current_thread->stat & ~RT_THREAD_STAT_MASK);
            }
        }
    }

    /* 使能全局中断 */
    rt_hw_interrupt_enable(level);

__exit:
    return;
}
#endif /* RT_USING_SMP */

/**
 * This function checks if a scheduling is needed after IRQ context. If yes,
 * it will select one thread with the highest priority level, and then switch
 * to it.
 */


/*
 * This function will insert a thread to system ready queue. The state of
 * thread will be set as READY and remove from suspend queue.
 *
 * @param thread the thread to be inserted
 * @note Please do not invoke this function in user application.
 */
void rt_schedule_insert_thread(struct rt_thread *thread)
{
    register rt_base_t temp; /* 临时变量  */

    RT_ASSERT(thread != RT_NULL);

    /* 关闭全局中断 */
    temp = rt_hw_interrupt_disable();

    /* 若是当前线程 设置为运行 直接退出 */
    if (thread == rt_current_thread)
    {
        thread->stat = RT_THREAD_RUNNING | (thread->stat & ~RT_THREAD_STAT_MASK);
        goto __exit;
    }

    /* 就绪的线程 设置状态为就绪 */
    thread->stat = RT_THREAD_READY | (thread->stat & ~RT_THREAD_STAT_MASK);
    /* 插到就绪链表中 从链表尾部插入 */
    rt_list_insert_before(&(rt_thread_priority_table[thread->current_priority]),&(thread->tlist));

    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("insert thread[%.*s], the priority: %d\n",
                                      RT_NAME_MAX, thread->name, thread->current_priority));

    /* 将查询该优先级的优先级位置为 */
    rt_thread_ready_priority_group |= thread->number_mask;

__exit:
    /* 使能全局中断 */
    rt_hw_interrupt_enable(temp);
}

/*
 * This function will remove a thread from system ready queue.
 *
 * @param thread the thread to be removed
 *
 * @note Please do not invoke this function in user application.
 */
void rt_schedule_remove_thread(struct rt_thread *thread)
{
    register rt_base_t level;

    RT_ASSERT(thread != RT_NULL);

    /* 关闭全局中断 */
    level = rt_hw_interrupt_disable();

    RT_DEBUG_LOG(RT_DEBUG_SCHEDULER, ("remove thread[%.*s], the priority: %d\n",
                                      RT_NAME_MAX, thread->name,
                                      thread->current_priority));

    /* 将线程对象从就绪链表中移除 若该优先级下有同样优先级就绪的任务 则该任务会替补成为该优先级下最先就绪的任务 */
    rt_list_remove(&(thread->tlist));
    /* 判断该优先级下就绪的任务是否为空 */
    if (rt_list_isempty(&(rt_thread_priority_table[thread->current_priority])))
    {
        /* 若该优先级下已经不存在就绪任务 则将该优先级从任务优先级链表中移除 */
        rt_thread_ready_priority_group &= ~thread->number_mask;
    }
    /* 使能全局中断 */
    rt_hw_interrupt_enable(level);
}

/**
 * This function will lock the thread scheduler.
 */
void rt_enter_critical(void)
{
    register rt_base_t level;

    /* 关全局中断 */
    level = rt_hw_interrupt_disable();

    /*
     * the maximal number of nest is RT_UINT16_MAX, which is big
     * enough and does not check here
     */
    /* 调度器锁标志自增 */
    rt_scheduler_lock_nest ++;

    /* 开全局中断  */
    rt_hw_interrupt_enable(level);
}
RTM_EXPORT(rt_enter_critical);

/**
 * This function will unlock the thread scheduler.
 */
void rt_exit_critical(void)
{
    register rt_base_t level;

    /* 关全局中断 */
    level = rt_hw_interrupt_disable();
    /* 调度器锁标志自减 */
    rt_scheduler_lock_nest --;
    if (rt_scheduler_lock_nest <= 0)
    {
        /* 调度器未上锁 */
        rt_scheduler_lock_nest = 0;
        /* 开全局中断 */
        rt_hw_interrupt_enable(level);
        /* 若当线程非空 则进行一次调度*/
        if (rt_current_thread)
        {
            /* if scheduler is started, do a schedule */
            rt_schedule();
        }
    }
    else
    {
        /* 使能全局中断 */
        rt_hw_interrupt_enable(level);
    }
}

/**
 * Get the scheduler lock level
 *
 * @return the level of the scheduler lock. 0 means unlocked.
 */
/* 返回当前调度器锁 上锁的深度 */
rt_uint16_t rt_critical_level(void)
{
    return rt_scheduler_lock_nest;
}
RTM_EXPORT(rt_critical_level);

/**@}*/
