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

/* ��ʱʱ�� timeout
 *   ϵͳƵ�� freq
 *   �������ļ���ֵ counter
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
    /* ��ʱ�������Ķ�ʱ���� */
    overflow = timer->info->maxcnt/(float)timer->freq;
    /* ������ʱ�Ķ�ʱ��ʱ��   */
    tv_sec = tv->sec + tv->usec/(float)1000000;
    /* ������ʱ��ʱ��С��1��ʱ������ */
    if (tv_sec < (1/(float)timer->freq))
    {
        /* ����ʱʱ������Ϊ��ʱ������  */
        i = 0;
        timeout = 1/(float)timer->freq;
    }
    else
    {
        /* ѭ����ѯ���ʵķ�Ƶϵ�� */
        for (i = 1; i > 0; i ++)
        {
            /* �ڵ�ǰʱ�������µĳ�ʱʱ�� iΪ��Ƶϵ�� */
            timeout = tv_sec/i;
            /* ��ʱʱ�� С�����Ķ�ʱʱ�� ������λ����S */
            if (timeout <= overflow)
            {
                /* ��ǰ�ļ�������Ҫ������ֵ */
                counter = (rt_uint32_t)(timeout * timer->freq);
                /* �����Ķ�ʱʱ�� �� �ڵ�ǰ��Ƶֵ�µ����ڵĶ�ʱ��ʱ������ */
                devi = tv_sec - (counter / (float)timer->freq) * i;
                /* ��ǰ��ʱ��ʱ������ ���� ��С��������� 1s */
                if (devi > devi_min)
                {
                    /* i ����Ϊindex ��ֵΪ��һʱ�̵ķ�Ƶֵ */
                    i = index;
                    /* ��ʱ�������ʱ�� ��Ϊ��һʱ�̵ķ�Ƶֵ�µ����� */
                    timeout = tv_sec/i;
                    /* ��ʱʱ�� */
                    break;
                }/* û����� */
                else if (devi == 0)
                {/* ֱ���˳� */
                    break;
                }/* ���С����С���������ʱ�� */
                else if (devi < devi_min)
                {/* ���������ʱ������Ϊ ��ǰ����� ���ĵ�������ȵ�ǰֵ�� */
                    devi_min = devi;
                    /* ���÷�Ƶֵ */
                    index = i;
                }
            }
        }
    }
    /* ���ö�ʱ����� ѭ������ */
    timer->cycles = i;
    /* ��ʱ����װ�� ���� */
    timer->reload = i;
    /* ��ǰʱ�������� �ĳ�ʱʱ�� */
    timer->period_sec = timeout;
    /* ���ü����������ֵ �ڵ�ǰ�ķ�Ƶֵ�� ������ʱʱ���µĶ�ʱ������װ��ֵ ��ʱ���Ķ�ʱʱ��������С */
    counter = (rt_uint32_t)(timeout * timer->freq);

    return counter;
}

/* Ӳ����ʱ����ʼ�� */
static rt_err_t rt_hwtimer_init(struct rt_device *dev)
{
    rt_err_t result = RT_EOK;
    rt_hwtimer_t *timer;

    timer = (rt_hwtimer_t *)dev;
    /* try to change to 1MHz */
    /* ��ʼ����ʱ��Ƶ�� */
    if ((1000000 <= timer->info->maxfreq) && (1000000 >= timer->info->minfreq))
    {
        /* �����������Χ */
        timer->freq = 1000000;
    }
    else
    {   /* ��ʼ��Ϊ�������С��Ƶ�� */
        timer->freq = timer->info->minfreq;
    }
    /* ��ʱ��ģʽ ����ģʽ */
    timer->mode = HWTIMER_MODE_ONESHOT;
    /* ��ʼ�������жϵ�ѭ������Ϊ 0 */
    timer->cycles = 0;
    /* ��ʱ������Ĵ��� */
    timer->overflow = 0;

    /* ��ʼ����ʱ�� */
    if (timer->ops->init)
    {   /* */
        timer->ops->init(timer, 1);
    }
    else
    {
        result = -RT_ENOSYS;
    }

    return result;
}
/* �򿪶�ʱ�� */
static rt_err_t rt_hwtimer_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_err_t result = RT_EOK;
    rt_hwtimer_t *timer;

    /* ��ʱ���豸 */
    timer = (rt_hwtimer_t *)dev;
    /* ��ʼ����ʱ�� */
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

/* �ر�Ӳ����ʱ�� */
static rt_err_t rt_hwtimer_close(struct rt_device *dev)
{
    rt_err_t result = RT_EOK;
    rt_hwtimer_t *timer;

    timer = (rt_hwtimer_t*)dev;
    if (timer->ops->init != RT_NULL)
    {   /* �رն�ʱ�� */
        timer->ops->init(timer, 0);
    }
    else
    {
        result = -RT_ENOSYS;
    }
    /* ��ʱ��״̬����λ������ */
    dev->flag &= ~RT_DEVICE_FLAG_ACTIVATED;
    dev->rx_indicate = RT_NULL;

    return result;
}

