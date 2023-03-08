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
/* CH32使能的设备链表 */
enum
{
#ifdef BSP_USING_TIM1
    TIM1_INDEX,
#endif
#ifdef BSP_USING_TIM2
    TIM2_INDEX,
#endif

};
/* 定时器设备初始化 */
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

/* CH32总线倍频系数 */
void ch32_get_pclk_doubler(rt_uint32_t *pclk1_doubler, rt_uint32_t *pclk2_doubler)
{
    RT_ASSERT(pclk1_doubler != RT_NULL);
    RT_ASSERT(pclk2_doubler != RT_NULL);

    *pclk1_doubler = 1;
    *pclk2_doubler = 1;
    /* 判断定时器所在的时钟总线的分频系数 */
    if((RCC->CFGR0 & RCC_PPRE1) == RCC_PPRE1_DIV1)
    {   /* 不分频 */
        *pclk1_doubler = 1;
    }
    else
    {   /* 其他分频系数 */
        *pclk1_doubler = 2;
    }

    if((RCC->CFGR0 & RCC_PPRE2) == RCC_PPRE2_DIV1)
    {
        /* 不分频 */
        *pclk2_doubler = 1;
    }
    else
    {
        *pclk2_doubler = 2;
    }
}
/* 定时器初始化 */
static void ch32_hwtimer_init(struct rt_hwtimer_device *timer, rt_uint32_t state)
{
    RT_ASSERT(timer != RT_NULL);
    TIM_HandleTypeDef *tim = RT_NULL;
    RCC_ClocksTypeDef RCC_ClockStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    /* 临时定时器设备 */
    struct ch32_hwtimer *tim_device = RT_NULL;
    /* 分频系数 */
    rt_uint32_t prescaler_value = 0;
    /* 倍频系数 */
    rt_uint32_t pclk1_doubler, pclk2_doubler;
    /* 获取CH32时钟频率 */
    RCC_GetClocksFreq(&RCC_ClockStruct);
    /* 获取倍频系数 */
    ch32_get_pclk_doubler(&pclk1_doubler, &pclk2_doubler);
    /* */
    if(state)
    {
        /* 获取CH32定时器句柄  */
        tim = (TIM_HandleTypeDef *)timer->parent.user_data;
        /* 强制类型转换 */
        tim_device = (struct ch32_hwtimer *)timer;
        /* 判断定时器基地址  */
        if(tim->instance == TIM1 || tim->instance == TIM8 ||
                tim->instance == TIM9 || tim->instance == TIM10)
        {
            /* 使能定时器时钟 */
            RCC_APB2PeriphClockCmd(tim->rcc, ENABLE);
            /* 设置分频系数 */
            prescaler_value = (RCC_ClockStruct.PCLK2_Frequency * pclk2_doubler / 10000) - 1;
        }
        else
        {
            /* */
            RCC_APB1PeriphClockCmd(tim->rcc, ENABLE);
            prescaler_value = (RCC_ClockStruct.PCLK1_Frequency * pclk1_doubler / 10000) - 1;
        }
        /* 初始化分频系数 */
        tim->init.TIM_Prescaler = prescaler_value;
        /* 设置时钟分割 与输入捕获和输出滤波相关 */
        tim->init.TIM_ClockDivision = TIM_CKD_DIV1;
        /* 时钟周期 */
        tim->init.TIM_Period = 10000 - 1;
        /* 延时周期 最终的延时周期为 T * TIM_RepetitionCounter  */
        tim->init.TIM_RepetitionCounter = 0;
        /* 计数模式 */
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
        /* 使用前面配置的参数 初始化定时器 */
        TIM_TimeBaseInit(tim->instance, &tim->init);

        /* 中断相关配置 */
        NVIC_InitStruct.NVIC_IRQChannel = tim_device->irqn;/* 中断源 */
        NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;/* 中断优先级 */
        NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;/* 中断优先级 */
        NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;/* 中断使能  */
        NVIC_Init(&NVIC_InitStruct);

        /* 清除中断标志位   */
        TIM_ClearITPendingBit(tim->instance, TIM_IT_Update);
        /* 中断更新使能  */
        TIM_ITConfig(tim->instance, TIM_IT_Update, ENABLE);
    }
}

