/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2007-01-21     Bernard      the first version
 * 2010-05-04     Bernard      add rt_device_init implementation
 * 2012-10-20     Bernard      add device check in register function,
 *                             provided by Rob <rdent@iinet.net.au>
 * 2012-12-25     Bernard      return RT_EOK if the device interface not exist.
 * 2013-07-09     Grissiom     add ref_count support
 * 2016-04-02     Bernard      fix the open_flag initialization issue.
 * 2021-03-19     Meco Man     remove rt_device_init_all()
 */

#include <rtthread.h>
#ifdef RT_USING_POSIX_DEVIO
#include <rtdevice.h> /* for wqueue_init */
#endif /* RT_USING_POSIX_DEVIO */

#ifdef RT_USING_DEVICE

#ifdef RT_USING_DEVICE_OPS
#define device_init     (dev->ops->init)
#define device_open     (dev->ops->open)
#define device_close    (dev->ops->close)
#define device_read     (dev->ops->read)
#define device_write    (dev->ops->write)
#define device_control  (dev->ops->control)
#else
#define device_init     (dev->init)
#define device_open     (dev->open)
#define device_close    (dev->close)
#define device_read     (dev->read)
#define device_write    (dev->write)
#define device_control  (dev->control)
#endif /* RT_USING_DEVICE_OPS */

/**
 * @brief This function registers a device driver with a specified name.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @param name is the device driver's name.
 *
 * @param flags is the capabilities flag of device.
 *
 * @return the error code, RT_EOK on initialization successfully.
 */
/* 注册一个RT-Thread设备对象 */
rt_err_t rt_device_register(rt_device_t dev,
                            const char *name,
                            rt_uint16_t flags)
{
    if (dev == RT_NULL)
        return -RT_ERROR;

    if (rt_device_find(name) != RT_NULL)
        return -RT_ERROR;
    /* 初始化一个设备对象 */
    rt_object_init(&(dev->parent), RT_Object_Class_Device, name);
    dev->flag = flags;  /* 设置设备标志 */
    dev->ref_count = 0; /* 被操作次数  */
    dev->open_flag = 0; /* 打开标志  */

    return RT_EOK;
}
RTM_EXPORT(rt_device_register);

/**
 * @brief This function removes a previously registered device driver.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @return the error code, RT_EOK on successfully.
 */
/* 取消注册 */
rt_err_t rt_device_unregister(rt_device_t dev)
{
    /* parameter check */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    RT_ASSERT(rt_object_is_systemobject(&dev->parent));
    /* 将设别对象脱离 */
    rt_object_detach(&(dev->parent));

    return RT_EOK;
}
RTM_EXPORT(rt_device_unregister);

/**
 * @brief This function finds a device driver by specified name.
 *
 * @param name is the device driver's name.
 *
 * @return the registered device driver on successful, or RT_NULL on failure.
 */
/* 查找设备对象 */
rt_device_t rt_device_find(const char *name)
{
    return (rt_device_t)rt_object_find(name, RT_Object_Class_Device);
}
RTM_EXPORT(rt_device_find);

#ifdef RT_USING_HEAP
/**
 * @brief This function creates a device object with user data size.
 *
 * @param type is the type of the device object.
 *
 * @param attach_size is the size of user data.
 *
 * @return the allocated device object, or RT_NULL when failed.
 */
/* 动态床架一个设备对象 */
rt_device_t rt_device_create(int type, int attach_size)
{
    int size;
    rt_device_t device;/* 设备对象 */

    size = RT_ALIGN(sizeof(struct rt_device), RT_ALIGN_SIZE); /* 设备大小 */
    attach_size = RT_ALIGN(attach_size, RT_ALIGN_SIZE);/* 用户数据大小 */
    /* use the total size */
    size += attach_size;/* 总共需要的数据 */

    device = (rt_device_t)rt_malloc(size);/* 动态申请空间 */
    if (device)
    {
        /* 初始化空间内容 */
        rt_memset(device, 0x0, sizeof(struct rt_device));
        /* 初始化设备类型 */
        device->type = (enum rt_device_class_type)type;
    }
    /* 返回设备地址 */
    return device;
}
RTM_EXPORT(rt_device_create);

