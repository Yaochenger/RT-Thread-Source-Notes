/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-02-24     Bernard      first version
 * 2006-05-03     Bernard      add IRQ_DEBUG
 * 2016-08-09     ArdaFu       add interrupt enter and leave hook.
 * 2018-11-22     Jesven       rt_interrupt_get_nest function add disable irq
 * 2021-08-15     Supperthomas fix the comment
 * 2022-01-07     Gabriel      Moving __on_rt_xxxxx_hook to irq.c
 * 2022-07-04     Yunjie       fix RT_DEBUG_LOG
 */

#include <rthw.h>
#include <rtthread.h>

#ifndef __on_rt_interrupt_enter_hook
    #define __on_rt_interrupt_enter_hook()          __ON_HOOK_ARGS(rt_interrupt_enter_hook, ())
#endif
#ifndef __on_rt_interrupt_leave_hook
    #define __on_rt_interrupt_leave_hook()          __ON_HOOK_ARGS(rt_interrupt_leave_hook, ())
#endif

#if defined(RT_USING_HOOK) && defined(RT_HOOK_USING_FUNC_PTR)

static void (*rt_interrupt_enter_hook)(void);
static void (*rt_interrupt_leave_hook)(void);

/**
 * @ingroup Hook
 *
 * @brief This function set a hook function when the system enter a interrupt
 *
 * @note The hook function must be simple and never be blocked or suspend.
 *
 * @param hook the function point to be called
 */
void rt_interrupt_enter_sethook(void (*hook)(void))
{
    rt_interrupt_enter_hook = hook;
}

/**
 * @ingroup Hook
 *
 * @brief This function set a hook function when the system exit a interrupt.
 *
 * @note The hook function must be simple and never be blocked or suspend.
 *
 * @param hook the function point to be called
 */
void rt_interrupt_leave_sethook(void (*hook)(void))
{
    rt_interrupt_leave_hook = hook;
}
#endif /* RT_USING_HOOK */

/**
 * @addtogroup Kernel
 */

/**@{*/

#ifdef RT_USING_SMP
#define rt_interrupt_nest rt_cpu_self()->irq_nest
#else
volatile rt_uint8_t rt_interrupt_nest = 0;
#endif /* RT_USING_SMP */


/**
 * @brief This function will be invoked by BSP, when enter interrupt service routine
 *
 * @note Please don't invoke this routine in application
 *
 * @see rt_interrupt_leave
 */
/* 通知内核进中断 */
rt_weak void rt_interrupt_enter(void)
{
    /* 调关中断返回值 */
    rt_base_t level;
    /* 关中断 */
    level = rt_hw_interrupt_disable();
    /* 中断锁个数 */
    rt_interrupt_nest ++;
    RT_OBJECT_HOOK_CALL(rt_interrupt_enter_hook,());
    /* 开中断 */
    rt_hw_interrupt_enable(level);

    RT_DEBUG_LOG(RT_DEBUG_IRQ, ("irq has come..., irq current nest:%d\n",
                                (rt_int32_t)rt_interrupt_nest));
}
RTM_EXPORT(rt_interrupt_enter);


/**
 * @brief This function will be invoked by BSP, when leave interrupt service routine
 *
 * @note Please don't invoke this routine in application
 *
 * @see rt_interrupt_enter
 */
/*通知内核离开中断 *//* */
rt_weak void rt_interrupt_leave(void)
{
    /* 关中断返回值 */
    rt_base_t level;

    RT_DEBUG_LOG(RT_DEBUG_IRQ, ("irq is going to leave, irq current nest:%d\n",
                                (rt_int32_t)rt_interrupt_nest));
    /* 关中断 */
    level = rt_hw_interrupt_disable();
    RT_OBJECT_HOOK_CALL(rt_interrupt_leave_hook,());
    /* 中断锁自减 */
    rt_interrupt_nest --;
    /* 开中断 */
    rt_hw_interrupt_enable(level);
}
RTM_EXPORT(rt_interrupt_leave);


/**
 * @brief This function will return the nest of interrupt.
 *
 * User application can invoke this function to get whether current
 * context is interrupt context.
 *
 * @return the number of nested interrupts.
 */
/* 获取关中断的次数 */
rt_weak rt_uint8_t rt_interrupt_get_nest(void)
{
    rt_uint8_t ret;
    rt_base_t level;
    /* 关中断 */
    level = rt_hw_interrupt_disable();
    /* 中断锁 */
    ret = rt_interrupt_nest;
    /* 开中断 */
    rt_hw_interrupt_enable(level);
    return ret;
}
RTM_EXPORT(rt_interrupt_get_nest);

RTM_EXPORT(rt_hw_interrupt_disable);
RTM_EXPORT(rt_hw_interrupt_enable);

/**@}*/

