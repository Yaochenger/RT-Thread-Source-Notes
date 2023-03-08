/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-10     MXH          the first version
 */

#include <rtthread.h>
#include <rtdevice.h>

#ifdef BSP_USING_HWTIMER
#define DBG_TAG               "TIM"
#define DBG_LVL               DBG_LOG
#include <rtdbg.h>

#include "drv_hwtimer.h"
#include "board.h"

#ifdef RT_USING_HWTIMER
/* CH32ʹ�ܵ��豸���� */
enum
{
#ifdef BSP_USING_TIM1
    TIM1_INDEX,
#endif
#ifdef BSP_USING_TIM2
    TIM2_INDEX,
#endif

};
/* ��ʱ���豸��ʼ�� */
static struct ch32_hwtimer ch32_hwtimer_obj[] =
{
#ifdef BSP_USING_TIM1
    TIM1_CONFIG,
#endif

#ifdef BSP_USING_TIM2
    TIM2_CONFIG,
#endif

};

/* APBx timer clocks frequency doubler state related to APB1CLKDivider value */

/* CH32���߱�Ƶϵ�� */
void ch32_get_pclk_doubler(rt_uint32_t *pclk1_doubler, rt_uint32_t *pclk2_doubler)
{
    RT_ASSERT(pclk1_doubler != RT_NULL);
    RT_ASSERT(pclk2_doubler != RT_NULL);

    *pclk1_doubler = 1;
    *pclk2_doubler = 1;
    /* �ж϶�ʱ�����ڵ�ʱ�����ߵķ�Ƶϵ�� */
    if((RCC->CFGR0 & RCC_PPRE1) == RCC_PPRE1_DIV1)
    {   /* ����Ƶ */
        *pclk1_doubler = 1;
    }
    else
    {   /* ������Ƶϵ�� */
        *pclk1_doubler = 2;
    }

    if((RCC->CFGR0 & RCC_PPRE2) == RCC_PPRE2_DIV1)
    {
        /* ����Ƶ */
        *pclk2_doubler = 1;
    }
    else
    {
        *pclk2_doubler = 2;
    }
}
/* ��ʱ����ʼ�� */
static void ch32_hwtimer_init(struct rt_hwtimer_device *timer, rt_uint32_t state)
{
    RT_ASSERT(timer != RT_NULL);
    TIM_HandleTypeDef *tim = RT_NULL;
    RCC_ClocksTypeDef RCC_ClockStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    /* ��ʱ��ʱ���豸 */
    struct ch32_hwtimer *tim_device = RT_NULL;
    /* ��Ƶϵ�� */
    rt_uint32_t prescaler_value = 0;
    /* ��Ƶϵ�� */
    rt_uint32_t pclk1_doubler, pclk2_doubler;
    /* ��ȡCH32ʱ��Ƶ�� */
    RCC_GetClocksFreq(&RCC_ClockStruct);
    /* ��ȡ��Ƶϵ�� */
    ch32_get_pclk_doubler(&pclk1_doubler, &pclk2_doubler);
    /* */
    if(state)
    {
        /* ��ȡCH32��ʱ�����  */
        tim = (TIM_HandleTypeDef *)timer->parent.user_data;
        /* ǿ������ת�� */
        tim_device = (struct ch32_hwtimer *)timer;
        /* �ж϶�ʱ������ַ  */
        if(tim->instance == TIM1 || tim->instance == TIM8 ||
                tim->instance == TIM9 || tim->instance == TIM10)
        {
            /* ʹ�ܶ�ʱ��ʱ�� */
            RCC_APB2PeriphClockCmd(tim->rcc, ENABLE);
            /* ���÷�Ƶϵ�� */
            prescaler_value = (RCC_ClockStruct.PCLK2_Frequency * pclk2_doubler / 10000) - 1;
        }
        else
        {
            /* */
            RCC_APB1PeriphClockCmd(tim->rcc, ENABLE);
            prescaler_value = (RCC_ClockStruct.PCLK1_Frequency * pclk1_doubler / 10000) - 1;
        }
        /* ��ʼ����Ƶϵ�� */
        tim->init.TIM_Prescaler = prescaler_value;
        /* ����ʱ�ӷָ� �����벶�������˲���� */
        tim->init.TIM_ClockDivision = TIM_CKD_DIV1;
        /* ʱ������ */
        tim->init.TIM_Period = 10000 - 1;
        /* ��ʱ���� ���յ���ʱ����Ϊ T * TIM_RepetitionCounter  */
        tim->init.TIM_RepetitionCounter = 0;
        /* ����ģʽ */
        if(timer->info->cntmode == HWTIMER_CNTMODE_UP)
        {
            tim->init.TIM_CounterMode = TIM_CounterMode_Up;
        }
        else
        {
            tim->init.TIM_CounterMode   = TIM_CounterMode_Down;
        }

        /* TIM6 and TIM7 only support counter up mode */
        if(tim->instance == TIM6 || tim->instance == TIM7)
        {
            tim->init.TIM_CounterMode = TIM_CounterMode_Up;
        }
        /* ʹ��ǰ�����õĲ��� ��ʼ����ʱ�� */
        TIM_TimeBaseInit(tim->instance, &tim->init);

        /* �ж�������� */
        NVIC_InitStruct.NVIC_IRQChannel = tim_device->irqn;/* �ж�Դ */
        NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;/* �ж����ȼ� */
        NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;/* �ж����ȼ� */
        NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;/* �ж�ʹ��  */
        NVIC_Init(&NVIC_InitStruct);

        /* ����жϱ�־λ   */
        TIM_ClearITPendingBit(tim->instance, TIM_IT_Update);
        /* �жϸ���ʹ��  */
        TIM_ITConfig(tim->instance, TIM_IT_Update, ENABLE);
    }
}

