/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-01-08     bernard      first version.
 * 2012-02-03     bernard      add const attribute to the ops.
 * 2012-05-15     dzzxzz       fixed the return value in attach_device.
 * 2012-05-18     bernard      Changed SPI message to message list.
 *                             Added take/release SPI device/bus interface.
 * 2012-09-28     aozima       fixed rt_spi_release_bus assert error.
 */

#include <drivers/spi.h>

extern rt_err_t rt_spi_bus_device_init(struct rt_spi_bus *bus, const char *name);
extern rt_err_t rt_spidev_device_init(struct rt_spi_device *dev, const char *name);

/* SPI总线设备注册 */
rt_err_t rt_spi_bus_register(struct rt_spi_bus       *bus,
                             const char              *name,
                             const struct rt_spi_ops *ops)
{
    rt_err_t result;
    /* 总线设备初始化 */
    result = rt_spi_bus_device_init(bus, name);
    if (result != RT_EOK)
        return result;

    /* 初始化互斥量 */
    rt_mutex_init(&(bus->lock), name, RT_IPC_FLAG_FIFO);
    /* 注册总线 方法  */
    bus->ops = ops;
    /* 初始化挂载设备  */
    bus->owner = RT_NULL;
    /* 设置SPI总线模式  */
    bus->mode = RT_SPI_BUS_MODE_SPI;

    return RT_EOK;
}
/* SPI设备（挂）挂载至SPI总线 */
rt_err_t rt_spi_bus_attach_device(struct rt_spi_device *device,
                                  const char           *name,
                                  const char           *bus_name,
                                  void                 *user_data)
{
    rt_err_t result;
    rt_device_t bus;

    /* 查询SPI总线基地址  */
    bus = rt_device_find(bus_name);
    /*  总线设备存在且为SPI总线设备类型 */
    if (bus != RT_NULL && bus->type == RT_Device_Class_SPIBUS)
    {
        /* SPI设备（挂）所在的SPI总线 */
        device->bus = (struct rt_spi_bus *)bus;

        /* 初始化SPI设备（挂） */
        result = rt_spidev_device_init(device, name);
        if (result != RT_EOK)
            return result;
        /*  初始化SPI设备（挂）空间 */
        rt_memset(&device->config, 0, sizeof(device->config));
        /*  初始化SPI设备用户数据 这里将CS控制块初始化至userdata*/
        device->parent.user_data = user_data;

        return RT_EOK;
    }
    /* not found the host bus */
    return -RT_ERROR;
}
/* SPI配置 */
rt_err_t rt_spi_configure(struct rt_spi_device        *device,
                          struct rt_spi_configuration *cfg)
{
    rt_err_t result;

    RT_ASSERT(device != RT_NULL);

    /* 设置配置 */
    device->config.data_width = cfg->data_width;    /* 设置数据宽度 */
    device->config.mode       = cfg->mode & RT_SPI_MODE_MASK ;/* 配置模式 */
    device->config.max_hz     = cfg->max_hz ;       /* 传输速率 */
    /* 总线设备存在 */
    if (device->bus != RT_NULL)
    {
        /* 获取互斥量 */
        result = rt_mutex_take(&(device->bus->lock), RT_WAITING_FOREVER);
        if (result == RT_EOK)
        {
            /* */
            if (device->bus->owner == device)
            {
                device->bus->ops->configure(device, &device->config);
            }

            /* 释放互斥量 */
            rt_mutex_release(&(device->bus->lock));
        }
    }

    return RT_EOK;
}
/* 总线数据发送：连续发送俩次
 * （1）SPI总线设备
 * （2）SPI发送数据buffer1
 * （3）发送数据长度1
 * （4）SPI发送数据buffer2
 * （5）发送数据长度2
 *  */
