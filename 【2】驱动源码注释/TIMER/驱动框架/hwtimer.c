/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2015-08-31     heyuanjie87    first version
 */

#include <rtdevice.h>
#include <rthw.h>

#define DBG_TAG "hwtimer"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* 超时时间 timeout
 *   系统频率 freq
 *   计数器的计数值 counter
 *
 *  timeout = counter / freq
 *  counter = timeout * freq */
rt_inline rt_uint32_t timeout_calc(rt_hwtimer_t *timer, rt_hwtimerval_t *tv)
{

    float overflow;
    float timeout;
    rt_uint32_t counter;
    int i, index = 0;
    float tv_sec;
    float devi_min = 1;
    float devi;

    /* changed to second */
    /* 定时器的最大的定时周期 */
    overflow = timer->info->maxcnt/(float)timer->freq;
    /* 期望定时的定时器时间   */
    tv_sec = tv->sec + tv->usec/(float)1000000;
    /* 期望定时的时间小于1个时钟周期 */
    if (tv_sec < (1/(float)timer->freq))
    {
        /* 将超时时间设置为定时器周期  */
        i = 0;
        timeout = 1/(float)timer->freq;
    }
    else
    {
        /* 循环查询合适的分频系数 */
        for (i = 1; i > 0; i ++)
        {
            /* 在当前时钟周期下的超时时间 i为分频系数 */
            timeout = tv_sec/i;
            /* 超时时间 小于最大的定时时间 这俩单位都是S */
            if (timeout <= overflow)
            {
                /* 当前的计数器需要填充的数值 */
                counter = (rt_uint32_t)(timeout * timer->freq);
                /* 期望的定时时间 与 在当前分频值下的周期的定时器时间的误差 */
                devi = tv_sec - (counter / (float)timer->freq) * i;
                /* 当前定时器时间的误差 大于 最小的容忍误差 1s */
                if (devi > devi_min)
                {
                    /* i 更新为index 该值为上一时刻的分频值 */
                    i = index;
                    /* 超时（溢出）时间 定为上一时刻的分频值下的周期 */
                    timeout = tv_sec/i;
                    /* 超时时间 */
                    break;
                }/* 没有误差 */
                else if (devi == 0)
                {/* 直接退出 */
                    break;
                }/* 误差小于最小的误差允许时间 */
                else if (devi < devi_min)
                {/* 将误差允许时间设置为 当前的误差 下文的误差不允许比当前值高 */
                    devi_min = devi;
                    /* 设置分频值 */
                    index = i;
                }
            }
        }
    }
    /* 设置定时器循环时间 */
    timer->cycles = i;
    /* 定时器重装载周期 */
    timer->reload = i;
    /* 超时时间 */
    timer->period_sec = timeout;
    /* 计算处理来计数器的填充值 在当前的分频值 定时器的定时时间的误差最小 */
    counter = (rt_uint32_t)(timeout * timer->freq);

    return counter;
}

/* 硬件定时器初始化 */
static rt_err_t rt_hwtimer_init(struct rt_device *dev)
{
    rt_err_t result = RT_EOK;
    rt_hwtimer_t *timer;

    timer = (rt_hwtimer_t *)dev;
    /* try to change to 1MHz */
    /* 初始化定时器频率 */
    if ((1000000 <= timer->info->maxfreq) && (1000000 >= timer->info->minfreq))
    {
        timer->freq = 1000000;
    }
    else
    {
        timer->freq = timer->info->minfreq;
    }
    /* 定时器模式 单次模式 */
    timer->mode = HWTIMER_MODE_ONESHOT;
    timer->cycles = 0;
    timer->overflow = 0;

    /* 初始化定时器 */
    if (timer->ops->init)
    {
        timer->ops->init(timer, 1);
    }
    else
    {
        result = -RT_ENOSYS;
    }

    return result;
}
/* 打开定时器 */
static rt_err_t rt_hwtimer_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_err_t result = RT_EOK;
    rt_hwtimer_t *timer;

    /* 定时器设备 */
    timer = (rt_hwtimer_t *)dev;
    /* 初始化定时器 */
    if (timer->ops->control != RT_NULL)
    {   /* */
        timer->ops->control(timer, HWTIMER_CTRL_FREQ_SET, &timer->freq);
    }
    else
    {
        result = -RT_ENOSYS;
    }

    return result;
}

