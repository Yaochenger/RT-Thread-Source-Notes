/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 */

#include <rtthread.h>
#include <drivers/spi.h>

/* SPI bus device interface, compatible with RT-Thread 0.3.x/1.0.x */
/* SPI设备读
 * （1）设备
 * （2）读偏移
 * （3）读buffer
 * （4）读数据量
 * */
static rt_size_t _spi_bus_device_read(rt_device_t dev,
                                      rt_off_t    pos,
                                      void       *buffer,
                                      rt_size_t   size)
{
    struct rt_spi_bus *bus;

    bus = (struct rt_spi_bus *)dev; /* 强制设备类型转换 */
    RT_ASSERT(bus != RT_NULL);
    RT_ASSERT(bus->owner != RT_NULL);
    /* SPI总线读 */
    return rt_spi_transfer(bus->owner, RT_NULL, buffer, size);
}
/* SPI设备写
 * （1）设备
 * （2）写偏移
 * （3）写buffer
 * （4）写数据量
 * */
static rt_size_t _spi_bus_device_write(rt_device_t dev,
                                       rt_off_t    pos,
                                       const void *buffer,
                                       rt_size_t   size)
{
    struct rt_spi_bus *bus;

    bus = (struct rt_spi_bus *)dev; /* 强制设备类型转换 */
    RT_ASSERT(bus != RT_NULL);
    RT_ASSERT(bus->owner != RT_NULL);
    /* SPI总线写 */
    return rt_spi_transfer(bus->owner, buffer, RT_NULL, size);
}
/* SPI设备控制
 * （1）设备
 * （2）命令
 * （3）参数
 * */
static rt_err_t _spi_bus_device_control(rt_device_t dev,
                                        int         cmd,
                                        void       *args)
{
    /* TODO: add control command handle */
    switch (cmd)
    {
    case 0: /* set device */
        break;
    case 1:
        break;
    }

    return RT_EOK;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops spi_bus_ops =
{
    RT_NULL,
    RT_NULL,
    RT_NULL,
    _spi_bus_device_read,
    _spi_bus_device_write,
    _spi_bus_device_control
};
#endif

/*
 * SPI设备初始化
 * （1）SPI总线
 * （2）SPI总线名称
 * */
rt_err_t rt_spi_bus_device_init(struct rt_spi_bus *bus, const char *name)
{
    struct rt_device *device;
    RT_ASSERT(bus != RT_NULL);

    device = &bus->parent;/* 总线的爸爸 rt_device */

    /* 设置总线类型 */
    device->type    = RT_Device_Class_SPIBUS;
    /* 初始化总线接口 */
#ifdef RT_USING_DEVICE_OPS
    device->ops     = &spi_bus_ops;
#else
    device->init    = RT_NULL;
    device->open    = RT_NULL;
    device->close   = RT_NULL;
    device->read    = _spi_bus_device_read;
    device->write   = _spi_bus_device_write;
    device->control = _spi_bus_device_control;
#endif
    /* 将设备注册到设备管理器  */
    return rt_device_register(device, name, RT_DEVICE_FLAG_RDWR);
}

/* SPI Dev device interface, compatible with RT-Thread 0.3.x/1.0.x */
/* SPI设备读*/
static rt_size_t _spidev_device_read(rt_device_t dev,
                                     rt_off_t    pos,
                                     void       *buffer,
                                     rt_size_t   size)
{
    struct rt_spi_device *device;

    device = (struct rt_spi_device *)dev;
    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);

    return rt_spi_transfer(device, RT_NULL, buffer, size);
}
/* 设备写 */
static rt_size_t _spidev_device_write(rt_device_t dev,
                                      rt_off_t    pos,
                                      const void *buffer,
                                      rt_size_t   size)
{
    struct rt_spi_device *device;

    device = (struct rt_spi_device *)dev;
    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(device->bus != RT_NULL);

    return rt_spi_transfer(device, buffer, RT_NULL, size);
}

static rt_err_t _spidev_device_control(rt_device_t dev,
                                       int         cmd,
                                       void       *args)
{
    switch (cmd)
    {
    case 0: /* set device */
        break;
    case 1:
        break;
    }

    return RT_EOK;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops spi_device_ops =
{
    RT_NULL,
    RT_NULL,
    RT_NULL,
    _spidev_device_read,
    _spidev_device_write,
    _spidev_device_control
};
#endif

rt_err_t rt_spidev_device_init(struct rt_spi_device *dev, const char *name)
{
    struct rt_device *device;
    RT_ASSERT(dev != RT_NULL);

    device = &(dev->parent);

    /* set device type */
    device->type    = RT_Device_Class_SPIDevice;
#ifdef RT_USING_DEVICE_OPS
    device->ops     = &spi_device_ops;
#else
    device->init    = RT_NULL;
    device->open    = RT_NULL;
    device->close   = RT_NULL;
    device->read    = _spidev_device_read;
    device->write   = _spidev_device_write;
    device->control = _spidev_device_control;
#endif

    /* register to device manager */
    return rt_device_register(device, name, RT_DEVICE_FLAG_RDWR);
}
