/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-05-07     aozima       the first version
 * 2022-09-24     yuqi         add phase and dead time configuration
 */

#ifndef __DRV_PWM_H_INCLUDE__
#define __DRV_PWM_H_INCLUDE__

#include <rtthread.h>

#define PWM_CMD_ENABLE      (RT_DEVICE_CTRL_BASE(PWM) + 0)  /* PWM设备使能  */ /*  */
#define PWM_CMD_DISABLE     (RT_DEVICE_CTRL_BASE(PWM) + 1)  /* PWM设备取消  */
#define PWM_CMD_SET         (RT_DEVICE_CTRL_BASE(PWM) + 2)  /* PWM设备设置参数   */
#define PWM_CMD_GET         (RT_DEVICE_CTRL_BASE(PWM) + 3)  /* PWM设备获取参数   */
#define PWMN_CMD_ENABLE     (RT_DEVICE_CTRL_BASE(PWM) + 4)  /* PWM设备互补通道使能  */
#define PWMN_CMD_DISABLE    (RT_DEVICE_CTRL_BASE(PWM) + 5)  /* PWM设备互补通道取消   */
#define PWM_CMD_SET_PERIOD  (RT_DEVICE_CTRL_BASE(PWM) + 6)  /* PWM设备周期设置 单位ns  */
#define PWM_CMD_SET_PULSE   (RT_DEVICE_CTRL_BASE(PWM) + 7)  /* PWM设备脉冲宽度设置 单位 ns */
#define PWM_CMD_SET_DEAD_TIME  (RT_DEVICE_CTRL_BASE(PWM) + 8) /* PWM设备死区时间设置   */
#define PWM_CMD_SET_PHASE   (RT_DEVICE_CTRL_BASE(PWM) + 9)    /* PWM设备相位设置  */
#define PWM_CMD_ENABLE_IRQ  (RT_DEVICE_CTRL_BASE(PWM) + 10)   /* PWM设备中断使能  */
#define PWM_CMD_DISABLE_IRQ  (RT_DEVICE_CTRL_BASE(PWM) + 11)  /* PWM设备中断失能 */

struct rt_pwm_configuration
{
    /* PWM设备输出通道   */
    rt_uint32_t channel; /* 0 ~ n or 0 ~ -n, which depends on specific MCU requirements */
    /* PWM设备周期 单位ns */
    rt_uint32_t period;  /* unit:ns 1ns~4.29s:1Ghz~0.23hz */
    /* PWM设备脉冲宽度 单位ns   */
    rt_uint32_t pulse;   /* unit:ns (pulse<=period) */
    /* PWM设备死区时间 单位ns   */
    rt_uint32_t dead_time;  /* unit:ns */
    /* PWM设备脉相位 单位ns   */
    rt_uint32_t phase;  /*unit: degree, 0~360, which is the phase of pwm output, */
    /*
     * RT_TRUE  : The channel of pwm is complememtary.
     * RT_FALSE : The channel of pwm is nomal.
    */
    rt_bool_t  complementary;
};

struct rt_device_pwm;
/* PWM设备方法   */
struct rt_pwm_ops
{
    rt_err_t (*control)(struct rt_device_pwm *device, int cmd, void *arg);
};

/* PWM设备   */
struct rt_device_pwm
{
    /* PWM设备的爸爸 rt_device */
    struct rt_device parent;
    const struct rt_pwm_ops *ops;
};

rt_err_t rt_device_pwm_register(struct rt_device_pwm *device, const char *name, const struct rt_pwm_ops *ops, const void *user_data);

rt_err_t rt_pwm_enable(struct rt_device_pwm *device, int channel);
rt_err_t rt_pwm_disable(struct rt_device_pwm *device, int channel);
rt_err_t rt_pwm_set(struct rt_device_pwm *device, int channel, rt_uint32_t period, rt_uint32_t pulse);
rt_err_t rt_pwm_set_period(struct rt_device_pwm *device, int channel, rt_uint32_t period);
rt_err_t rt_pwm_set_pulse(struct rt_device_pwm *device, int channel, rt_uint32_t pulse);
rt_err_t rt_pwm_set_dead_time(struct rt_device_pwm *device, int channel, rt_uint32_t dead_time);
rt_err_t rt_pwm_set_phase(struct rt_device_pwm *device, int channel, rt_uint32_t phase);

#endif /* __DRV_PWM_H_INCLUDE__ */
