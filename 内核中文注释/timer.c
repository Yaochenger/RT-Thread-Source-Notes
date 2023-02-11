/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-03-12     Bernard      first version
 * 2006-04-29     Bernard      implement thread timer
 * 2006-06-04     Bernard      implement rt_timer_control
 * 2006-08-10     Bernard      fix the periodic timer bug
 * 2006-09-03     Bernard      implement rt_timer_detach
 * 2009-11-11     LiJin        add soft timer
 * 2010-05-12     Bernard      fix the timer check bug.
 * 2010-11-02     Charlie      re-implement tick overflow issue
 * 2012-12-15     Bernard      fix the next timeout issue in soft timer
 * 2014-07-12     Bernard      does not lock scheduler when invoking soft-timer
 *                             timeout function.
 * 2021-08-15     supperthomas add the comment
 * 2022-01-07     Gabriel      Moving __on_rt_xxxxx_hook to timer.c
 * 2022-04-19     Stanley      Correct descriptions
 */

#include <rtthread.h>
#include <rthw.h>

/* hard timer list */
static rt_list_t _timer_list[RT_TIMER_SKIP_LIST_LEVEL]; /* 硬定时器链表  */

#ifdef RT_USING_TIMER_SOFT           /* 使用软件定时器 */

#define RT_SOFT_TIMER_IDLE              1 /* 定时器空闲状态 */
#define RT_SOFT_TIMER_BUSY              0 /* 定时器忙状态 */

/* soft timer status */
static rt_uint8_t _soft_timer_status = RT_SOFT_TIMER_IDLE;/* 初始化软件定时器状态:空闲*/
/* soft timer list */
static rt_list_t _soft_timer_list[RT_TIMER_SKIP_LIST_LEVEL];/* 软件定时器链表 */
static struct rt_thread _timer_thread;/* 软件定时器线程 */
ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t _timer_thread_stack[RT_TIMER_THREAD_STACK_SIZE];/* 软件定时器栈*/
#endif /* RT_USING_TIMER_SOFT */

#ifndef __on_rt_object_take_hook
    #define __on_rt_object_take_hook(parent)        __ON_HOOK_ARGS(rt_object_take_hook, (parent))
#endif
#ifndef __on_rt_object_put_hook
    #define __on_rt_object_put_hook(parent)         __ON_HOOK_ARGS(rt_object_put_hook, (parent))
#endif
#ifndef __on_rt_timer_enter_hook
    #define __on_rt_timer_enter_hook(t)             __ON_HOOK_ARGS(rt_timer_enter_hook, (t))
#endif
#ifndef __on_rt_timer_exit_hook
    #define __on_rt_timer_exit_hook(t)              __ON_HOOK_ARGS(rt_timer_exit_hook, (t))
#endif

#if defined(RT_USING_HOOK) && defined(RT_HOOK_USING_FUNC_PTR)
extern void (*rt_object_take_hook)(struct rt_object *object);
extern void (*rt_object_put_hook)(struct rt_object *object);
static void (*rt_timer_enter_hook)(struct rt_timer *timer);
static void (*rt_timer_exit_hook)(struct rt_timer *timer);

/**
 * @addtogroup Hook
 */

/**@{*/

/**
 * @brief This function will set a hook function on timer,
 *        which will be invoked when enter timer timeout callback function.
 *
 * @param hook is the function point of timer
 */
void rt_timer_enter_sethook(void (*hook)(struct rt_timer *timer))
{
    rt_timer_enter_hook = hook;
}

/**
 * @brief This function will set a hook function, which will be
 *        invoked when exit timer timeout callback function.
 *
 * @param hook is the function point of timer
 */
void rt_timer_exit_sethook(void (*hook)(struct rt_timer *timer))
{
    rt_timer_exit_hook = hook;
}

/**@}*/
#endif /* RT_USING_HOOK */


/**
 * @brief [internal] The init funtion of timer
 *
 *        The internal called function of rt_timer_init
 *
 * @see rt_timer_init
 *
 * @param timer is timer object
 *
 * @param timeout is the timeout function
 *
 * @param parameter is the parameter of timeout function
 *
 * @param time is the tick of timer
 *
 * @param flag the flag of timer
 */
