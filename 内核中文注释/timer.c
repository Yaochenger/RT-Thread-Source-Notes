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
static rt_list_t _timer_list[RT_TIMER_SKIP_LIST_LEVEL]; /* Ӳ��ʱ������  */

#ifdef RT_USING_TIMER_SOFT           /* ʹ�������ʱ�� */

#define RT_SOFT_TIMER_IDLE              1 /* ��ʱ������״̬ */
#define RT_SOFT_TIMER_BUSY              0 /* ��ʱ��æ״̬ */

/* soft timer status */
static rt_uint8_t _soft_timer_status = RT_SOFT_TIMER_IDLE;/* ��ʼ�������ʱ��״̬:����*/
/* soft timer list */
static rt_list_t _soft_timer_list[RT_TIMER_SKIP_LIST_LEVEL];/* �����ʱ������ */
static struct rt_thread _timer_thread;/* �����ʱ���߳� */
ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t _timer_thread_stack[RT_TIMER_THREAD_STACK_SIZE];/* �����ʱ��ջ*/
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
/* ��ʼ����ʱ�� */
static void _timer_init(rt_timer_t timer,
                        void (*timeout)(void *parameter),
                        void      *parameter,
                        rt_tick_t  time,
                        rt_uint8_t flag)
{
    int i;/* ��ʱ���� */

    /* ���ö�ʱ����־ */
    timer->parent.flag  = flag;
    /* ���ö�ʱ��״̬: δ���� */
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    /* ���ö�ʱ���ص����� */
    timer->timeout_func = timeout;
    /* ���ö�ʱ���ص���������  */
    timer->parameter    = parameter;
    /* ���ö�ʱ��'����'ʱ�� */
    timer->timeout_tick = 0;
    /* ���ö�ʱ��'����'���� */
    timer->init_tick    = time;
    /* ��ʼ����ʱ������ڵ� */
    for (i = 0; i < RT_TIMER_SKIP_LIST_LEVEL; i++)
    {
        rt_list_init(&(timer->row[i]));/* ��ʼ����ʱ������ڵ� */
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
/* ������һ������Ķ�ʱ��������ʱ�� */
static rt_err_t _timer_list_next_timeout(rt_list_t timer_list[], rt_tick_t *timeout_tick)
{
    struct rt_timer *timer;/* ��ʱ����� */
    rt_base_t level;/* ���жϱ���ֵ */

    /* ���ж� */
    level = rt_hw_interrupt_disable();
    /* ��鶨ʱ������ */
    if (!rt_list_isempty(&timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1]))
    {
        /* ��ȡ��ʱ�������ַ  */
        timer = rt_list_entry(timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1].next,struct rt_timer, row[RT_TIMER_SKIP_LIST_LEVEL - 1]);
        /* ��������ʱ�� */
        *timeout_tick = timer->timeout_tick;
        /* ���ж� */
        rt_hw_interrupt_enable(level);

        return RT_EOK;
    }
    /* ���ж� */
    rt_hw_interrupt_enable(level);

    return -RT_ERROR;
}

/**
 * @brief Remove the timer
 *
 * @param timer the point of the timer
 */