/**
 * @brief This function destroy the specific device object.
 *
 * @param dev is a specific device object.
 */
void rt_device_destroy(rt_device_t dev)
{
    /* 参数检查 */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    RT_ASSERT(rt_object_is_systemobject(&dev->parent) == RT_FALSE);
    /* 将设备对象脱离 */
    rt_object_detach(&(dev->parent));
    /* 释放申请的空间 */
    rt_free(dev);
}
RTM_EXPORT(rt_device_destroy);
#endif /* RT_USING_HEAP */

/**
 * @brief This function will initialize the specified device.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @return the result, RT_EOK on successfully.
 */
/* 设备初始化 */
rt_err_t rt_device_init(rt_device_t dev)
{
    /* 临时*/
    rt_err_t result = RT_EOK;

    RT_ASSERT(dev != RT_NULL);

    /* get device_init handler */ //device_init     (dev->init)
    if (device_init != RT_NULL)
    {
        /* 若设备的标志:非激活 */
        if (!(dev->flag & RT_DEVICE_FLAG_ACTIVATED))
        {
            /* 初始化设备 */
            result = device_init(dev);
            if (result != RT_EOK)
            {
                RT_DEBUG_LOG(RT_DEBUG_DEVICE, ("To initialize device:%s failed. The error code is %d\n",
                           dev->parent.name, result));
            }
            else
            {   /* 设备的标志:激活 */
                dev->flag |= RT_DEVICE_FLAG_ACTIVATED;
            }
        }
    }

    return result;
}

/**
 * @brief This function will open a device.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @param oflag is the flags for device open.
 *
 * @return the result, RT_EOK on successfully.
 */

/* 打开设备 */
rt_err_t rt_device_open(rt_device_t dev, rt_uint16_t oflag)
{
    /* 设备标志 */
    rt_err_t result = RT_EOK;

    /* 参数检查 */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);

    /* 若设备状态 == 未激活 */
    if (!(dev->flag & RT_DEVICE_FLAG_ACTIVATED))
    {
        /* 设备未初始化 */
        if (device_init != RT_NULL)
        {
            /* 初始化设备 */
            result = device_init(dev);
            if (result != RT_EOK)
            {
                RT_DEBUG_LOG(RT_DEBUG_DEVICE, ("To initialize device:%s failed. The error code is %d\n",
                           dev->parent.name, result));
                /* 返回设备初始化结果 */
                return result;
            }
        }
        /* 设置设备标志:激活 */
        dev->flag |= RT_DEVICE_FLAG_ACTIVATED;
    }

    /* 独立设备且设别状态：打开 */
    if ((dev->flag & RT_DEVICE_FLAG_STANDALONE) &&
        (dev->open_flag & RT_DEVICE_OFLAG_OPEN))
    {
        return -RT_EBUSY;
    }

    /* 调用设别打开接口 */
    if (device_open != RT_NULL)
    {
        /* 打开设备 */
        result = device_open(dev, oflag);
    }
    elsae
    {
        /* 设置设备打开标志 */
        dev->open_flag = (oflag & RT_DEVICE_OFLAG_MASK);
    }

    /* 设置设备打开标志 */
    if (result == RT_EOK || result == -RT_ENOSYS)
    {
        /* 设置设备打开标志 */
        dev->open_flag |= RT_DEVICE_OFLAG_OPEN;
        /* 设置设备打开标志 */
        dev->ref_count++;
        /* don't let bad things happen silently. If you are bitten by this assert,
         * please set the ref_count to a bigger type. */
        RT_ASSERT(dev->ref_count != 0);
    }

    return result;
}
RTM_EXPORT(rt_device_open);

/**
 * @brief This function will close a device.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @return the result, RT_EOK on successfully.
 */