/* 初始化定时器 */
static void _timer_init(rt_timer_t timer,
                        void (*timeout)(void *parameter),
                        void      *parameter,
                        rt_tick_t  time,
                        rt_uint8_t flag)
{
    int i;/* 临时变量 */

    /* 设置定时器标志 */
    timer->parent.flag  = flag;
    /* 设置定时器状态: 未激活 */
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    /* 设置定时器回调函数 */
    timer->timeout_func = timeout;
    /* 设置定时器回调函数参数  */
    timer->parameter    = parameter;
    /* 设置定时器'响铃'时间 */
    timer->timeout_tick = 0;
    /* 设置定时器'响铃'周期 */
    timer->init_tick    = time;
    /* 初始化定时器链表节点 */
    for (i = 0; i < RT_TIMER_SKIP_LIST_LEVEL; i++)
    {
        rt_list_init(&(timer->row[i]));/* 初始化定时器链表节点 */
    }
}

/**
 * @brief  Find the next emtpy timer ticks
 *
 * @param timer_list is the array of time list
 *
 * @param timeout_tick is the next timer's ticks
 *
 * @return  Return the operation status. If the return value is RT_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
/* 查找下一个响铃的定时器的响铃时间 */
static rt_err_t _timer_list_next_timeout(rt_list_t timer_list[], rt_tick_t *timeout_tick)
{
    struct rt_timer *timer;/* 定时器句柄 */
    rt_base_t level;/* 关中断保存值 */

    /* 关中断 */
    level = rt_hw_interrupt_disable();
    /* 检查定时器链表 */
    if (!rt_list_isempty(&timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1]))
    {
        /* 获取定时器句柄地址  */
        timer = rt_list_entry(timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1].next,struct rt_timer, row[RT_TIMER_SKIP_LIST_LEVEL - 1]);
        /* 保存响铃时间 */
        *timeout_tick = timer->timeout_tick;
        /* 开中断 */
        rt_hw_interrupt_enable(level);

        return RT_EOK;
    }
    /* 开中断 */
    rt_hw_interrupt_enable(level);

    return -RT_ERROR;
}

/**
 * @brief Remove the timer
 *
 * @param timer the point of the timer
 */
/* 移除定时器 */
rt_inline void _timer_remove(rt_timer_t timer)
{
    int i;

    for (i = 0; i < RT_TIMER_SKIP_LIST_LEVEL; i++)
    {
        /* 从链表中移除 */
        rt_list_remove(&timer->row[i]);
    }
}

/**
 * @addtogroup Clock
 */

/**@{*/

/**
 * @brief This function will initialize a timer
 *        normally this function is used to initialize a static timer object.
 *
 * @param timer is the point of timer
 *
 * @param name is a pointer to the name of the timer
 *
 * @param timeout is the callback of timer
 *
 * @param parameter is the param of the callback
 *
 * @param time is timeout ticks of timer
 *
 *             NOTE: The max timeout tick should be no more than (RT_TICK_MAX/2 - 1).
 *
 * @param flag is the flag of timer
 *
 */
/* 初始化一个定时器 */
void rt_timer_init(rt_timer_t  timer,
                   const char *name,
                   void (*timeout)(void *parameter),
                   void       *parameter,
                   rt_tick_t   time,
                   rt_uint8_t  flag)
{
    /* 检查参数 */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(timeout != RT_NULL);
    RT_ASSERT(time < RT_TICK_MAX / 2);
    /*初始化定时器对象 */
    rt_object_init(&(timer->parent), RT_Object_Class_Timer, name);
    /* 初始化定时器参数 */
    _timer_init(timer, timeout, parameter, time, flag);
}
RTM_EXPORT(rt_timer_init);

/**
 * @brief This function will detach a timer from timer management.
 *
 * @param timer is the timer to be detached
 *
 * @return the status of detach
 */
/* 脱离一个定时器 */
rt_err_t rt_timer_detach(rt_timer_t timer)
{
    /* 关中断返回值 */
    rt_base_t level;
    /* 检查参数 */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);
    RT_ASSERT(rt_object_is_systemobject(&timer->parent));
    /* 关中断 */
    level = rt_hw_interrupt_disable();
    /* 将定时器从链表移除 */
    _timer_remove(timer);
    /* 设置标志为:未激活 */
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    /* 开全局中断 */
    rt_hw_interrupt_enable(level);
    /* 将定时器的链表节点从定时器对象容器中 */
    rt_object_detach(&(timer->parent));

    return RT_EOK;
}
RTM_EXPORT(rt_timer_detach);