/* �Ƴ���ʱ�� */
rt_inline void _timer_remove(rt_timer_t timer)
{
    int i;

    for (i = 0; i < RT_TIMER_SKIP_LIST_LEVEL; i++)
    {
        /* ���������Ƴ� */
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
/* ��ʼ��һ����ʱ�� */
void rt_timer_init(rt_timer_t  timer,
                   const char *name,
                   void (*timeout)(void *parameter),
                   void       *parameter,
                   rt_tick_t   time,
                   rt_uint8_t  flag)
{
    /* ������ */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(timeout != RT_NULL);
    RT_ASSERT(time < RT_TICK_MAX / 2);
    /*��ʼ����ʱ������ */
    rt_object_init(&(timer->parent), RT_Object_Class_Timer, name);
    /* ��ʼ����ʱ������ */
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
/* ����һ����ʱ�� */
rt_err_t rt_timer_detach(rt_timer_t timer)
{
    /* ���жϷ���ֵ */
    rt_base_t level;
    /* ������ */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);
    RT_ASSERT(rt_object_is_systemobject(&timer->parent));
    /* ���ж� */
    level = rt_hw_interrupt_disable();
    /* ����ʱ���������Ƴ� */
    _timer_remove(timer);
    /* ���ñ�־Ϊ:δ���� */
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    /* ��ȫ���ж� */
    rt_hw_interrupt_enable(level);
    /* ����ʱ��������ڵ�Ӷ�ʱ������������ */
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
/* ��̬������ʱ�� */
rt_timer_t rt_timer_create(const char *name,
                           void (*timeout)(void *parameter),
                           void       *parameter,
                           rt_tick_t   time,
                           rt_uint8_t  flag)
{
    /* ��ʱ�� */
    struct rt_timer *timer;
    /* ������ */
    RT_ASSERT(timeout != RT_NULL);
    RT_ASSERT(time < RT_TICK_MAX / 2);
    /* Ϊ��ʱ������̬�����ڴ� */
    timer = (struct rt_timer *)rt_object_allocate(RT_Object_Class_Timer, name);
    if (timer == RT_NULL)
    {
        return RT_NULL;
    }
    /* ��ʼ����ʱ������ */
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
/* ɾ����ʱ�� */
rt_err_t rt_timer_delete(rt_timer_t timer)
{
    /* ���жϷ���ֵ */
    rt_base_t level;
    /* ������� */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);
    RT_ASSERT(rt_object_is_systemobject(&timer->parent) == RT_FALSE);
    /* ���ж� */
    level = rt_hw_interrupt_disable();
    /* ����ʱ���������Ƴ� */
    _timer_remove(timer);
    /* ���ñ�־Ϊ~RT_TIMER_FLAG_ACTIVATED*/
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    /* ���ж� */
    rt_hw_interrupt_enable(level);
    /* ����ʱ��������ڵ�Ӷ�ʱ�������������Ƴ� */
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
/* ������ʱ�� */
rt_err_t rt_timer_start(rt_timer_t timer)
{
    unsigned int row_lvl;/* ����ȼ� */
    rt_list_t *timer_list;
    rt_base_t level;/* ���ж�״̬����ֵ */
    rt_bool_t need_schedule;/* ����״̬ */
    /* ��ʱ��ʱ������ڵ� */
    rt_list_t *row_head[RT_TIMER_SKIP_LIST_LEVEL];/* ��ʱ������ͷ */
    unsigned int tst_nr;
    static unsigned int random_nr;

    /* parameter check */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);

    need_schedule = RT_FALSE;

    /* ���ж� */
    level = rt_hw_interrupt_disable();
    /* ����ʱ�����������Ƴ� */
    _timer_remove(timer);
    /* �ı䶨ʱ����״̬:δ���� */
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    RT_OBJECT_HOOK_CALL(rt_object_take_hook, (&(timer->parent)));
    /* ���ö�ʱ���ĳ�ʱʱ�� :��ǰʱ����϶�ʱʱ�� */
    timer->timeout_tick = rt_tick_get() + timer->init_tick;
    /* ���������ʱ�� */
#ifdef RT_USING_TIMER_SOFT
    /* ��ʱ����־Ϊ�����ʱ�� */
    if (timer->parent.flag & RT_TIMER_FLAG_SOFT_TIMER)
    {
        /* ���ö�ʱ������Ϊ��ʱ�� */
        timer_list = _soft_timer_list;
    }
    else
#endif /* RT_USING_TIMER_SOFT */
    {
        /* ���ö�ʱ������ΪӲ��ʱ������ */
        timer_list = _timer_list;
    }
    /* ��ʼ����ʱ��ʱ������ڵ�Ϊ�������õĶ�ʱ������ڵ� ��/Ӳ */
    row_head[0]  = &timer_list[0];
    /* Ĭ�����ã�����ѭ����ִ�� 1 �� */
    for (row_lvl = 0; row_lvl < RT_TIMER_SKIP_LIST_LEVEL; row_lvl++)
    {
        /* ɨ�趨ʱ�� */
        for (; row_head[row_lvl] != timer_list[row_lvl].prev;row_head[row_lvl]  = row_head[row_lvl]->next)
        {
            struct rt_timer *t;
            /* ��ȡ��һ����ʱ��������ڵ� */
            rt_list_t *p = row_head[row_lvl]->next;
            /* ���Ҷ�ʱ����������ַ */
            t = rt_list_entry(p, struct rt_timer, row[row_lvl]);

            /* If we have two timers that timeout at the same time, it's
             * preferred that the timer inserted early get called early.
             * So insert the new timer to the end the the some-timeout timer
             * list.
             */
            if ((t->timeout_tick - timer->timeout_tick) == 0)
            {
                continue;
            }/* ��ʱ��break */
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
    /* ������� ���ղŵĶ�ʱ�����뵽��ʱ�������� */
    rt_list_insert_after(row_head[RT_TIMER_SKIP_LIST_LEVEL - 1], &(timer->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
    /* ����ѭ��������ִ�� */
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
    /* ��ʱ������������ */
    timer->parent.flag |= RT_TIMER_FLAG_ACTIVATED;

#ifdef RT_USING_TIMER_SOFT
    if (timer->parent.flag & RT_TIMER_FLAG_SOFT_TIMER)
    {
        /* �������ʱ���߳̿����ұ�����  */
        if ((_soft_timer_status == RT_SOFT_TIMER_IDLE) &&((_timer_thread.stat & RT_THREAD_SUSPEND_MASK) == RT_THREAD_SUSPEND_MASK))
        {
            /* �ָ���ʱ�� */
            rt_thread_resume(&_timer_thread);
            need_schedule = RT_TRUE;/* ��Ҫ���е��� */
        }
    }
#endif /* RT_USING_TIMER_SOFT */

    /* ���ж� */
    rt_hw_interrupt_enable(level);
    if (need_schedule)/* ��Ҫ���е��� */
    {
        /* ִ�е��� */
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
/* ֹͣ��ʱ�� */
rt_err_t rt_timer_stop(rt_timer_t timer)
{
    /* ���жϷ���ֵ */
    rt_base_t level;
    /* ���ж� */
    level = rt_hw_interrupt_disable();
    /* ������� */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);
    /* �ظ������ض�ʱ�� */
    if (!(timer->parent.flag & RT_TIMER_FLAG_ACTIVATED))
    {
        /* ���ж� */
        rt_hw_interrupt_enable(level);
        /* ���ش��� */
        return -RT_ERROR;
    }
    RT_OBJECT_HOOK_CALL(rt_object_put_hook, (&(timer->parent)));
    /* ����ʱ���Ӷ�ʱ�������Ƴ� */
    _timer_remove(timer);
    /* �ı䶨ʱ��״̬:�ر� */
    timer->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
    /* ���ж� */
    rt_hw_interrupt_enable(level);
    /* ����OK */
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
/* ���ƶ�ʱ�� */
rt_err_t rt_timer_control(rt_timer_t timer, int cmd, void *arg)
{
    /* ���жϷ���ֵ */
    rt_base_t level;
    /* ������ */
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);
    /* ���ж� */
    level = rt_hw_interrupt_disable();
    switch (cmd)
    {
    /* ��ȡ��ʱ���� */
    case RT_TIMER_CTRL_GET_TIME:
        *(rt_tick_t *)arg = timer->init_tick;
        break;
        /* ���ö�ʱ���� */
    case RT_TIMER_CTRL_SET_TIME:
        RT_ASSERT((*(rt_tick_t *)arg) < RT_TICK_MAX / 2);
        timer->init_tick = *(rt_tick_t *)arg;
        break;
        /* ����Ϊ���ζ�ʱ�� */
    case RT_TIMER_CTRL_SET_ONESHOT:
        timer->parent.flag &= ~RT_TIMER_FLAG_PERIODIC;
        break;
        /* ����Ϊ���ڶ�ʱ�� */
    case RT_TIMER_CTRL_SET_PERIODIC:
        timer->parent.flag |= RT_TIMER_FLAG_PERIODIC;
        break;
        /* ��ȡ��ʱ��״̬ */
    case RT_TIMER_CTRL_GET_STATE:
        if(timer->parent.flag & RT_TIMER_FLAG_ACTIVATED)
        {
            /* ��ʱ������������ */
            *(rt_uint32_t *)arg = RT_TIMER_FLAG_ACTIVATED;
        }
        else
        {
            /* ��ʱ��ֹͣ */
            *(rt_uint32_t *)arg = RT_TIMER_FLAG_DEACTIVATED;
        }
        break;
        /* ��ȡ��ʱ������ʱ�� */
    case RT_TIMER_CTRL_GET_REMAIN_TIME:
        *(rt_tick_t *)arg =  timer->timeout_tick;
        break;
        /* ��ȡ�ص����� */
    case RT_TIMER_CTRL_GET_FUNC:
        *(void **)arg = timer->timeout_func;
        break;
        /* ���ûص����� */
    case RT_TIMER_CTRL_SET_FUNC:
        timer->timeout_func = (void (*)(void*))arg;
        break;
        /* ��ȡ�ص��������� */
    case RT_TIMER_CTRL_GET_PARM:
        *(void **)arg = timer->parameter;
        break;
        /* ���ûص��������� */
    case RT_TIMER_CTRL_SET_PARM:
        timer->parameter = arg;
        break;

    default:
        break;
    }
    /* ���ж� */
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
/* Ӳ��ʱ������ ���ж��е���  */
void rt_timer_check(void)
{

    struct rt_timer *t;
    rt_tick_t current_tick;
    rt_base_t level;
    rt_list_t list;
    /* ��ʼ��һ����ʱ������ڵ� */
    rt_list_init(&list);

    RT_DEBUG_LOG(RT_DEBUG_TIMER, ("timer check enter\n"));
    /* ��ȡ��ǰϵͳ���� */
    current_tick = rt_tick_get();

    /* ��ȫ���ж�  */
    level = rt_hw_interrupt_disable();
    /* ��Ӳ��ʱ��������ڶ�ʱ���ڵ� */
    while (!rt_list_isempty(&_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1]))
    {
        /* ��ȡ�������׸���ʱ���ڵ��ַ */
        t = rt_list_entry(_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1].next,
                          struct rt_timer, row[RT_TIMER_SKIP_LIST_LEVEL - 1]);

        /*
         * It supposes that the new tick shall less than the half duration of
         * tick max.
         */
        /* ��ʱʱ�����Ҫ�� */
        if ((current_tick - t->timeout_tick) < RT_TICK_MAX / 2)
        {
            RT_OBJECT_HOOK_CALL(rt_timer_enter_hook, (t));

            /* ���Ƚ���ʱ��������ڵ��Ƴ�  */
            _timer_remove(t);
            if (!(t->parent.flag & RT_TIMER_FLAG_PERIODIC))
            {
                t->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
            }
            /* ����ʱ���ڵ�嵽��ʱ��ʱ������  */
            rt_list_insert_after(&list, &(t->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
            /* ���ó�ʱ����  */
            t->timeout_func(t->parameter);

            /* ��ȡ��ǰϵͳ���� */
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

    /* ʹ��ȫ���ж�  */
    rt_hw_interrupt_enable(level);

    RT_DEBUG_LOG(RT_DEBUG_TIMER, ("timer check leave\n"));
}

/**
 * @brief This function will return the next timeout tick in the system.
 *
 * @return the next timeout tick in the system
 */
/* ���Ҽ�����ʱ�Ķ�ʱ�� */
rt_tick_t rt_timer_next_timeout_tick(void)
{
    /* next_timeoutΪ��һ��Ҫ��ʱ�Ķ�ʱ�� RT_TICK_MAX = 0xffffffff */
    rt_tick_t next_timeout = RT_TICK_MAX;
    /* ������һ����ʱ���ĳ�ʱʱ�� ��д��next_timeout*/
    _timer_list_next_timeout(_timer_list, &next_timeout);
    /* ���س�ʱʱ�� */
    return next_timeout;
}

#ifdef RT_USING_TIMER_SOFT
/**
 * @brief This function will check software-timer list, if a timeout event happens, the
 *        corresponding timeout function will be invoked.
 */
/* ��ʱ��ɨ�� *//* */
void rt_soft_timer_check(void)
{
    /* ��ǰʱ�� */
    rt_tick_t current_tick;
    struct rt_timer *t;
    /* ���жϷ���ֵ */
    rt_base_t level;
    /* ��ʱ��ʱ������ */
    rt_list_t list;
    /* ��ʼ����ʱ��ʱ������ */
    rt_list_init(&list);

    RT_DEBUG_LOG(RT_DEBUG_TIMER, ("software timer check enter\n"));

    /* ���ж� */
    level = rt_hw_interrupt_disable();
    /* �������ʱ������ǿ� */
    while (!rt_list_isempty(&_soft_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1]))
    {
        /* ���������ʱ���ľ�� */
        t = rt_list_entry(_soft_timer_list[RT_TIMER_SKIP_LIST_LEVEL - 1].next,
                            struct rt_timer, row[RT_TIMER_SKIP_LIST_LEVEL - 1]);
        /* ��ȡϵͳ��ǰ��ʱ�� */
        current_tick = rt_tick_get();
        /* ϵͳ��ǰ��ʱ����ڳ�ʱʱ�� ���һ����ʱ����ʱ*/
        if ((current_tick - t->timeout_tick) < RT_TICK_MAX / 2)
        {
            RT_OBJECT_HOOK_CALL(rt_timer_enter_hook, (t));

            /* ��ʱ����ʱ���������ʱ�������Ƴ� */
            _timer_remove(t);
            /* ����ʱ����־Ϊ�����ڶ�ʱ�� */
            if (!(t->parent.flag & RT_TIMER_FLAG_PERIODIC))
            {
                /* ������ʱ����״̬Ϊ�Ǽ���*/
                t->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
            }
            /* ���������ʱ������嵽��ʱ������ */
            rt_list_insert_after(&list, &(t->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
            /* ���������ʱ��״̬��־ */
            _soft_timer_status = RT_SOFT_TIMER_BUSY;
            /* ���ж� */
            rt_hw_interrupt_enable(level);

            /* ���ûص����� */
            t->timeout_func(t->parameter);

            RT_OBJECT_HOOK_CALL(rt_timer_exit_hook, (t));
            RT_DEBUG_LOG(RT_DEBUG_TIMER, ("current tick: %d\n", current_tick));

            /* ���ж� */
            level = rt_hw_interrupt_disable();
            /* ������ʱ��״̬ */
            _soft_timer_status = RT_SOFT_TIMER_IDLE;
            /* ��������Ƿ�ǿ� */
            if (rt_list_isempty(&list))
            {
                continue;
            }
            /* ����ʱ���������ʱ�����Ƴ� */
            rt_list_remove(&(t->row[RT_TIMER_SKIP_LIST_LEVEL - 1]));
            /* ����ʱ����״̬Ϊ���ڶ�ʱ�� */
            if ((t->parent.flag & RT_TIMER_FLAG_PERIODIC) &&
                (t->parent.flag & RT_TIMER_FLAG_ACTIVATED))
            {
                /* ������ʱ����״̬Ϊ~RT_TIMER_FLAG_ACTIVATED */
                t->parent.flag &= ~RT_TIMER_FLAG_ACTIVATED;
                /* ����������ʱ�� */
                rt_timer_start(t);
            }
        }
        else break; /* not check anymore */
    }
    /* ���ж� */
    rt_hw_interrupt_enable(level);

    RT_DEBUG_LOG(RT_DEBUG_TIMER, ("software timer check leave\n"));
}

/**
 * @brief System timer thread entry
 *
 * @param parameter is the arg of the thread
 */
/* �����ʱ���߳���� */
static void _timer_thread_entry(void *parameter)
{
    /* */
    rt_tick_t next_timeout;

    while (1)
    {
        /* ��ȡ��һ����ʱ���ĳ�ʱʱ�� */
        if (_timer_list_next_timeout(_soft_timer_list, &next_timeout) != RT_EOK)
        {
            /* �����������ʱ�� ���̹߳��� */
            rt_thread_suspend_with_flag(rt_thread_self(), RT_UNINTERRUPTIBLE);
            /* �������� */
            rt_schedule();
        }
        else
        {
            /* ���������ʱ�� */
            rt_tick_t current_tick;
            /* ��ȡϵͳ��ǰ��ʱ�� */
            current_tick = rt_tick_get();
            /* ��ʱʱ�仹δ�� */
            if ((next_timeout - current_tick) < RT_TICK_MAX / 2)
            {
                /* ��ȡ���Գ�ʱʱ�� */
                next_timeout = next_timeout - current_tick;
                /* ��ʱ��Ӧʱ�� */
                rt_thread_delay(next_timeout);
            }
        }
        /* ��������ʱ�� */
        rt_soft_timer_check();
    }
}
#endif /* RT_USING_TIMER_SOFT */

/**
 * @ingroup SystemInit
 *
 * @brief This function will initialize system timer
 */
/* Ӳ��ʱ�������ʼ�� */
void rt_system_timer_init(void)
{
    rt_size_t i;
    /* ɨ��Ӳ��ʱ������ */
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
/* ��ʱ���̴߳��� */
void rt_system_timer_thread_init(void)
{
    /* �ж��Ƿ�����ʱ���� */
#ifdef RT_USING_TIMER_SOFT
    int i;
    /* ɨ����ʱ������ */
    for (i = 0; i < sizeof(_soft_timer_list) / sizeof(_soft_timer_list[0]);i++)
    {
        /* ��ʼ����ʱ������ */
        rt_list_init(_soft_timer_list + i);
    }
    /* ������ʱ�� */
    rt_thread_init(&_timer_thread,
                   "timer",
                   _timer_thread_entry,
                   RT_NULL,
                   &_timer_thread_stack[0],
                   sizeof(_timer_thread_stack),
                   RT_TIMER_THREAD_PRIO,
                   10);

    /* ������ʱ�� */
    rt_thread_startup(&_timer_thread);
#endif /* RT_USING_TIMER_SOFT */
}

/**@}*/