/* Ӳ����ʱ������ */
static rt_err_t ch32_hwtimer_start(struct rt_hwtimer_device *timer, rt_uint32_t cnt, rt_hwtimer_mode_t mode)
{
    RT_ASSERT(timer != RT_NULL);
    TIM_HandleTypeDef *tim = RT_NULL;
    /* ��ȡCH32��ʱ���豸��� */
    tim = (TIM_HandleTypeDef *)timer->parent.user_data;

    /* ��ʼ����ʱ�� ����ֵ */
    tim->instance->CNT = 0;
    /* ���ö�ʱ���Զ���װ��ֵ */
    tim->instance->ATRLR = cnt - 1;
    /* ��ʼ�Ķ�ʱ������ */
    tim->init.TIM_Period = cnt - 1;
    /* �ж϶�ʱ��ģʽ */
    if (mode == HWTIMER_MODE_ONESHOT)
    {
        /* set timer to single mode */
        tim->instance->CTLR1 &= (uint16_t) ~((uint16_t)TIM_OPM);
        tim->instance->CTLR1 |= TIM_OPMode_Single;
    }
    else
    {
        tim->instance->CTLR1 &= (uint16_t) ~((uint16_t)TIM_OPM);
        tim->instance->CTLR1 |= TIM_OPMode_Repetitive;
    }

    /* ������ʱ��  */
    TIM_Cmd(tim->instance, ENABLE);

    return RT_EOK;
}
/* ֹͣ��ʱ�� */
static void ch32_hwtimer_stop(struct rt_hwtimer_device *timer)
{
    RT_ASSERT(timer != RT_NULL);
    /**/
    TIM_HandleTypeDef *tim = RT_NULL;
    /* ��ȡCH32��ʱ����� */
    tim = (TIM_HandleTypeDef *)timer->parent.user_data;

    /* ֹͣ��ʱ��  */
    TIM_Cmd(tim->instance, DISABLE);

    /* ��ʱ������ֵ����  */
    tim->instance->CNT = 0;
}