#ifdef RT_USING_HEAP
/**
 * @brief This function will create a timer
 *
 * @param name is the name of timer
 *
 * @param timeout is the timeout function
 *
 * @param parameter is the parameter of timeout function
 *
 * @param time is timeout ticks of the timer
 *
 *        NOTE: The max timeout tick should be no more than (RT_TICK_MAX/2 - 1).
 *
 * @param flag is the flag of timer. Timer will invoke the timeout function according to the selected values of flag, if one or more of the following flags is set.
 *
 *          RT_TIMER_FLAG_ONE_SHOT          One shot timing
 *          RT_TIMER_FLAG_PERIODIC          Periodic timing
 *
 *          RT_TIMER_FLAG_HARD_TIMER        Hardware timer
 *          RT_TIMER_FLAG_SOFT_TIMER        Software timer
 *
 *        NOTE:
 *        You can use multiple values with "|" logical operator.  By default, system will use the RT_TIME_FLAG_HARD_TIMER.
 *
 * @return the created timer object
 */
/* 动态创建定时器 */
rt_timer_t rt_timer_create(const char *name,
                           void (*timeout)(void *parameter),
                           void       *parameter,
                           rt_tick_t   time,
                           rt_uint8_t  flag)
{
    /* 定时器 */
    struct rt_timer *timer;
    /* 检查参数 */
    RT_ASSERT(timeout != RT_NULL);
    RT_ASSERT(time < RT_TICK_MAX / 2);
    /* 为定时器对象动态分配内存 */
    timer = (struct rt_timer *)rt_object_allocate(RT_Object_Class_Timer, name);
    if (timer == RT_NULL)
    {
        return RT_NULL;
    }
    /* 初始化定时器参数 */
    _timer_init(timer, timeout, parameter, time, flag);

    return timer;
}
RTM_EXPORT(rt_timer_create);

/**
 * @brief This function will delete a timer and release timer memory
 *
 * @param timer the timer to be deleted
 *
 * @return the operation status, RT_EOK on OK; RT_ERROR on error
 */
/* 删除定时器 */
rt_err_t rt_timer_delete(rt_timer_t timer)
{
    /* 关中断返回值 */
    rt_base_t level;
    /* 参数检查 */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);
    RT_ASSERT(rt_object_is_systemobject(&timer->parent) == RT_FALSE);
    /* 关中断 */
    level = rt_hw_interrupt_disable();
    /* 将定时器从链表移除 */
    _timer_remove(timer);
    /* 设置标志为~RT_TIMER_FLAG_ACTIVATED*/
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    /* 开中断 */
    rt_hw_interrupt_enable(level);
    /* 将定时器的链表节点从定时器对象容器中移除 */
    rt_object_delete(&(timer->parent));

    return RT_EOK;
}
RTM_EXPORT(rt_timer_delete);
#endif /* RT_USING_HEAP */

/**
 * @brief This function will start the timer
 *
 * @param timer the timer to be started
 *
 * @return the operation status, RT_EOK on OK, -RT_ERROR on error
 */
/* 启动定时器 */
rt_err_t rt_timer_start(rt_timer_t timer)
{
    unsigned int row_lvl;/* 跳表等级 */
    rt_list_t *timer_list;
    rt_base_t level;/* 关中断状态保存值 */
    rt_bool_t need_schedule;/* 调度状态 */
    /* 临时定时器链表节点 */
    rt_list_t *row_head[RT_TIMER_SKIP_LIST_LEVEL];/* 定时器链表头 */
    unsigned int tst_nr;
    static unsigned int random_nr;

    /* parameter check */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);

    need_schedule = RT_FALSE;

    /* 关中断 */
    level = rt_hw_interrupt_disable();
    /* 将定时器从链表中移除 */
    _timer_remove(timer);
    /* 改变定时器的状态:未激活 */
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    RT_OBJECT_HOOK_CALL(rt_object_take_hook, (&(timer->parent)));
    /* 设置定时器的超时时间 :当前时间加上定时时间 */
    timer->timeout_tick = rt_tick_get() + timer->init_tick;
    /* 开启软件定时器 */
