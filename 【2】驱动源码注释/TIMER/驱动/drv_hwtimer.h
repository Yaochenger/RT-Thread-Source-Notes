/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-10     MXH          the first version
 */

#ifndef __DRV_HWTIMER_H__
#define __DRV_HWTIMER_H__

#include <rtthread.h>

#include "ch32v30x_tim.h"

#ifdef BSP_USING_HWTIMER

/* CH32定时器句柄 */
typedef struct
{
    /* CH32定时器句柄 */
    TIM_TypeDef *instance;
    /* 定时器属性 */
    TIM_TimeBaseInitTypeDef init;
    /* 定时器所在总线时钟 */
    rt_uint32_t rcc;

}TIM_HandleTypeDef;

struct ch32_hwtimer
{
    /* RT-Thread 定时器设备 */
    rt_hwtimer_t device;
    /* CH32定时器参数句柄 */
    TIM_HandleTypeDef handle;
    /* 中断源 */
    IRQn_Type irqn;
    /* CH32的定时器设备名称 */
    char *name;
};

/* TIM CONFIG */
/* 设备初始化配置
 * 最大频率
 * 最小频率
 * 最大计数值
 * 计数模式
 * */
#ifndef TIM_DEV_INFO_CONFIG
#define TIM_DEV_INFO_CONFIG                     \
    {                                           \
        .maxfreq = 1000000,                     \
        .minfreq = 3000,                        \
        .maxcnt  = 0xFFFF,                      \
        .cntmode = HWTIMER_CNTMODE_UP,          \
    }
#endif /* TIM_DEV_INFO_CONFIG */
/* 定时器1初始化配置
 *   设备时钟初始化 APB2时钟初始化
 *   设备中断号
 *   设备名称*/
#ifdef BSP_USING_TIM1
#define TIM1_CONFIG                         \
{                                           \
    .handle.instance = TIM1,                \
    .handle.rcc = RCC_APB2Periph_TIM1,      \
    .irqn = TIM1_UP_IRQn,                   \
    .name = "timer1",                       \
}
#endif /* BSP_USING_TIM1 */

#ifdef BSP_USING_TIM2
#define TIM2_CONFIG                         \
{                                           \
    .handle.instance = TIM2,                \
    .handle.rcc = RCC_APB1Periph_TIM2,      \
    .irqn = TIM2_IRQn,                      \
    .name = "timer2",                       \
}
#endif /* BSP_USING_TIM2 */

#endif /* BSP_USING_HWTIMER */
#endif /* __DRV_HWTIMER_H__ */
