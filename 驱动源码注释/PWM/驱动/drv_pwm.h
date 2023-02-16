/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-10-20     MXH          the first version
 */

#ifndef __DRV_PWM_H__
#define __DRV_PWM_H__

#include <rtthread.h>
#ifdef BSP_USING_PWM
#include "ch32v30x_tim.h"
#include <drivers/rt_drv_pwm.h>
#include <drivers/hwtimer.h>
#include <board.h>

/* 计算设备个数 */
#ifndef ITEM_NUM
#define ITEM_NUM(items) sizeof(items) / sizeof(items[0])
#endif

#define MAX_COUNTER     65535
#define MIN_COUNTER     2
#define MIN_PULSE       2
#define FLAG_NOT_INIT   0xFF

/* PWM设备 */
struct rtdevice_pwm_device
{
    struct rt_device_pwm parent;/* RT-Thread PWM设备框架 */
    TIM_TypeDef* periph;        /* PWM寄存器基地址 */
    rt_uint8_t channel[4];      /* PWN输出通道 */
    char* name;                 /* PWM设备名称 */
};

#endif/* BSP_USING_PWM */

#endif/* __DRV_PWM_H__ */
