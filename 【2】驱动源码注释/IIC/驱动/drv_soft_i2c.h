/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-19     hg0720       the first version which add from wch
 */

#ifndef __DRV_I2C__
#define __DRV_I2C__

#include <rtthread.h>
#include <rthw.h>
#include <rtdevice.h>

/* ch32 config class */
/* */
/* 软件I2C配置 */
struct ch32_soft_i2c_config
{
    rt_uint8_t scl;/* SCL管脚 */
    rt_uint8_t sda;/* SDA管脚 */
    const char *bus_name;/* 总线名称 */
};

/* ch32 i2c dirver class */
/* CH32 I2C 设备 */
struct ch32_i2c
{
    /* CH32 I2C方法 */
    struct rt_i2c_bit_ops ops;
    /* I2C总线设备 */
    struct rt_i2c_bus_device i2c2_bus;
};

#ifdef BSP_USING_I2C1
#define I2C1_BUS_CONFIG                                  \
    {                                                    \
        .scl = BSP_I2C1_SCL_PIN,                         \
        .sda = BSP_I2C1_SDA_PIN,                         \
        .bus_name = "i2c1",                              \
    }
#endif

#ifdef BSP_USING_I2C2
#define I2C2_BUS_CONFIG                                  \
    {                                                    \
        .scl = BSP_I2C2_SCL_PIN,                         \
        .sda = BSP_I2C2_SDA_PIN,                         \
        .bus_name = "i2c2",                              \
    }
#endif

int rt_hw_i2c_init(void);

#endif