/* 关闭硬件定时器 */
static rt_err_t rt_hwtimer_close(struct rt_device *dev)
{
    rt_err_t result = RT_EOK;
    rt_hwtimer_t *timer;

    timer = (rt_hwtimer_t*)dev;
    if (timer->ops->init != RT_NULL)
    {   /* 关闭定时器 */
        timer->ops->init(timer, 0);
    }
    else
    {
        result = -RT_ENOSYS;
    }
    /* 定时器状态设置位不激活 */
    dev->flag &= ~RT_DEVICE_FLAG_ACTIVATED;
    dev->rx_indicate = RT_NULL;

    return result;
}

/* 定时器读 */
static rt_size_t rt_hwtimer_read(struct rt_device *dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_hwtimer_t *timer;
    rt_hwtimerval_t tv;
    rt_uint32_t cnt;
    rt_base_t level;
    rt_int32_t overflow;
    float t;

    timer = (rt_hwtimer_t *)dev;
    if (timer->ops->count_get == RT_NULL)
        return 0;
    /* 关闭全局中断 */
    level = rt_hw_interrupt_disable();
    /* 获取定时器计数值 */
    cnt = timer->ops->count_get(timer);
    /* 定时器溢出 */
    overflow = timer->overflow;
    /* 开全局中断 */
    rt_hw_interrupt_enable(level);
    /* 定时器计数模式 */
    if (timer->info->cntmode == HWTIMER_CNTMODE_DW)
    {
        cnt = (rt_uint32_t)(timer->freq * timer->period_sec) - cnt;
    }

    t = overflow * timer->period_sec + cnt/(float)timer->freq;
    /* 秒 */
    tv.sec = (rt_int32_t)t;
    /* 微秒 */
    tv.usec = (rt_int32_t)((t - tv.sec) * 1000000);
    size = size > sizeof(tv)? sizeof(tv) : size;
    rt_memcpy(buffer, &tv, size);

    return size;
}

/* */
static rt_size_t rt_hwtimer_write(struct rt_device *dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_base_t level;
    rt_uint32_t t;
    rt_hwtimer_mode_t opm = HWTIMER_MODE_PERIOD;
    rt_hwtimer_t *timer;

    timer = (rt_hwtimer_t *)dev;
    if ((timer->ops->start == RT_NULL) || (timer->ops->stop == RT_NULL))
        return 0;

    if (size != sizeof(rt_hwtimerval_t))
        return 0;
    /* 停止定时器  */
    timer->ops->stop(timer);
    /* 关全局中断 */
    level = rt_hw_interrupt_disable();
    /* 重置溢出状态 */
    timer->overflow = 0;
    /* 开全局中断 */
    rt_hw_interrupt_enable(level);

    t = timeout_calc(timer, (rt_hwtimerval_t*)buffer);
    if ((timer->cycles <= 1) && (timer->mode == HWTIMER_MODE_ONESHOT))
    {
        opm = HWTIMER_MODE_ONESHOT;
    }

    if (timer->ops->start(timer, t, opm) != RT_EOK)
        size = 0;

    return size;
}

