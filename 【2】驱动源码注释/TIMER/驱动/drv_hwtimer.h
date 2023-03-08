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
/* CH32��ʱ�����
 * ��ʱ������ַ
 * ��ʱ������
 * ʱ��
 * */
typedef struct
{
    TIM_TypeDef *instance;
    TIM_TimeBaseInitTypeDef init;
    rt_uint32_t rcc;

}TIM_HandleTypeDef;

/*
 * CH32��ʱ���������
 * RT-Thread��ʱ���豸
 * CH32��ʱ�����
 *   �жϺ�
 *  �豸����
 *  */
struct ch32_hwtimer
{
    rt_hwtimer_t device;
    TIM_HandleTypeDef handle;
    IRQn_Type irqn;
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