/* ��ʱ���� */
static rt_size_t rt_hwtimer_read(struct rt_device *dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_hwtimer_t *timer;
    rt_hwtimerval_t tv;
    rt_uint32_t cnt;
    rt_base_t level;
    rt_int32_t overflow;
    float t;
    /* ǿ��ת�� */
    timer = (rt_hwtimer_t *)dev;
    if (timer->ops->count_get == RT_NULL)
        return 0;
    /* �ر�ȫ���ж� */
    level = rt_hw_interrupt_disable();
    /* ��ȡ������CNT����ֵ */
    cnt = timer->ops->count_get(timer);
    /* ��ʱ����� */
    overflow = timer->overflow;
    /* ��ȫ���ж� */
    rt_hw_interrupt_enable(level);
    /* ����ʱ��Ϊ���¼���ģʽ  */
    if (timer->info->cntmode == HWTIMER_CNTMODE_DW)
    {
        /* �Ѿ���ɼ����ļ���ֵ = ��װ�ؼ���ֵ - ��������ǰ�ļ���ֵ */
        cnt = (rt_uint32_t)(timer->freq * timer->period_sec) - cnt;
    }
    /* ��ʱ���� * ��ʱʱ�� + �Ѿ���ɼ����ļ���ֵ */
    t = overflow * timer->period_sec + cnt/(float)timer->freq;
    /* �� */
    tv.sec = (rt_int32_t)t;
    /* ΢�� */
    tv.usec = (rt_int32_t)((t - tv.sec) * 1000000);
    /* �ж����ݳ��� */
    size = size > sizeof(tv)? sizeof(tv) : size;
    /* �������ʱ��ֵ������buffer */
    rt_memcpy(buffer, &tv, size);

    return size;
}

/* ��ʱ��д */
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
    /* ֹͣ��ʱ��  */
    timer->ops->stop(timer);
    /* ��ȫ���ж� */
    level = rt_hw_interrupt_disable();
    /* ����������� */
    timer->overflow = 0;
    /* ��ȫ���ж� */
    rt_hw_interrupt_enable(level);
    /* ��ȡ��ѵ���װ�ؼ���ֵ */
    t = timeout_calc(timer, (rt_hwtimerval_t*)buffer);
    /* */
    if ((timer->cycles <= 1) && (timer->mode == HWTIMER_MODE_ONESHOT))
    {
        opm = HWTIMER_MODE_ONESHOT;
    }
    /* ������ʱ�� */
    if (timer->ops->start(timer, t, opm) != RT_EOK)
        size = 0;

    return size;
}

/* ��ʱ������ */
static rt_err_t rt_hwtimer_control(struct rt_device *dev, int cmd, void *args)
{
    rt_base_t level;
    rt_err_t result = RT_EOK;
    rt_hwtimer_t *timer;

    timer = (rt_hwtimer_t *)dev;
    /* �ж����� */
    switch (cmd)
    {/* ����Ӳ����ʱ��ֹͣ */
    case HWTIMER_CTRL_STOP:
    {
        /* ֹͣ��ʱ�� */
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
        /* */
        if (args == RT_NULL)
        {
            result = -RT_EEMPTY;
            break;
        }
        /* ��ȡƵ��ֵ */
        f = (rt_int32_t*)args;
        /* �ж϶�ʱ��Ƶ���Ƿ�Ϸ� */
        if ((*f > timer->info->maxfreq) || (*f < timer->info->minfreq))
        {
            LOG_W("frequency setting out of range! It will maintain at %d Hz", timer->freq);
            result = -RT_EINVAL;
            break;
        }
        /* �жϿ��ƺ����Ƿ�ע�� */
        if (timer->ops->control != RT_NULL)
        {
            /* ִ��ע�ắ�� */
            result = timer->ops->control(timer, cmd, args);
            if (result == RT_EOK)
            {
                /* ��ȫ���ж� */
                level = rt_hw_interrupt_disable();
                /* ���ö�ʱ��Ƶ�� */
                timer->freq = *f;
                /* ��ȫ���ж� */
                rt_hw_interrupt_enable(level);
            }
        }
        else
        {
            result = -RT_ENOSYS;
        }
    }
    break;/* ��ȡ��ʱ����Ϣ */
    case HWTIMER_CTRL_INFO_GET:
    {
        if (args == RT_NULL)
        {
            result = -RT_EEMPTY;
            break;
        }

        *((struct rt_hwtimer_info*)args) = *timer->info;
    }
    break;/* ���ö�ʱ��ģʽ */
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
/* ��ʱ���ж�ע�� */
void rt_device_hwtimer_isr(rt_hwtimer_t *timer)
{
    rt_base_t level;

    RT_ASSERT(timer != RT_NULL);
    /* ��ȫ���ж� */
    level = rt_hw_interrupt_disable();

    timer->overflow ++;
    /* ��Ҫѭ���Ĵ����ǿ� */
    if (timer->cycles != 0)
    {
        /* ��һ���ж� ��ȥһ����Ҫѭ���Ĵ��� */
        timer->cycles --;
    }
    /* ��Ҫѭ���Ĵ���Ϊ�� ����ִ���жϴ��� */
    if (timer->cycles == 0)
    {
        /* ����������ѭ������ */
        timer->cycles = timer->reload;
        /* ʹ��ȫ���ж� */
        rt_hw_interrupt_enable(level);
        /* ����ģʽ */
        if (timer->mode == HWTIMER_MODE_ONESHOT)
        {
            /* ֹͣ��ʱ�� */
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
    {/* ʹ��ȫ���ж� */
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
    /* ��ȡ�豸�İְ� */
    device = &(timer->parent);
    /* ��ʼ���豸���� */
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
    device->user_data   = user_data;/* ��ʼ��ΪCH32��ʱ�����  */
    /* ���豸ע�ᵽ�豸���� */
    return rt_device_register(device, name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);
}