rt_err_t rt_spi_send_then_send(struct rt_spi_device *device,
                               const void           *send_buf1,
                               rt_size_t             send_length1,
                               const void           *send_buf2,
                               rt_size_t             send_length2)
{
    rt_err_t result;
    struct rt_spi_message message;

    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);
    /* 获取互斥量 */
    result = rt_mutex_take(&(device->bus->lock), RT_WAITING_FOREVER);
    if (result == RT_EOK)
    {
        /* */
        if (device->bus->owner != device)
        {
            /* not the same owner as current, re-configure SPI bus */
            result = device->bus->ops->configure(device, &device->config);
            if (result == RT_EOK)
            {
                /* set SPI bus owner */
                device->bus->owner = device;
            }
            else
            {
                /* configure SPI bus failed */
                result = -RT_EIO;
                goto __exit;
            }
        }

        /* send data1 */
        message.send_buf   = send_buf1;
        message.recv_buf   = RT_NULL;
        message.length     = send_length1;
        message.cs_take    = 1;
        message.cs_release = 0;
        message.next       = RT_NULL;

        result = device->bus->ops->xfer(device, &message);
        if (result == 0)
        {
            result = -RT_EIO;
            goto __exit;
        }

        /* send data2 */
        message.send_buf   = send_buf2;
        message.recv_buf   = RT_NULL;
        message.length     = send_length2;
        message.cs_take    = 0;
        message.cs_release = 1;
        message.next       = RT_NULL;

        result = device->bus->ops->xfer(device, &message);
        if (result == 0)
        {
            result = -RT_EIO;
            goto __exit;
        }

        result = RT_EOK;
    }
    else
    {
        return -RT_EIO;
    }

__exit:
    rt_mutex_release(&(device->bus->lock));

    return result;
}
/* SPI发送再接收 */
rt_err_t rt_spi_send_then_recv(struct rt_spi_device *device,
                               const void           *send_buf,
                               rt_size_t             send_length,
                               void                 *recv_buf,
                               rt_size_t             recv_length)
{
    rt_err_t result;
    struct rt_spi_message message;

    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);
    /* 获取互斥量 */
    result = rt_mutex_take(&(device->bus->lock), RT_WAITING_FOREVER);
    if (result == RT_EOK)
    {
        if (device->bus->owner != device)
        {
            /* not the same owner as current, re-configure SPI bus */
            result = device->bus->ops->configure(device, &device->config);
            if (result == RT_EOK)
            {
                /* set SPI bus owner */
                device->bus->owner = device;
            }
            else
            {
                /* configure SPI bus failed */
                result = -RT_EIO;
                goto __exit;
            }
        }

        /* send data */
        message.send_buf   = send_buf;
        message.recv_buf   = RT_NULL;
        message.length     = send_length;
        message.cs_take    = 1;
        message.cs_release = 0;
        message.next       = RT_NULL;

        result = device->bus->ops->xfer(device, &message);
        if (result == 0)
        {
            result = -RT_EIO;
            goto __exit;
        }

        /* recv data */
        message.send_buf   = RT_NULL;
        message.recv_buf   = recv_buf;
        message.length     = recv_length;
        message.cs_take    = 0;
        message.cs_release = 1;
        message.next       = RT_NULL;

        result = device->bus->ops->xfer(device, &message);
        if (result == 0)
        {
            result = -RT_EIO;
            goto __exit;
        }

        result = RT_EOK;
    }
    else
    {
        return -RT_EIO;
    }

__exit:
    rt_mutex_release(&(device->bus->lock));

    return result;
}
/*
  * 传输一次数据
  *此函数可以传输传输一次数据。此函数等同于调用rt_spi_transfer_message()传输一条消息，开始发送数据时片选选中，函数返回时释放片选。*/
rt_size_t rt_spi_transfer(struct rt_spi_device *device,
                          const void           *send_buf,
                          void                 *recv_buf,
                          rt_size_t             length)
{
    rt_err_t result;
    struct rt_spi_message message;/* SPI消息 */

    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);
    /* 获取信号量 */
    result = rt_mutex_take(&(device->bus->lock), RT_WAITING_FOREVER);
    /* 获取成功 */
    if (result == RT_EOK)
    {
        /* SPI总线未挂载当前的设备 */
        if (device->bus->owner != device)
        {
            /* not the same owner as current, re-configure SPI bus */
            /* 重新配置SPI设备（挂） */
            result = device->bus->ops->configure(device, &device->config);
            if (result == RT_EOK)
            {
                /* set SPI bus owner */
                /* 配置SPI设备（挂）当前挂载设备 */
                device->bus->owner = device;
            }
            else
            {
                /* configure SPI bus failed */
                rt_set_errno(-RT_EIO);
                result = 0;
                goto __exit;
            }
        }

        /* 初始化SPI消息块  */
        message.send_buf   = send_buf;
        message.recv_buf   = recv_buf;
        message.length     = length;
        message.cs_take    = 1;
        message.cs_release = 1;
        message.next       = RT_NULL;

        /* SPI总线发送消息  */
        result = device->bus->ops->xfer(device, &message);
        if (result == 0)
        {
            rt_set_errno(-RT_EIO);
            goto __exit;
        }
    }
    else
    {
        rt_set_errno(-RT_EIO);

        return 0;
    }

