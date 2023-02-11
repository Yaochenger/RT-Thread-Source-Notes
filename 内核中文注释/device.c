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
/* ע��һ��RT-Thread�豸���� */
rt_err_t rt_device_register(rt_device_t dev,
                            const char *name,
                            rt_uint16_t flags)
{
    if (dev == RT_NULL)
        return -RT_ERROR;

    if (rt_device_find(name) != RT_NULL)
        return -RT_ERROR;
    /* ��ʼ��һ���豸���� */
    rt_object_init(&(dev->parent), RT_Object_Class_Device, name);
    dev->flag = flags;  /* �����豸��־ */
    dev->ref_count = 0; /* ����������  */
    dev->open_flag = 0; /* �򿪱�־  */

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
/* ȡ��ע�� */
rt_err_t rt_device_unregister(rt_device_t dev)
{
    /* parameter check */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    RT_ASSERT(rt_object_is_systemobject(&dev->parent));
    /* ������������ */
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
/* �����豸���� */
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
/* ��̬����һ���豸���� */
rt_device_t rt_device_create(int type, int attach_size)
{
    int size;
    rt_device_t device;/* �豸���� */

    size = RT_ALIGN(sizeof(struct rt_device), RT_ALIGN_SIZE); /* �豸��С */
    attach_size = RT_ALIGN(attach_size, RT_ALIGN_SIZE);/* �û����ݴ�С */
    /* use the total size */
    size += attach_size;/* �ܹ���Ҫ������ */

    device = (rt_device_t)rt_malloc(size);/* ��̬����ռ� */
    if (device)
    {
        /* ��ʼ���ռ����� */
        rt_memset(device, 0x0, sizeof(struct rt_device));
        /* ��ʼ���豸���� */
        device->type = (enum rt_device_class_type)type;
    }
    /* �����豸��ַ */
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
    /* ������� */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    RT_ASSERT(rt_object_is_systemobject(&dev->parent) == RT_FALSE);
    /* ���豸�������� */
    rt_object_detach(&(dev->parent));
    /* �ͷ�����Ŀռ� */
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
/* �豸��ʼ�� */
rt_err_t rt_device_init(rt_device_t dev)
{
    /* ��ʱ*/
    rt_err_t result = RT_EOK;

    RT_ASSERT(dev != RT_NULL);

    /* get device_init handler */ //device_init     (dev->init)
    if (device_init != RT_NULL)
    {
        /* ���豸�ı�־:�Ǽ��� */
        if (!(dev->flag & RT_DEVICE_FLAG_ACTIVATED))
        {
            /* ��ʼ���豸 */
            result = device_init(dev);
            if (result != RT_EOK)
            {
                RT_DEBUG_LOG(RT_DEBUG_DEVICE, ("To initialize device:%s failed. The error code is %d\n",
                           dev->parent.name, result));
            }
            else
            {   /* �豸�ı�־:���� */
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

/* ���豸 */
rt_err_t rt_device_open(rt_device_t dev, rt_uint16_t oflag)
{
    /* �豸��־ */
    rt_err_t result = RT_EOK;

    /* ������� */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);

    /* ���豸״̬ == δ���� */
    if (!(dev->flag & RT_DEVICE_FLAG_ACTIVATED))
    {
        /* �豸δ��ʼ�� */
        if (device_init != RT_NULL)
        {
            /* ��ʼ���豸 */
            result = device_init(dev);
            if (result != RT_EOK)
            {
                RT_DEBUG_LOG(RT_DEBUG_DEVICE, ("To initialize device:%s failed. The error code is %d\n",
                           dev->parent.name, result));
                /* �����豸��ʼ����� */
                return result;
            }
        }
        /* �����豸��־:���� */
        dev->flag |= RT_DEVICE_FLAG_ACTIVATED;
    }

    /* �����豸�����״̬���� */
    if ((dev->flag & RT_DEVICE_FLAG_STANDALONE) &&
        (dev->open_flag & RT_DEVICE_OFLAG_OPEN))
    {
        return -RT_EBUSY;
    }

    /* �������򿪽ӿ� */
    if (device_open != RT_NULL)
    {
        /* ���豸 */
        result = device_open(dev, oflag);
    }
    elsae
    {
        /* �����豸�򿪱�־ */
        dev->open_flag = (oflag & RT_DEVICE_OFLAG_MASK);
    }

    /* �����豸�򿪱�־ */
    if (result == RT_EOK || result == -RT_ENOSYS)
    {
        /* �����豸�򿪱�־ */
        dev->open_flag |= RT_DEVICE_OFLAG_OPEN;
        /* �����豸�򿪱�־ */
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

    /* ������� */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    /* �豸�򿪴��� == 0*/
    if (dev->ref_count == 0)
        return -RT_ERROR;
    /* �豸���������Լ� */
    dev->ref_count--;
    /* ������������ */
    if (dev->ref_count != 0)
        return RT_EOK;

    /* �����豸�رսӿ� */
    if (device_close != RT_NULL)
    {
        /* �ر��豸 */
        result = device_close(dev);
    }

    /* �����豸�ı�־Ϊ�ر� */
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
/* �豸��*/
rt_size_t rt_device_read(rt_device_t dev,
                         rt_off_t    pos,
                         void       *buffer,
                         rt_size_t   size)
{
    /* ������� */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    /* �豸�������� */
    if (dev->ref_count == 0)
    {
        rt_set_errno(-RT_ERROR);
        return 0;
    }

    /* ���ú������ӿ� */
    if (device_read != RT_NULL)
    {
        return device_read(dev, pos, buffer, size);
    }

    /* ����errno */
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
/* �豸д */
rt_size_t rt_device_write(rt_device_t dev,
                          rt_off_t    pos,
                          const void *buffer,
                          rt_size_t   size)
{
    /* ������� */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    /* �豸�򿪴��� */
    if (dev->ref_count == 0)
    {
        rt_set_errno(-RT_ERROR);
        return 0;
    }

    /* �����豸д�ӿ� */
    if (device_write != RT_NULL)
    {
        return device_write(dev, pos, buffer, size);
    }
    /* �����豸�ӿ� */
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
/* �豸���� */
rt_err_t rt_device_control(rt_device_t dev, int cmd, void *arg)
{
    /* ������� */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);

    /* �����豸���ƽӿ� */
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
/* ���ý��պ��� */
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
/* �����豸���ͽӿ� */
rt_err_t rt_device_set_tx_complete(rt_device_t dev,
                                   rt_err_t (*tx_done)(rt_device_t dev,
                                   void *buffer))
{
    /* parameter check */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(rt_object_get_type(&dev->parent) == RT_Object_Class_Device);
    /* �豸���� */
    dev->tx_complete = tx_done;

    return RT_EOK;
}
RTM_EXPORT(rt_device_set_tx_complete);

#endif /* RT_USING_DEVICE */