/* ��ȡ��ʱ������ֵ */
static rt_uint32_t ch32_hwtimer_count_get(struct rt_hwtimer_device *timer)
{
    RT_ASSERT(timer != RT_NULL);
    TIM_HandleTypeDef *tim = RT_NULL;
    tim = (TIM_HandleTypeDef *)timer->parent.user_data;

    return tim->instance->CNT;
}
/* ��ʱ������ */
static rt_err_t ch32_hwtimer_control(struct rt_hwtimer_device *timer, rt_uint32_t cmd, void *args)
{
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(args != RT_NULL);

    TIM_HandleTypeDef *tim = RT_NULL;
    rt_err_t result = RT_EOK;
    rt_uint32_t pclk1_doubler, pclk2_doubler;
    /* ��ȡCH32��ʱ����� */
    tim = (TIM_HandleTypeDef *)timer->parent.user_data;

    switch (cmd)
    {
    case HWTIMER_CTRL_FREQ_SET:
    {
        rt_uint32_t freq;
        rt_uint16_t val;
        RCC_ClocksTypeDef RCC_ClockStruct;

        /* set timer frequence */
        freq = *((rt_uint32_t *)args);

        ch32_get_pclk_doubler(&pclk1_doubler, &pclk2_doubler);
        RCC_GetClocksFreq(&RCC_ClockStruct);

        if(tim->instance == TIM1 || tim->instance == TIM8 ||
                tim->instance == TIM9 || tim->instance == TIM10)
        {
            val = RCC_ClockStruct.PCLK2_Frequency * pclk2_doubler / freq;
        }
        else
        {
            val = RCC_ClockStruct.PCLK1_Frequency * pclk1_doubler / freq;
        }

        /* Update frequency value */
        TIM_PrescalerConfig(tim->instance, val - 1, TIM_PSCReloadMode_Immediate);

        result = RT_EOK;
        break;
    }

    case HWTIMER_CTRL_MODE_SET:
    {
        if (*(rt_hwtimer_mode_t *)args == HWTIMER_MODE_ONESHOT)
        {
            /* set timer to single mode */
            tim->instance->CTLR1 &= (uint16_t) ~((uint16_t)TIM_OPM);
            tim->instance->CTLR1 |= TIM_OPMode_Single;
        }
        else
        {
            tim->instance->CTLR1 &= (uint16_t) ~((uint16_t)TIM_OPM);
            tim->instance->CTLR1 |= TIM_OPMode_Repetitive;
        }
        break;
    }

    case HWTIMER_CTRL_INFO_GET:
    {
        *(rt_hwtimer_mode_t *)args = tim->instance->CNT;
        break;
    }

    case HWTIMER_CTRL_STOP:
    {
        ch32_hwtimer_stop(timer);
        break;
    }

    default:
    {
        result = -RT_EINVAL;
        break;
    }
    }

    return result;
}

/* CH32��ʱ������ */
static const struct rt_hwtimer_info ch32_hwtimer_info = TIM_DEV_INFO_CONFIG;
static const struct rt_hwtimer_ops ch32_hwtimer_ops =
{
    ch32_hwtimer_init,
    ch32_hwtimer_start,
    ch32_hwtimer_stop,
    ch32_hwtimer_count_get,
    ch32_hwtimer_control
};

/* CH32��ʱ���ж�ע�ắ�� */
static void ch32_hwtimer_isr(struct rt_hwtimer_device *device)
{
    RT_ASSERT(device != RT_NULL);
    struct ch32_hwtimer *hwtimer = RT_NULL;
    /* ��ѯ�豸����ַ */
    hwtimer = rt_container_of(device, struct ch32_hwtimer, device);
    /* ��ȡ��ʱ������״̬ */
    if(TIM_GetITStatus(hwtimer->handle.instance, TIM_IT_Update) != RESET)
    {
        /* ���ö�ʱ���жϺ��� */
        rt_device_hwtimer_isr(device);
        TIM_ClearITPendingBit(hwtimer->handle.instance, TIM_IT_Update);
    }
}

#ifdef BSP_USING_TIM1
void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM1_UP_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    ch32_hwtimer_isr(&(ch32_hwtimer_obj[TIM1_INDEX].device));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif /* BSP_USING_TIM1 */

#ifdef BSP_USING_TIM2
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    ch32_hwtimer_isr(&(ch32_hwtimer_obj[TIM2_INDEX].device));
    rt_interrupt_leave();
    FREE_INT_SP();
}
#endif /* BSP_USING_TIM2 */


/* ��ʱ����ʼ�� */
static int rt_hw_timer_init(void)
{
    int i = 0;
    int result = RT_EOK;
    /* ѭ��ɨ�趨ʱ������ */
    for (i = 0; i < sizeof(ch32_hwtimer_obj) / sizeof(ch32_hwtimer_obj[0]); i++)
    {
        /* �豸��Ϣ */
        ch32_hwtimer_obj[i].device.info = &ch32_hwtimer_info;
        /* �豸���� */
        ch32_hwtimer_obj[i].device.ops  = &ch32_hwtimer_ops;
        /* ע���豸
         *(1) ch32��ʱ���豸
          (2) �豸����
          (3) CH32��ʱ������
          */
        result = rt_device_hwtimer_register(&ch32_hwtimer_obj[i].device,
                    ch32_hwtimer_obj[i].name, (void *)&ch32_hwtimer_obj[i].handle);
        RT_ASSERT(result == RT_EOK);
    }

    return result;
}
INIT_BOARD_EXPORT(rt_hw_timer_init);

#endif /* RT_USING_HWTIMER */
#endif /* BSP_USING_HWTIMER */