__exit:/* 释放互斥锁 */
    rt_mutex_release(&(device->bus->lock));
    /* 返回发送结果 */
    return result;
}

struct rt_spi_message *rt_spi_transfer_message(struct rt_spi_device  *device,
                                               struct rt_spi_message *message)
{
    rt_err_t result;
    struct rt_spi_message *index;

    RT_ASSERT(device != RT_NULL);

    /* get first message */
    index = message;
    if (index == RT_NULL)
        return index;

    result = rt_mutex_take(&(device->bus->lock), RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        rt_set_errno(-RT_EBUSY);

        return index;
    }

    /* reset errno */
    rt_set_errno(RT_EOK);

    /* configure SPI bus */
    if (device->bus->owner != device)
    {
        /* not the same owner as current, re-configure SPI bus */
        result = device->bus->ops->configure(device, &device->config);
        if (result == RT_EOK)
        {
            /* set SPI bus owner */
            device->bus->owner = device;
        }
        else
        {
            /* configure SPI bus failed */
            rt_set_errno(-RT_EIO);
            goto __exit;
        }
    }

    /* transmit each SPI message */
    while (index != RT_NULL)
    {
        /* transmit SPI message */
        result = device->bus->ops->xfer(device, index);
        if (result == 0)
        {
            rt_set_errno(-RT_EIO);
            break;
        }

        index = index->next;
    }

__exit:
    /* release bus lock */
    rt_mutex_release(&(device->bus->lock));

    return index;
}
/* 获取SPI总线 */
rt_err_t rt_spi_take_bus(struct rt_spi_device *device)
{
    rt_err_t result = RT_EOK;

    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);
    /* 获取互斥量 */
    result = rt_mutex_take(&(device->bus->lock), RT_WAITING_FOREVER);
    if (result != RT_EOK)
    {
        rt_set_errno(-RT_EBUSY);

        return -RT_EBUSY;
    }

    /* reset errno */
    rt_set_errno(RT_EOK);

    /* 配置SPI总线  */
    if (device->bus->owner != device)
    {
        /* not the same owner as current, re-configure SPI bus */
        result = device->bus->ops->configure(device, &device->config);
        if (result == RT_EOK)
        {
            /* set SPI bus owner */
            device->bus->owner = device;
        }
        else
        {
            /* configure SPI bus failed */
            rt_set_errno(-RT_EIO);
            /* release lock */
            rt_mutex_release(&(device->bus->lock));

            return -RT_EIO;
        }
    }

    return result;
}
/* 释放SPI总线设备 */
rt_err_t rt_spi_release_bus(struct rt_spi_device *device)
{
    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);
    RT_ASSERT(device->bus->owner == device);

    /* release lock */
    rt_mutex_release(&(device->bus->lock));

    return RT_EOK;
}
/* 片选SPI设备（挂） */
rt_err_t rt_spi_take(struct rt_spi_device *device)
{
    rt_err_t result;
    struct rt_spi_message message;

    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);
    /* */
    rt_memset(&message, 0, sizeof(message));
    message.cs_take = 1;
    /* 控制SPI设备（挂）*/
    result = device->bus->ops->xfer(device, &message);

    return result;
}
/* 释放SPI设备（挂）*/
rt_err_t rt_spi_release(struct rt_spi_device *device)
{
    rt_err_t result;
    struct rt_spi_message message;

    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);

    rt_memset(&message, 0, sizeof(message));
    message.cs_release = 1;
    /* 设备信息发送 */
    result = device->bus->ops->xfer(device, &message);

    return result;
}