static rt_err_t rt_hwtimer_control(struct rt_device *dev, int cmd, void *args)
{
    rt_base_t level;
    rt_err_t result = RT_EOK;
    rt_hwtimer_t *timer;

    timer = (rt_hwtimer_t *)dev;

    switch (cmd)
    {
    case HWTIMER_CTRL_STOP:
    {
        if (timer->ops->stop != RT_NULL)
        {
            timer->ops->stop(timer);
        }
        else
        {
            result = -RT_ENOSYS;
        }
    }
    break;
    case HWTIMER_CTRL_FREQ_SET:
    {
        rt_int32_t *f;

        if (args == RT_NULL)
        {
            result = -RT_EEMPTY;
            break;
        }

        f = (rt_int32_t*)args;
        if ((*f > timer->info->maxfreq) || (*f < timer->info->minfreq))
        {
            LOG_W("frequency setting out of range! It will maintain at %d Hz", timer->freq);
            result = -RT_EINVAL;
            break;
        }

        if (timer->ops->control != RT_NULL)
        {
            result = timer->ops->control(timer, cmd, args);
            if (result == RT_EOK)
            {
                level = rt_hw_interrupt_disable();
                timer->freq = *f;
                rt_hw_interrupt_enable(level);
            }
        }
        else
        {
            result = -RT_ENOSYS;
        }
    }
    break;
    case HWTIMER_CTRL_INFO_GET:
    {
        if (args == RT_NULL)
        {
            result = -RT_EEMPTY;
            break;
        }

        *((struct rt_hwtimer_info*)args) = *timer->info;
    }
    break;
    case HWTIMER_CTRL_MODE_SET:
    {
        rt_hwtimer_mode_t *m;

        if (args == RT_NULL)
        {
            result = -RT_EEMPTY;
            break;
        }

        m = (rt_hwtimer_mode_t*)args;

        if ((*m != HWTIMER_MODE_ONESHOT) && (*m != HWTIMER_MODE_PERIOD))
        {
            result = -RT_ERROR;
            break;
        }
        level = rt_hw_interrupt_disable();
        timer->mode = *m;
        rt_hw_interrupt_enable(level);
    }
    break;
    default:
    {
        result = -RT_ENOSYS;
    }
    break;
    }

    return result;
}

void rt_device_hwtimer_isr(rt_hwtimer_t *timer)
{
    rt_base_t level;

    RT_ASSERT(timer != RT_NULL);

    level = rt_hw_interrupt_disable();

    timer->overflow ++;

    if (timer->cycles != 0)
    {
        timer->cycles --;
    }

    if (timer->cycles == 0)
    {
        timer->cycles = timer->reload;

        rt_hw_interrupt_enable(level);

        if (timer->mode == HWTIMER_MODE_ONESHOT)
        {
            if (timer->ops->stop != RT_NULL)
            {
                timer->ops->stop(timer);
            }
        }

        if (timer->parent.rx_indicate != RT_NULL)
        {
            timer->parent.rx_indicate(&timer->parent, sizeof(struct rt_hwtimerval));
        }
    }
    else
    {
        rt_hw_interrupt_enable(level);
    }
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops hwtimer_ops =
{
    rt_hwtimer_init,
    rt_hwtimer_open,
    rt_hwtimer_close,
    rt_hwtimer_read,
    rt_hwtimer_write,
    rt_hwtimer_control
};
#endif

rt_err_t rt_device_hwtimer_register(rt_hwtimer_t *timer, const char *name, void *user_data)
{

    struct rt_device *device;

    RT_ASSERT(timer != RT_NULL);
    RT_ASSERT(timer->ops != RT_NULL);
    RT_ASSERT(timer->info != RT_NULL);
    /* 获取设备的爸爸 */
    device = &(timer->parent);
    /* 初始化设备类型 */
    device->type        = RT_Device_Class_Timer;
    device->rx_indicate = RT_NULL;
    device->tx_complete = RT_NULL;

#ifdef RT_USING_DEVICE_OPS
    device->ops         = &hwtimer_ops;
#else
    device->init        = rt_hwtimer_init;
    device->open        = rt_hwtimer_open;
    device->close       = rt_hwtimer_close;
    device->read        = rt_hwtimer_read;
    device->write       = rt_hwtimer_write;
    device->control     = rt_hwtimer_control;
#endif
    device->user_data   = user_data;/* 初始化为CH32定时器句柄  */
    /* 将设备注册到设备容器 */
    return rt_device_register(device, name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);
}