/* 硬件定时器启动 */
static rt_err_t ch32_hwtimer_start(struct rt_hwtimer_device *timer, rt_uint32_t cnt, rt_hwtimer_mode_t mode)
{
    RT_ASSERT(timer != RT_NULL);
    TIM_HandleTypeDef *tim = RT_NULL;
    /* 获取CH32定时器设备句柄 */
    tim = (TIM_HandleTypeDef *)timer->parent.user_data;

    /* 初始化定时器 计数值 */
    tim->instance->CNT = 0;
    /* 设置定时器自动重装载值 */
    tim->instance->ATRLR = cnt - 1;
    /* 初始的定时器周期 */
    tim->init.TIM_Period = cnt - 1;
    /* 判断定时器模式 */
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

    /* 启动定时器  */
    TIM_Cmd(tim->instance, ENABLE);

    return RT_EOK;
}
/* 停止定时器 */
static void ch32_hwtimer_stop(struct rt_hwtimer_device *timer)
{
    RT_ASSERT(timer != RT_NULL);
    /**/
    TIM_HandleTypeDef *tim = RT_NULL;
    /* 获取CH32定时器句柄 */
    tim = (TIM_HandleTypeDef *)timer->parent.user_data;

    /* 停止定时器  */
    TIM_Cmd(tim->instance, DISABLE);

    /* 定时器计数值置零  */
    tim->instance->CNT = 0;
}

/* 获取定时器计数值 */
static rt_uint32_t ch32_hwtimer_count_get(struct rt_hwtimer_device *timer)
{
    RT_ASSERT(timer != RT_NULL);
    TIM_HandleTypeDef *tim = RT_NULL;
    tim = (TIM_HandleTypeDef *)timer->parent.user_data;

    return tim->instance->CNT;
}
/* 定时器控制 */
static rt_err_t ch32_hwtimer_control(struct rt_hwtimer_device *timer, rt_uint32_t cmd, void *args)
{
    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(args != RT_NULL);

    TIM_HandleTypeDef *tim = RT_NULL;
    rt_err_t result = RT_EOK;
    rt_uint32_t pclk1_doubler, pclk2_doubler;
    /* 获取CH32定时器句柄 */
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

/* CH32定时器方法 */
static const struct rt_hwtimer_info ch32_hwtimer_info = TIM_DEV_INFO_CONFIG;
static const struct rt_hwtimer_ops ch32_hwtimer_ops =
{
    ch32_hwtimer_init,
    ch32_hwtimer_start,
    ch32_hwtimer_stop,
    ch32_hwtimer_count_get,
    ch32_hwtimer_control
};

/* CH32定时器中断注册函数 */
static void ch32_hwtimer_isr(struct rt_hwtimer_device *device)
{
    RT_ASSERT(device != RT_NULL);
    struct ch32_hwtimer *hwtimer = RT_NULL;
    /* 查询设备基地址 */
    hwtimer = rt_container_of(device, struct ch32_hwtimer, device);
    /* 获取定时器计数状态 */
    if(TIM_GetITStatus(hwtimer->handle.instance, TIM_IT_Update) != RESET)
    {
        /* 调用定时器中断函数 */
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


/* 定时器初始化 */
static int rt_hw_timer_init(void)
{
    int i = 0;
    int result = RT_EOK;
    /* 循环扫描定时器链表 */
    for (i = 0; i < sizeof(ch32_hwtimer_obj) / sizeof(ch32_hwtimer_obj[0]); i++)
    {
        /* 设备信息 */
        ch32_hwtimer_obj[i].device.info = &ch32_hwtimer_info;
        /* 设备方法 */
        ch32_hwtimer_obj[i].device.ops  = &ch32_hwtimer_ops;
        /* 注册设备
         *(1) ch32定时器设备
          (2) 设备名称
          (3) CH32定时器属性
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
