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

/* CH32��ʱ����� */
typedef struct
{
    /* CH32��ʱ����� */
    TIM_TypeDef *instance;
    /* ��ʱ������ */
    TIM_TimeBaseInitTypeDef init;
    /* ��ʱ����������ʱ�� */
    rt_uint32_t rcc;

}TIM_HandleTypeDef;

struct ch32_hwtimer
{
    /* RT-Thread ��ʱ���豸 */
    rt_hwtimer_t device;
    /* CH32��ʱ��������� */
    TIM_HandleTypeDef handle;
    /* �ж�Դ */
    IRQn_Type irqn;
    /* CH32�Ķ�ʱ���豸���� */
    char *name;
};

/* TIM CONFIG */
/* �豸��ʼ������
 * ���Ƶ��
 * ��СƵ��
 * ������ֵ
 * ����ģʽ
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
/* ��ʱ��1��ʼ������
 *   �豸ʱ�ӳ�ʼ�� APB2ʱ�ӳ�ʼ��
 *   �豸�жϺ�
 *   �豸����*/
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