#ifdef RT_USING_TIMER_SOFT
    /* 定时器标志为软件定时器 */
    if (timer->parent.flag & RT_TIMER_FLAG_SOFT_TIMER)
    {
        /* 设置定时器链表为软定时器 */
        timer_list = _soft_timer_list;
    }
    else
#endif /* RT_USING_TIMER_SOFT */
    {
        /* 设置定时器链表为硬定时器链表 */
        timer_list = _timer_list;
    }
    /* 初始化临时定时器链表节点为上述设置的定时器链表节点 软/硬 */
    row_head[0]  = &timer_list[0];
    /* 默认设置：下述循环仅执行 1 次 */
    for (row_lvl = 0; row_lvl < RT_TIMER_SKIP_LIST_LEVEL; row_lvl++)
    {
        /* 扫描定时器 */
        for (; row_head[row_lvl] != timer_list[row_lvl].prev;row_head[row_lvl]  = row_head[row_lvl]->next)
        {
            struct rt_timer *t;
            /* 获取第一个定时器的链表节点 */
            rt_list_t *p = row_head[row_lvl]->next;
            /* 查找定时器对象句柄地址 */
            t = rt_list_entry(p, struct rt_timer, row[row_lvl]);

            /* If we have two timers that timeout at the same time, it's
             * preferred that the timer inserted early get called early.
             * So insert the new timer to the end the the some-timeout timer
             * list.
             */
            if ((t->timeout_tick - timer->timeout_tick) == 0)
            {
                continue;
            }/* 超时则break */
            else if ((t->timeout_tick - timer->timeout_tick) < RT_TICK_MAX / 2)
            {
                break;
            }
        }/*  */
        if (row_lvl != RT_TIMER_SKIP_LIST_LEVEL - 1)
            row_head[row_lvl + 1] = row_head[row_lvl] + 1;
    }

    /* Interestingly, this super simple timer insert counter works very very
     * well on distributing the list height uniformly. By means of "very very
     * well", I mean it beats the randomness of timer->timeout_tick very easily
     * (actually, the timeout_tick is not random and easy to be attacked). */
    random_nr++;
    tst_nr = random_nr;
    /* 在排序后 将刚才的定时器插入到定时器链表中 */
    rt_list_insert_after(row_head[RT_TIMER_SKIP_LIST_LEVEL - 1], &(timer->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
    /* 下述循环并不会执行 */
    for (row_lvl = 2; row_lvl <= RT_TIMER_SKIP_LIST_LEVEL; row_lvl++)
    {
        if (!(tst_nr & RT_TIMER_SKIP_LIST_MASK))
            rt_list_insert_after(row_head[RT_TIMER_SKIP_LIST_LEVEL - row_lvl],
                                 &(timer->row[RT_TIMER_SKIP_LIST_LEVEL - row_lvl]));
        else
            break;
        /* Shift over the bits we have tested. Works well with 1 bit and 2
         * bits. */
        tst_nr >>= (RT_TIMER_SKIP_LIST_MASK + 1) >> 1;
    }
    /* 定时器启动并运行 */
    timer->parent.flag |= RT_TIMER_FLAG_ACTIVATED;

#ifdef RT_USING_TIMER_SOFT
    if (timer->parent.flag & RT_TIMER_FLAG_SOFT_TIMER)
    {
        /* 若软件定时器线程空闲且被挂起  */
        if ((_soft_timer_status == RT_SOFT_TIMER_IDLE) &&((_timer_thread.stat & RT_THREAD_SUSPEND_MASK) == RT_THREAD_SUSPEND_MASK))
        {
            /* 恢复定时器 */
            rt_thread_resume(&_timer_thread);
            need_schedule = RT_TRUE;/* 需要进行调度 */
        }
    }
#endif /* RT_USING_TIMER_SOFT */

    /* 关中断 */
    rt_hw_interrupt_enable(level);
    if (need_schedule)/* 需要进行调度 */
    {
        /* 执行调度 */
        rt_schedule();
    }
    return RT_EOK;
}
RTM_EXPORT(rt_timer_start);

/**
 * @brief This function will stop the timer
 *
 * @param timer the timer to be stopped
 *
 * @return the operation status, RT_EOK on OK, -RT_ERROR on error
 */
/* 停止定时器 */
rt_err_t rt_timer_stop(rt_timer_t timer)
{
    /* 关中断返回值 */
    rt_base_t level;
    /* 关中断 */
    level = rt_hw_interrupt_disable();
    /* 参数检查 */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);
    /* 重复操作关定时器 */
    if (!(timer->parent.flag & RT_TIMER_FLAG_ACTIVATED))
    {
        /* 开中断 */
        rt_hw_interrupt_enable(level);
        /* 返回错误 */
        return -RT_ERROR;
    }
    RT_OBJECT_HOOK_CALL(rt_object_put_hook, (&(timer->parent)));
    /* 将定时器从定时器链表移除 */
    _timer_remove(timer);
    /* 改变定时器状态:关闭 */
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    /* 开中断 */
    rt_hw_interrupt_enable(level);
    /* 返回OK */
    return RT_EOK;
}
RTM_EXPORT(rt_timer_stop);

