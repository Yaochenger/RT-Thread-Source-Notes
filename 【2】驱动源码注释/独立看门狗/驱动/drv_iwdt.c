/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2021-08-10     charlown          first version
 * 2022-09-27     hg0720            the first version which add from wch
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "ch32v30x_iwdg.h"

#ifdef BSP_USING_IWDT

#define LOG_TAG "drv.wdt"
#include "drv_log.h"

#ifndef LSI_VALUE
#error "Please define the value of LSI_VALUE!"
#endif
/* CH32看门狗设备 */
struct watchdog_device
{
    /* RT-Thread 看门狗设备驱动 */
    rt_watchdog_t parent;
    /* CH32看门狗设备寄存器句柄 */
    IWDG_TypeDef *instance;
    /* 看门狗定时器时钟源分频值 */
    rt_uint32_t prescaler;
    /* 重装载值 */
    rt_uint32_t reload;
    /* 启动状态 */
    rt_uint16_t is_start;
};
static struct watchdog_device watchdog_dev;

static rt_err_t ch32_wdt_init(rt_watchdog_t *wdt)
{
    return RT_EOK;
}
/* CH32看门狗控制函数 */
static rt_err_t ch32_wdt_control(rt_watchdog_t *wdt, int cmd, void *arg)
{
    struct watchdog_device *wdt_dev;
    /* 强制类型转换 */
    wdt_dev = (struct watchdog_device *)wdt;

    switch (cmd)
    {
    /* 喂狗 */
    case RT_DEVICE_CTRL_WDT_KEEPALIVE:
        /* 看门狗重装载计数值 */
        IWDG_ReloadCounter();
        break;
        /* 设置看门狗定时器超时时间 */
    case RT_DEVICE_CTRL_WDT_SET_TIMEOUT:
        if (LSI_VALUE)
        {
            /* 设置重装载值 */
            wdt_dev->reload = (*((rt_uint32_t *)arg)) * LSI_VALUE / 256;
        }
        else
        {
            LOG_E("Please define the value of LSI_VALUE!");
        }
        if (wdt_dev->reload > 0xFFF)
        {
            LOG_E("wdg set timeout parameter too large, please less than %ds", 0xFFF * 256 / LSI_VALUE);
            return -RT_EINVAL;
        }
        /* 若看门狗已经启动 */
        if (wdt_dev->is_start)
        {
            /* 看门狗写入使能 */
            IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
            IWDG_SetPrescaler(wdt_dev->prescaler);
            IWDG_SetReload(wdt_dev->reload);
            /* 看门狗写入缺省 */
            IWDG_WriteAccessCmd(IWDG_WriteAccess_Disable);
            /* 看门狗使能 */
            IWDG_Enable();
        }
        break;
        /* 获取看门狗超时时间 */
    case RT_DEVICE_CTRL_WDT_GET_TIMEOUT:
        if (LSI_VALUE)
        {
            (*((rt_uint32_t *)arg)) = wdt_dev->reload * 256 / LSI_VALUE;
        }
        else
        {
            LOG_E("Please define the value of LSI_VALUE!");
        }
        break;
        /* 启动看门狗 */
    case RT_DEVICE_CTRL_WDT_START:
        IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
        IWDG_SetPrescaler(wdt_dev->prescaler);
        IWDG_SetReload(wdt_dev->reload);
        IWDG_WriteAccessCmd(IWDG_WriteAccess_Disable);
        IWDG_Enable();
        wdt_dev->is_start = 1;
        break;
    default:
        LOG_W("This command is not supported.");
        return -RT_ERROR;
    }
    return RT_EOK;
}

static struct rt_watchdog_ops watchdog_ops =
{
    .init = ch32_wdt_init,
    .control = ch32_wdt_control,
};

int rt_hw_wdt_init(void)
{
    watchdog_dev.instance = IWDG;
    watchdog_dev.prescaler = IWDG_Prescaler_256;
    watchdog_dev.reload = 0x0000FFF;
    watchdog_dev.is_start = 0;
    watchdog_dev.parent.ops = &watchdog_ops;
    /* 注册看门狗设备 */
    if (rt_hw_watchdog_register(&watchdog_dev.parent, "wdt", RT_DEVICE_FLAG_DEACTIVATE, RT_NULL) != RT_EOK)
    {
        LOG_E("wdt device register failed.");
        return -RT_ERROR;
    }
    LOG_D("wdt device register success.");
    return RT_EOK;
}
INIT_BOARD_EXPORT(rt_hw_wdt_init);

#endif /* BSP_USING_IWDT */
