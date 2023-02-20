/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-19     hg0720       the first version which add from wch
 */

#include <board.h>
#include "drv_soft_i2c.h"

#ifdef BSP_USING_SOFT_I2C

//#define DRV_DEBUG
#define LOG_TAG              "drv.i2c"
#include <drv_log.h>

#if !defined(BSP_USING_I2C1) && !defined(BSP_USING_I2C2)
#error "Please define at least one BSP_USING_I2Cx"
/* this driver can be disabled at menuconfig -> RT-Thread Components -> Device Drivers */
#endif

static const struct ch32_soft_i2c_config soft_i2c_config[] =
{
#ifdef BSP_USING_I2C1
    I2C1_BUS_CONFIG,
#endif
#ifdef BSP_USING_I2C2
    I2C2_BUS_CONFIG,
#endif
};

/* */
/* 设备链表 */
static struct ch32_i2c i2c_obj[sizeof(soft_i2c_config) / sizeof(soft_i2c_config[0])];

/*
 * This function initializes the i2c pin.
 *
 * @param ch32 i2c dirver class.
 */
/* I2C设备管脚初始化 */
static void ch32_i2c_gpio_init(struct ch32_i2c *i2c)
{
    struct ch32_soft_i2c_config* cfg = (struct ch32_soft_i2c_config*)i2c->ops.data;

    rt_pin_mode(cfg->scl, PIN_MODE_OUTPUT_OD);
    rt_pin_mode(cfg->sda, PIN_MODE_OUTPUT_OD);

    rt_pin_write(cfg->scl, PIN_HIGH);
    rt_pin_write(cfg->sda, PIN_HIGH);
}

/*
 * This function sets the sda pin.
 *
 * @param Ch32 config class.
 * @param The sda pin state.
 */
/* 设置数据总线电平 */
static void ch32_set_sda(void *data, rt_int32_t state)
{
    struct ch32_soft_i2c_config* cfg = (struct ch32_soft_i2c_config*)data;
    if (state)
    {
        rt_pin_write(cfg->sda, PIN_HIGH);
    }
    else
    {
        rt_pin_write(cfg->sda, PIN_LOW);
    }
}

/*
 * This function sets the scl pin.
 *
 * @param Ch32 config class.
 * @param The scl pin state.
 */
/* 设置SCL电平 */
static void ch32_set_scl(void *data, rt_int32_t state)
{
    struct ch32_soft_i2c_config* cfg = (struct ch32_soft_i2c_config*)data;
    if (state)
    {
        rt_pin_write(cfg->scl, PIN_HIGH);
    }
    else
    {
        rt_pin_write(cfg->scl, PIN_LOW);
    }
}

/*
 * This function gets the sda pin state.
 *
 * @param The sda pin state.
 */
/* 获取SDA电平 */
static rt_int32_t ch32_get_sda(void *data)
{
    struct ch32_soft_i2c_config* cfg = (struct ch32_soft_i2c_config*)data;
    return rt_pin_read(cfg->sda);
}

/*
 * This function gets the scl pin state.
 *
 * @param The scl pin state.
 */
/* 获取SCL电平 */
static rt_int32_t ch32_get_scl(void *data)
{
    struct ch32_soft_i2c_config* cfg = (struct ch32_soft_i2c_config*)data;
    return rt_pin_read(cfg->scl);
}

/*
 * The time delay function.
 *
 * @param microseconds.
 */
/* CH32 us级别延时 */ /* */
static void ch32_udelay(rt_uint32_t us)
{
    rt_uint32_t ticks;
    rt_uint32_t told, tnow, tcnt = 0;
    rt_uint32_t reload = SysTick->CMP;/* 重装载值 */

    /* 运行 num1 us 系统节拍定时器会计算的个数 */
    ticks = us * reload / (1000000 / RT_TICK_PER_SECOND);
    /* old tick 系统定时器 _计数值 */
    told = SysTick->CNT;
    /* */
    while (1)
    {
        /* now tick 定时器当前计数值 */
        tnow = SysTick->CNT;
        /* 若 now计数值  ！=  old计数值 */
        if (tnow != told)
        {
            /* 若 now计数值 > old计数值 */
            if (tnow > told)
            {
                /* 计数差 */
                tcnt += tnow - told;
            }
            else/* 若 now计数值 < = old计数值 */
            {
                /* 计数差  */
                tcnt += reload + tnow - told;
            }
            /* 更新told = tnow */
            told = tnow;
            /* 时间差 */
            if (tcnt >= ticks)
            {
                break;
            }
        }
    }
}

/* I2C设备框架方法 初始化 */
static const struct rt_i2c_bit_ops ch32_bit_ops_default =
{
    .data     = RT_NULL,
    .set_sda  = ch32_set_sda,
    .set_scl  = ch32_set_scl,
    .get_sda  = ch32_get_sda,
    .get_scl  = ch32_get_scl,
    .udelay   = ch32_udelay,
    .delay_us = 1,
    .timeout  = 100
};

/*
 * if i2c is locked, this function will unlock it
 *
 * @param ch32 config class
 *
 * @return RT_EOK indicates successful unlock.
 */
/* */
static rt_err_t ch32_i2c_bus_unlock(const struct ch32_soft_i2c_config *cfg)
{
    rt_int32_t i = 0;
    /* 若 SDA = 0*/
    if (PIN_LOW == rt_pin_read(cfg->sda))
    {
        /* 输出9个时钟 解锁IIC死锁  */
        while (i++ < 9)
        {
            rt_pin_write(cfg->scl, PIN_HIGH);
            ch32_udelay(100);
            rt_pin_write(cfg->scl, PIN_LOW);
            ch32_udelay(100);
        }
    }/* 若 SDA = 0*/
    if (PIN_LOW == rt_pin_read(cfg->sda))
    {
        /* 解锁失败 */
        return -RT_ERROR;
    }
    /* 解锁成功 */
    return RT_EOK;
}

/* I2C initialization function */
/* I2C 初始化 */
int rt_hw_i2c_init(void)
{
    /* I2C设备数量 */
    rt_size_t obj_num = sizeof(i2c_obj) / sizeof(struct ch32_i2c);
    rt_err_t result;
    /* 循环初始化 */
    for (int i = 0; i < obj_num; i++)
    {
        /* 注册方法 */
        i2c_obj[i].ops = ch32_bit_ops_default;
        /* 设备硬件数据 */
        i2c_obj[i].ops.data = (void*)&soft_i2c_config[i];
        /* 保存设备方法 */
        i2c_obj[i].i2c2_bus.priv = &i2c_obj[i].ops;
        /* 初始化管脚 */
        ch32_i2c_gpio_init(&i2c_obj[i]);
        /* 将设备注册到设备容器 */
        result = rt_i2c_bit_add_bus(&i2c_obj[i].i2c2_bus, soft_i2c_config[i].bus_name);
        RT_ASSERT(result == RT_EOK);
        /* */
        ch32_i2c_bus_unlock(&soft_i2c_config[i]);
        LOG_D("software simulation %s init done, pin scl: %d, pin sda %d",
        soft_i2c_config[i].bus_name,
        soft_i2c_config[i].scl,
        soft_i2c_config[i].sda);
    }

    return RT_EOK;
}
INIT_BOARD_EXPORT(rt_hw_i2c_init);

#endif /* RT_USING_I2C */