/**
 * @brief This function will get or set some options of the timer
 *
 * @param timer the timer to be get or set
 * @param cmd the control command
 * @param arg the argument
 *
 * @return the statu of control
 */
/* 控制定时器 */
rt_err_t rt_timer_control(rt_timer_t timer, int cmd, void *arg)
{
    /* 关中断返回值 */
    rt_base_t level;
    /* 检查参数 */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);
    /* 关中断 */
    level = rt_hw_interrupt_disable();
    switch (cmd)
    {
    /* 获取定时周期 */
    case RT_TIMER_CTRL_GET_TIME:
        *(rt_tick_t *)arg = timer->init_tick;
        break;
        /* 设置定时周期 */
    case RT_TIMER_CTRL_SET_TIME:
        RT_ASSERT((*(rt_tick_t *)arg) < RT_TICK_MAX / 2);
        timer->init_tick = *(rt_tick_t *)arg;
        break;
        /* 设置为单次定时器 */
    case RT_TIMER_CTRL_SET_ONESHOT:
        timer->parent.flag &= ~RT_TIMER_FLAG_PERIODIC;
        break;
        /* 设置为周期定时器 */
    case RT_TIMER_CTRL_SET_PERIODIC:
        timer->parent.flag |= RT_TIMER_FLAG_PERIODIC;
        break;
        /* 获取定时器状态 */
    case RT_TIMER_CTRL_GET_STATE:
        if(timer->parent.flag & RT_TIMER_FLAG_ACTIVATED)
        {
            /* 定时器启动并运行 */
            *(rt_uint32_t *)arg = RT_TIMER_FLAG_ACTIVATED;
        }
        else
        {
            /* 定时器停止 */
            *(rt_uint32_t *)arg = RT_TIMER_FLAG_DEACTIVATED;
        }
        break;
        /* 获取定时器响铃时间 */
    case RT_TIMER_CTRL_GET_REMAIN_TIME:
        *(rt_tick_t *)arg =  timer->timeout_tick;
        break;
        /* 获取回调函数 */
    case RT_TIMER_CTRL_GET_FUNC:
        *(void **)arg = timer->timeout_func;
        break;
        /* 设置回调函数 */
    case RT_TIMER_CTRL_SET_FUNC:
        timer->timeout_func = (void (*)(void*))arg;
        break;
        /* 获取回调函数参数 */
    case RT_TIMER_CTRL_GET_PARM:
        *(void **)arg = timer->parameter;
        break;
        /* 设置回调函数参数 */
    case RT_TIMER_CTRL_SET_PARM:
        timer->parameter = arg;
        break;

    default:
        break;
    }
    /* 开中断 */
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}
RTM_EXPORT(rt_timer_control);

/**
 * @brief This function will check timer list, if a timeout event happens,
 *        the corresponding timeout function will be invoked.
 *
 * @note This function shall be invoked in operating system timer interrupt.
 */
