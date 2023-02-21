/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2012-04-25     weety         first version
 * 2021-04-20     RiceChen      added support for bus control api
 */

#ifndef __I2C_H__
#define __I2C_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RT_I2C_WR                0x0000
#define RT_I2C_RD               (1u << 0)
#define RT_I2C_ADDR_10BIT       (1u << 2)  /* this is a ten bit chip address */
#define RT_I2C_NO_START         (1u << 4)
#define RT_I2C_IGNORE_NACK      (1u << 5)
#define RT_I2C_NO_READ_ACK      (1u << 6)  /* when I2C reading, we do not ACK */
#define RT_I2C_NO_STOP          (1u << 7)

/* I2C信息 */
struct rt_i2c_msg
{
    rt_uint16_t addr; /* 地址 */
    rt_uint16_t flags;/* 标志 */
    rt_uint16_t len;  /* 长度 */
    rt_uint8_t  *buf; /* buffer */
};

struct rt_i2c_bus_device;
/* */
/* I2C设备方法 */
struct rt_i2c_bus_device_ops
{
    /* 主设备发送方法 */
    rt_size_t (*master_xfer)(struct rt_i2c_bus_device *bus,
                             struct rt_i2c_msg msgs[],
                             rt_uint32_t num);
    /* 从设备发送方法 */
    rt_size_t (*slave_xfer)(struct rt_i2c_bus_device *bus,
                            struct rt_i2c_msg msgs[],
                            rt_uint32_t num);
    /* 设备总线控制 */
    rt_err_t (*i2c_bus_control)(struct rt_i2c_bus_device *bus,
                                rt_uint32_t,
                                rt_uint32_t);
};

/* I2C 总线设备 */
struct rt_i2c_bus_device
{
    struct rt_device parent;                  /* I2C设备的爸爸 */
    const struct rt_i2c_bus_device_ops *ops;  /* I2C设备方法 */
    rt_uint16_t  flags;                       /* 标志 */
    struct rt_mutex lock;                     /* 互斥锁 */
    rt_uint32_t  timeout;                     /* 等待时间 */
    rt_uint32_t  retries;                     /* I2C设备获取次数 */
    void *priv;                               /* 设备私有数据指针 */
};

/* */
struct rt_i2c_client
{
    struct rt_i2c_bus_device       *bus;
    rt_uint16_t                    client_addr;
};

/* IIC总线设备注册函数 */
rt_err_t rt_i2c_bus_device_register(struct rt_i2c_bus_device *bus,
                                    const char               *bus_name);
/* 总线设备查询  */
struct rt_i2c_bus_device *rt_i2c_bus_device_find(const char *bus_name);
/* IIC总线设备发送 */
rt_size_t rt_i2c_transfer(struct rt_i2c_bus_device *bus,
                          struct rt_i2c_msg         msgs[],
                          rt_uint32_t               num);
/* IIC总线设备 控制 */
rt_err_t rt_i2c_control(struct rt_i2c_bus_device *bus,
                        rt_uint32_t               cmd,
                        rt_uint32_t               arg);
/* 主机发送 */
rt_size_t rt_i2c_master_send(struct rt_i2c_bus_device *bus,
                             rt_uint16_t               addr,
                             rt_uint16_t               flags,
                             const rt_uint8_t         *buf,
                             rt_uint32_t               count);
/* 主机接收  */
rt_size_t rt_i2c_master_recv(struct rt_i2c_bus_device *bus,
                             rt_uint16_t               addr,
                             rt_uint16_t               flags,
                             rt_uint8_t               *buf,
                             rt_uint32_t               count);
/* 总线上锁 */
rt_inline rt_err_t rt_i2c_bus_lock(struct rt_i2c_bus_device *bus, rt_tick_t timeout)
{
    return rt_mutex_take(&bus->lock, timeout);
}
/* 总线解锁 */
rt_inline rt_err_t rt_i2c_bus_unlock(struct rt_i2c_bus_device *bus)
{
    return rt_mutex_release(&bus->lock);
}

int rt_i2c_core_init(void);

#ifdef __cplusplus
}
#endif

#endif
