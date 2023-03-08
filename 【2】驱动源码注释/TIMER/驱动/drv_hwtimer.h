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
/* CH32定时器句柄
 * 定时器基地址
 * 定时器属性
 * 时钟
 * */
typedef struct
{
    TIM_TypeDef *instance;
    TIM_TimeBaseInitTypeDef init;
    rt_uint32_t rcc;

}TIM_HandleTypeDef;

/*
 * CH32定时器驱动句柄
 * RT-Thread定时器设备
 * CH32定时器句柄
 *   中断号
 *  设备名称
 *  */
struct ch32_hwtimer
{
    rt_hwtimer_t device;
    TIM_HandleTypeDef handle;
    IRQn_Type irqn;
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