/* 硬定时链表检查 在中断中调用  */
void rt_timer_check(void)
{

    struct rt_timer *t;
    rt_tick_t current_tick;
    rt_base_t level;
    rt_list_t list;
    /* 初始化一个定时器链表节点 */
    rt_list_init(&list);

    RT_DEBUG_LOG(RT_DEBUG_TIMER, ("timer check enter\n"));
    /* 获取当前系统节拍 */
    current_tick = rt_tick_get();

    /* 关全局中断  */
    level = rt_hw_interrupt_disable();
    /* 当硬定时器链表存在定时器节点 */
    while (!rt_list_isempty(&_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1]))
    {
        /* 获取排序后的首个定时器节点地址 */
        t = rt_list_entry(_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1].next,
                          struct rt_timer, row[RT_TIMER_SKIP_LIST_LEVEL - 1]);

        /*
         * It supposes that the new tick shall less than the half duration of
         * tick max.
         */
        /* 定时时间符合要求 */
        if ((current_tick - t->timeout_tick) < RT_TICK_MAX / 2)
        {
            RT_OBJECT_HOOK_CALL(rt_timer_enter_hook, (t));

            /* 首先将定时器的链表节点移除  */
            _timer_remove(t);
            if (!(t->parent.flag & RT_TIMER_FLAG_PERIODIC))
            {
                t->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
            }
            /* 将定时器节点插到临时定时器链表  */
            rt_list_insert_after(&list, &(t->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
            /* 调用超时函数  */
            t->timeout_func(t->parameter);

            /* 获取当前系统节拍 */
            current_tick = rt_tick_get();

            RT_OBJECT_HOOK_CALL(rt_timer_exit_hook, (t));
            RT_DEBUG_LOG(RT_DEBUG_TIMER, ("current tick: %d\n", current_tick));

            /* Check whether the timer object is detached or started again */
            if (rt_list_isempty(&list))
            {
                continue;
            }
            rt_list_remove(&(t->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
            if ((t->parent.flag & RT_TIMER_FLAG_PERIODIC) &&(t->parent.flag & RT_TIMER_FLAG_ACTIVATED))
            {
                /* start it */
                t->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
                rt_timer_start(t);
            }
        }
        else break;
    }

    /* 使能全局中断  */
    rt_hw_interrupt_enable(level);

    RT_DEBUG_LOG(RT_DEBUG_TIMER, ("timer check leave\n"));
}

/**
 * @brief This function will return the next timeout tick in the system.
 *
 * @return the next timeout tick in the system
 */
/* 查找即将超时的定时器 */
rt_tick_t rt_timer_next_timeout_tick(void)
{
    /* next_timeout为下一个要超时的定时器 RT_TICK_MAX = 0xffffffff */
    rt_tick_t next_timeout = RT_TICK_MAX;
    /* 查找下一个定时器的超时时间 并写入next_timeout*/
    _timer_list_next_timeout(_timer_list, &next_timeout);
    /* 返回超时时间 */
    return next_timeout;
}

#ifdef RT_USING_TIMER_SOFT
/**
 * @brief This function will check software-timer list, if a timeout event happens, the
 *        corresponding timeout function will be invoked.
 */
/* 软定时器扫描 *//* */
void rt_soft_timer_check(void)
{
    /* 当前时间 */
    rt_tick_t current_tick;
    struct rt_timer *t;
    /* 关中断返回值 */
    rt_base_t level;
    /* 临时定时器链表 */
    rt_list_t list;
    /* 初始化临时定时器链表 */
    rt_list_init(&list);

    RT_DEBUG_LOG(RT_DEBUG_TIMER, ("software timer check enter\n"));

    /* 关中断 */
    level = rt_hw_interrupt_disable();
    /* 若软件定时器链表非空 */
    while (!rt_list_isempty(&_soft_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1]))
    {
        /* 查找软件定时器的句柄 */
        t = rt_list_entry(_soft_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1].next,
                            struct rt_timer, row[RT_TIMER_SKIP_LIST_LEVEL - 1]);
        /* 获取系统当前的时间 */
        current_tick = rt_tick_get();
        /* 系统当前的时间大于超时时间 则第一个定时器超时*/
        if ((current_tick - t->timeout_tick) < RT_TICK_MAX / 2)
        {
            RT_OBJECT_HOOK_CALL(rt_timer_enter_hook, (t));

            /* 临时将定时器从软件定时器链表移除 */
            _timer_remove(t);
            /* 若软定时器标志为非周期定时器 */
            if (!(t->parent.flag & RT_TIMER_FLAG_PERIODIC))
            {
                /* 设置软定时器的状态为非激活*/
                t->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
            }
            /* 将该软件定时器链表插到临时链表中 */
            rt_list_insert_after(&list, &(t->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
            /* 设置软件定时器状态标志 */
            _soft_timer_status = RT_SOFT_TIMER_BUSY;
            /* 开中断 */
            rt_hw_interrupt_enable(level);

            /* 调用回调函数 */
            t->timeout_func(t->parameter);

            RT_OBJECT_HOOK_CALL(rt_timer_exit_hook, (t));
            RT_DEBUG_LOG(RT_DEBUG_TIMER, ("current tick: %d\n", current_tick));

            /* 关中断 */
            level = rt_hw_interrupt_disable();
            /* 重置软定时器状态 */
            _soft_timer_status = RT_SOFT_TIMER_IDLE;
            /* 检查链表是否非空 */
            if (rt_list_isempty(&list))
            {
                continue;
            }
            /* 将定时器链表从临时链表移除 */
            rt_list_remove(&(t->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
            /* 若定时器的状态为周期定时器 */
            if ((t->parent.flag & RT_TIMER_FLAG_PERIODIC) &&
                (t->parent.flag & RT_TIMER_FLAG_ACTIVATED))
            {
                /* 设置软定时器的状态为~RT_TIMER_FLAG_ACTIVATED */
                t->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
                /* 重新启动定时器 */
                rt_timer_start(t);
            }
        }
        else break; /* not check anymore */
    }
    /* 开中断 */
    rt_hw_interrupt_enable(level);

    RT_DEBUG_LOG(RT_DEBUG_TIMER, ("software timer check leave\n"));
}

/**
 * @brief System timer thread entry
 *
 * @param parameter is the arg of the thread
 */
/* 软件定时器线程入口 */
static void _timer_thread_entry(void *parameter)
{
    /* */
    rt_tick_t next_timeout;

    while (1)
    {
        /* 获取下一个定时器的超时时间 */
        if (_timer_list_next_timeout(_soft_timer_list, &next_timeout) != RT_EOK)
        {
            /* 不存在软件定时器 将线程挂起 */
            rt_thread_suspend_with_flag(rt_thread_self(), RT_UNINTERRUPTIBLE);
            /* 开启调度 */
            rt_schedule();
        }
        else
        {
            /* 存在软件定时器 */
            rt_tick_t current_tick;
            /* 获取系统当前的时间 */
            current_tick = rt_tick_get();
            /* 超时时间还未到 */
            if ((next_timeout - current_tick) < RT_TICK_MAX / 2)
            {
                /* 获取绝对超时时间 */
                next_timeout = next_timeout - current_tick;
                /* 延时相应时间 */
                rt_thread_delay(next_timeout);
            }
        }
        /* 检查软件定时器 */
        rt_soft_timer_check();
    }
}
#endif /* RT_USING_TIMER_SOFT */

/**
 * @ingroup SystemInit
 *
 * @brief This function will initialize system timer
 */
/* 硬定时器链表初始化 */
void rt_system_timer_init(void)
{
    rt_size_t i;
    /* 扫描硬定时器链表 */
    for (i = 0; i < sizeof(_timer_list) / sizeof(_timer_list[0]); i++)
    {
        rt_list_init(_timer_list + i);
    }
}

/**
 * @ingroup SystemInit
 *
 * @brief This function will initialize system timer thread
 */
/* 软定时器线程创建 */
void rt_system_timer_thread_init(void)
{
    /* 判断是否开启软定时器宏 */
#ifdef RT_USING_TIMER_SOFT
    int i;
    /* 扫描软定时器链表 */
    for (i = 0; i < sizeof(_soft_timer_list) / sizeof(_soft_timer_list[0]);i++)
    {
        /* 初始化软定时器链表 */
        rt_list_init(_soft_timer_list + i);
    }
    /* 创建软定时器 */
    rt_thread_init(&_timer_thread,
                   "timer",
                   _timer_thread_entry,
                   RT_NULL,
                   &_timer_thread_stack[0],
                   sizeof(_timer_thread_stack),
                   RT_TIMER_THREAD_PRIO,
                   10);

    /* 启动软定时器 */
    rt_thread_startup(&_timer_thread);
#endif /* RT_USING_TIMER_SOFT */
}

/**@}*/