/* */
rt_err_t rt_device_close(rt_device_t dev)
{
    rt_err_t result = RT_EOK;

    /* 参数检查 */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    /* 设备打开次数 == 0*/
    if (dev->ref_count == 0)
        return -RT_ERROR;
    /* 设备操作参数自减 */
    dev->ref_count--;
    /* 操作参数非零 */
    if (dev->ref_count != 0)
        return RT_EOK;

    /* 调用设备关闭接口 */
    if (device_close != RT_NULL)
    {
        /* 关闭设备 */
        result = device_close(dev);
    }

    /* 设置设备的标志为关闭 */
    if (result == RT_EOK || result == -RT_ENOSYS)
        dev->open_flag = RT_DEVICE_OFLAG_CLOSE;

    return result;
}
RTM_EXPORT(rt_device_close);

/**
 * @brief This function will read some data from a device.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @param pos is the position when reading.
 *
 * @param buffer is a data buffer to save the read data.
 *
 * @param size is the size of buffer.
 *
 * @return the actually read size on successful, otherwise 0 will be returned.
 *
 * @note the unit of size/pos is a block for block device.
 */
/* 设备读*/
rt_size_t rt_device_read(rt_device_t dev,
                         rt_off_t    pos,
                         void       *buffer,
                         rt_size_t   size)
{
    /* 参数检查 */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    /* 设备操作参数 */
    if (dev->ref_count == 0)
    {
        rt_set_errno(-RT_ERROR);
        return 0;
    }

    /* 调用函数读接口 */
    if (device_read != RT_NULL)
    {
        return device_read(dev, pos, buffer, size);
    }

    /* 设置errno */
    rt_set_errno(-RT_ENOSYS);

    return 0;
}
RTM_EXPORT(rt_device_read);

/**
 * @brief This function will write some data to a device.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @param pos is the position when writing.
 *
 * @param buffer is the data buffer to be written to device.
 *
 * @param size is the size of buffer.
 *
 * @return the actually written size on successful, otherwise 0 will be returned.
 *
 * @note the unit of size/pos is a block for block device.
 */
/* 设备写 */
rt_size_t rt_device_write(rt_device_t dev,
                          rt_off_t    pos,
                          const void *buffer,
                          rt_size_t   size)
{
    /* 参数检查 */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    /* 设备打开次数 */
    if (dev->ref_count == 0)
    {
        rt_set_errno(-RT_ERROR);
        return 0;
    }

    /* 调用设备写接口 */
    if (device_write != RT_NULL)
    {
        return device_write(dev, pos, buffer, size);
    }
    /* 设置设备接口 */
    rt_set_errno(-RT_ENOSYS);

    return 0;
}
RTM_EXPORT(rt_device_write);

/**
 * @brief This function will perform a variety of control functions on devices.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @param cmd is the command sent to device.
 *
 * @param arg is the argument of command.
 *
 * @return the result, -RT_ENOSYS for failed.
 */
/* 设备控制 */
rt_err_t rt_device_control(rt_device_t dev, int cmd, void *arg)
{
    /* 参数检查 */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);

    /* 调用设备控制接口 */
    if (device_control != RT_NULL)
    {
        return device_control(dev, cmd, arg);
    }

    return -RT_ENOSYS;
}
RTM_EXPORT(rt_device_control);

/**
 * @brief This function will set the reception indication callback function. This callback function
 *        is invoked when this device receives data.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @param rx_ind is the indication callback function.
 *
 * @return RT_EOK
 */
/* 设置接收函数 */
rt_err_t rt_device_set_rx_indicate(rt_device_t dev,
                                   rt_err_t (*rx_ind)(rt_device_t dev,
                                   rt_size_t size))
{
    /* parameter check */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);

    dev->rx_indicate = rx_ind;

    return RT_EOK;
}
RTM_EXPORT(rt_device_set_rx_indicate);

/**
 * @brief This function will set a callback function. The callback function
 *        will be called when device has written data to physical hardware.
 *
 * @param dev is the pointer of device driver structure.
 *
 * @param tx_done is the indication callback function.
 *
 * @return RT_EOK
 */
/* 设置设备发送接口 */
rt_err_t rt_device_set_tx_complete(rt_device_t dev,
                                   rt_err_t (*tx_done)(rt_device_t dev,
                                   void *buffer))
{
    /* parameter check */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    /* 设备发送 */
    dev->tx_complete = tx_done;

    return RT_EOK;
}
RTM_EXPORT(rt_device_set_tx_complete);

#endif /* RT_USING_DEVICE */
