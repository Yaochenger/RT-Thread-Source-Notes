/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-03-13     bernard      first version
 * 2012-05-15     lgnq         modified according bernard's implementation.
 * 2012-05-28     bernard      code cleanup
 * 2012-11-23     bernard      fix compiler warning.
 * 2013-02-20     bernard      use RT_SERIAL_RB_BUFSZ to define
 *                             the size of ring buffer.
 * 2014-07-10     bernard      rewrite serial framework
 * 2014-12-31     bernard      use open_flag for poll_tx stream mode.
 * 2015-05-19     Quintin      fix DMA tx mod tx_dma->activated flag !=RT_FALSE BUG
 *                             in open function.
 * 2015-11-10     bernard      fix the poll rx issue when there is no data.
 * 2016-05-10     armink       add fifo mode to DMA rx when serial->config.bufsz != 0.
 * 2017-01-19     aubr.cool    prevent change serial rx bufsz when serial is opened.
 * 2017-11-07     JasonJia     fix data bits error issue when using tcsetattr.
 * 2017-11-15     JasonJia     fix poll rx issue when data is full.
 *                             add TCFLSH and FIONREAD support.
 * 2018-12-08     Ernest Chen  add DMA choice
 * 2020-09-14     WillianChan  add a line feed to the carriage return character
 *                             when using interrupt tx
 * 2020-12-14     Meco Man     add function of setting window's size(TIOCSWINSZ)
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>

#define DBG_TAG    "UART"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

/*
 * 串口轮询接收
 * （1）串口设备
 * （2）接收数据缓存区
 * （3）需要接收的数据长度
 */
rt_inline int _serial_poll_rx(struct rt_serial_device *serial, rt_uint8_t *data, int length)
{
    int ch;
    int size;

    RT_ASSERT(serial != RT_NULL);
    size = length;

    while (length)
    {
        ch = serial->ops->getc(serial);/* 调用设备方法读取数据 */
        if (ch == -1) break;

        *data = ch;/* 将数据保存至 data */
        data ++; length --;
        /* 若打开标志为RT_DEVICE_FLAG_STREAM 遇到回车则推出  */
        if(serial->parent.open_flag & RT_DEVICE_FLAG_STREAM)
        {
            if (ch == '\n') break;
        }
    }
    /* 返回读取到的数据的个数 */
    return size - length;
}
/*
 * 串口轮询发送
 * （1）串口设备
 * （2）发送数据缓存区
 * （3）需要接收的数据长度
 */
rt_inline int _serial_poll_tx(struct rt_serial_device *serial, const rt_uint8_t *data, int length)
{
    int size;
    RT_ASSERT(serial != RT_NULL);

    size = length;
    while (length)
    {   /* 若data为回车 且设备的标志为流 */
        if (*data == '\n' && (serial->parent.open_flag & RT_DEVICE_FLAG_STREAM))
        {
            /* 发送换行 */
            serial->ops->putc(serial, '\r');
        }
        /* 发送数据 */
        serial->ops->putc(serial, *data);

        ++ data;
        -- length;
    }
    /* 返回发送的数据长度 */
    return size - length;
}

/*
 *  串口中断接收
 * （1）串口设备
 * （2）希望接收的数据
 * （3）希望接收的数据长度
 */
rt_inline int _serial_int_rx(struct rt_serial_device *serial, rt_uint8_t *data, int length)
{
    int size;
    struct rt_serial_rx_fifo* rx_fifo;/* 接收FIFO控制块 */

    RT_ASSERT(serial != RT_NULL);
    size = length;/* */

    rx_fifo = (struct rt_serial_rx_fifo*) serial->serial_rx;/* 接收FIFO控制块索引获取 */
    RT_ASSERT(rx_fifo != RT_NULL);

    /* read from software FIFO */
    while (length)
    {
        int ch;/* 接收数据 */
        rt_base_t level;

        /* 关全局中断 */
        level = rt_hw_interrupt_disable();

        /* 无数据 */
        if ((rx_fifo->get_index == rx_fifo->put_index) && (rx_fifo->is_full == RT_FALSE))
        {
            /* 使能全局中断 */
            rt_hw_interrupt_enable(level);
            break;
        }
        /* 从接收FIFO中获取数据 */
        ch = rx_fifo->buffer[rx_fifo->get_index];
        /* 接收数据与索引递增 */
        rx_fifo->get_index += 1;
        /* 接收的数据超出串口配置的大小 接收数据索引清零 */
        if (rx_fifo->get_index >= serial->config.bufsz) rx_fifo->get_index = 0;
        /* 接收FIFO满flag修正 */
        if (rx_fifo->is_full == RT_TRUE)
        {
            rx_fifo->is_full = RT_FALSE;
        }
        /* 使能全局中断 */
        rt_hw_interrupt_enable(level);
        /* 将接收的数据保存至 data */
        *data = ch & 0xff;
        /* 接收的数据自增 数据长度自减 */
        data ++; length --;
    }

    return size - length;
}
/*
 *  串口中断发送
 * （1）串口设备
 * （2）希望发送的数据
 * （3）希望发送的数据长度
 */
rt_inline int _serial_int_tx(struct rt_serial_device *serial, const rt_uint8_t *data, int length)
{
    int size;
    struct rt_serial_tx_fifo *tx;

    RT_ASSERT(serial != RT_NULL);

    size = length;
    tx = (struct rt_serial_tx_fifo*) serial->serial_tx;/* 发送数据控制块索引 */
    RT_ASSERT(tx != RT_NULL);

    while (length)
    {
        /* 若data为回车 且设备的标志为流 */
        if (*data == '\n' && (serial->parent.open_flag & RT_DEVICE_FLAG_STREAM))
        {
            /* 发送换行 */
            if (serial->ops->putc(serial, '\r') == -1)
            {
                rt_completion_wait(&(tx->completion), RT_WAITING_FOREVER);
                continue;
            }
        }
        /* 发送数据 */
        if (serial->ops->putc(serial, *(char*)data) == -1)
        {
            rt_completion_wait(&(tx->completion), RT_WAITING_FOREVER);
            continue;
        }

        data ++; length --;
    }

    return size - length;
}
/* 串口buffer检查 */
static void _serial_check_buffer_size(void)
{
    /* 执行到该函数 说明buffer已经满了 */
    static rt_bool_t already_output = RT_FALSE;

    if (already_output == RT_FALSE)
    {
#if !defined(RT_USING_ULOG) || defined(ULOG_USING_ISR_LOG)
        LOG_W("Warning: There is no enough buffer for saving data,"
              " please increase the RT_SERIAL_RB_BUFSZ option.");
#endif
        already_output = RT_TRUE;
    }
}


/* RT-Thread Device Interface */
/*
 * This function initializes serial device.
 */
/*
 *  串口框架初始化
 * （1）设备
 *  */
static rt_err_t rt_serial_init(struct rt_device *dev)
{
    rt_err_t result = RT_EOK;
    struct rt_serial_device *serial;/* 串口设备 */

    RT_ASSERT(dev != RT_NULL);
    serial = (struct rt_serial_device *)dev;/* 强制转为串口设备  */

    serial->serial_rx = RT_NULL;/* 接收FIFO首地址 */
    serial->serial_tx = RT_NULL;/* 发送FIFO首地址 */

    /* 应用配置 */
    if (serial->ops->configure)
        result = serial->ops->configure(serial, &serial->config);/* 调用设备方法 */

    return result;
}
/*
 * 串口框架打开
 * （1）设备
 * （2）打开标志
 * */
static rt_err_t rt_serial_open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_uint16_t stream_flag = 0;
    struct rt_serial_device *serial;

    RT_ASSERT(dev != RT_NULL);
    serial = (struct rt_serial_device *)dev;/* 强制转为串口设备 */

    LOG_D("open serial device: 0x%08x with open flag: 0x%04x",
        dev, oflag);
    /* 检查参数是否支持  */
    if ((oflag & RT_DEVICE_FLAG_DMA_RX) && !(dev->flag & RT_DEVICE_FLAG_DMA_RX))
        return -RT_EIO;
    if ((oflag & RT_DEVICE_FLAG_DMA_TX) && !(dev->flag & RT_DEVICE_FLAG_DMA_TX))
        return -RT_EIO;
    if ((oflag & RT_DEVICE_FLAG_INT_RX) && !(dev->flag & RT_DEVICE_FLAG_INT_RX))
        return -RT_EIO;
    if ((oflag & RT_DEVICE_FLAG_INT_TX) && !(dev->flag & RT_DEVICE_FLAG_INT_TX))
        return -RT_EIO;

    /* 保持原来的流模式不变 */
    if ((oflag & RT_DEVICE_FLAG_STREAM) || (dev->open_flag & RT_DEVICE_FLAG_STREAM))
        stream_flag = RT_DEVICE_FLAG_STREAM;

    /* 设置打开标志 */
    dev->open_flag = oflag & 0xff;

    /* 根据接收的标志设置 进行初始化 */
    if (serial->serial_rx == RT_NULL)
    {
        /* 接收的标志为 中断接收 轮询发送 */
        if (oflag & RT_DEVICE_FLAG_INT_RX)
        {   /* 接收FIFO控制块  */
            struct rt_serial_rx_fifo* rx_fifo;
            /* 为设备分配空间 */
            rx_fifo = (struct rt_serial_rx_fifo*) rt_malloc (sizeof(struct rt_serial_rx_fifo) + serial->config.bufsz);
            RT_ASSERT(rx_fifo != RT_NULL);
            /* rxFIFO地址初始化  buffer指向为其分配的地址 */
            rx_fifo->buffer = (rt_uint8_t*) (rx_fifo + 1);//(这块是C指针的小知识 )
            /* rxFIFO初始化 将buffer空间初始化为0 */
            rt_memset(rx_fifo->buffer, 0, serial->config.bufsz);
            /* 输出索引 设置输出字符的索引 */
            rx_fifo->put_index = 0;
            /* 输入索引 设置输入字符的索引 */
            rx_fifo->get_index = 0;
            /* FIFO满flag */
            rx_fifo->is_full = RT_FALSE;
            /* 接收数据索引 : 信息头 + buffer空间  */
            serial->serial_rx = rx_fifo;
            /* 打开中断标志设置 */
            dev->open_flag |= RT_DEVICE_FLAG_INT_RX;
            /* 调用设备方法 设置中断接收  */
            serial->ops->control(serial, RT_DEVICE_CTRL_SET_INT, (void *)RT_DEVICE_FLAG_INT_RX);
        }
        else
        {   /* 不操作 接收FIFO地址 */
            serial->serial_rx = RT_NULL;
        }
    }
    else /* 不做操作 */
    {
        if (oflag & RT_DEVICE_FLAG_INT_RX)
            dev->open_flag |= RT_DEVICE_FLAG_INT_RX;
    }
    /* 接收FIFO索引地址为空 */
    if (serial->serial_tx == RT_NULL)
    {   /* 接收的标志为 轮询接收 中断发送 */
        if (oflag & RT_DEVICE_FLAG_INT_TX)
        {
            struct rt_serial_tx_fifo *tx_fifo;
            /* 分配空间 */
            tx_fifo = (struct rt_serial_tx_fifo*) rt_malloc(sizeof(struct rt_serial_tx_fifo));
            RT_ASSERT(tx_fifo != RT_NULL);

            rt_completion_init(&(tx_fifo->completion));
            serial->serial_tx = tx_fifo;

            dev->open_flag |= RT_DEVICE_FLAG_INT_TX;
            /* configure low level device */
            serial->ops->control(serial, RT_DEVICE_CTRL_SET_INT, (void *)RT_DEVICE_FLAG_INT_TX);
        }
        else
        {
            serial->serial_tx = RT_NULL;
        }
    }
    else
    {
        if (oflag & RT_DEVICE_FLAG_INT_TX)
            dev->open_flag |= RT_DEVICE_FLAG_INT_TX;
    }

    /* set stream flag */
    dev->open_flag |= stream_flag;

    return RT_EOK;
}
/* 串口框架关闭 */
static rt_err_t rt_serial_close(struct rt_device *dev)
{
    struct rt_serial_device *serial;

    RT_ASSERT(dev != RT_NULL);
    serial = (struct rt_serial_device *)dev;/* 强制转为串口设备 */

    /* 设备状态：打开 */
    if (dev->ref_count > 1) return RT_EOK;
    /* 设备flag:中断接收 */
    if (dev->open_flag & RT_DEVICE_FLAG_INT_RX)
    {
        struct rt_serial_rx_fifo* rx_fifo;

        /* 调用设备方法配置 清除中断使能 */
        serial->ops->control(serial, RT_DEVICE_CTRL_CLR_INT, (void*)RT_DEVICE_FLAG_INT_RX);
        /* 取消使能设备中断接收flag */
        dev->open_flag &= ~RT_DEVICE_FLAG_INT_RX;
        /* 获取接收FIFO首地址 */
        rx_fifo = (struct rt_serial_rx_fifo*)serial->serial_rx;
        RT_ASSERT(rx_fifo != RT_NULL);
        /* 释放空间 */
        rt_free(rx_fifo);
        serial->serial_rx = RT_NULL;

    }
    if (dev->open_flag & RT_DEVICE_FLAG_INT_TX)
    {
        struct rt_serial_tx_fifo* tx_fifo;

        serial->ops->control(serial, RT_DEVICE_CTRL_CLR_INT, (void*)RT_DEVICE_FLAG_INT_TX);
        dev->open_flag &= ~RT_DEVICE_FLAG_INT_TX;

        tx_fifo = (struct rt_serial_tx_fifo*)serial->serial_tx;
        RT_ASSERT(tx_fifo != RT_NULL);

        rt_free(tx_fifo);
        serial->serial_tx = RT_NULL;

        /* configure low level device */
    }
    serial->ops->control(serial, RT_DEVICE_CTRL_CLOSE, RT_NULL);
    dev->flag &= ~RT_DEVICE_FLAG_ACTIVATED;

    return RT_EOK;
}
/* 串口框架读
 * （1）设备
 * （2）读取偏移
 * （3）接收buffer
 * （4）接收大小
 * */
static rt_size_t rt_serial_read(struct rt_device *dev,
                                rt_off_t          pos,
                                void             *buffer,
                                rt_size_t         size)
{
    struct rt_serial_device *serial;

    RT_ASSERT(dev != RT_NULL);
    if (size == 0) return 0;

    serial = (struct rt_serial_device *)dev;/* 强制转为串口设备 */
    /* 设备标志：中断接收 */
    if (dev->open_flag & RT_DEVICE_FLAG_INT_RX)
    {
        /* 中断接收 */
        return _serial_int_rx(serial, (rt_uint8_t *)buffer, size);
    }
    /* 轮询接收 */
    return _serial_poll_rx(serial, (rt_uint8_t *)buffer, size);
}
/* 串口框架写
 * （1）设备
 * （2）写偏移
 * （3）写buffer
 * （4）写数据大小
 * */
static rt_size_t rt_serial_write(struct rt_device *dev,
                                 rt_off_t          pos,
                                 const void       *buffer,
                                 rt_size_t         size)
{
    struct rt_serial_device *serial;

    RT_ASSERT(dev != RT_NULL);
    if (size == 0) return 0;

    serial = (struct rt_serial_device *)dev;/* 强制转为串口设备 */
    /* 设备标志 ：中断发送 */
    if (dev->open_flag & RT_DEVICE_FLAG_INT_TX)
    {
        return _serial_int_tx(serial, (const rt_uint8_t *)buffer, size);
    }
    else/* 轮询发送 */
    {
        return _serial_poll_tx(serial, (const rt_uint8_t *)buffer, size);
    }
}
/* 串口框架控制
 * （1）设备
 * （2）命令
 * （3）参数
 * */
static rt_err_t rt_serial_control(struct rt_device *dev,
                                  int              cmd,
                                  void             *args)
{
    rt_err_t ret = RT_EOK;
    struct rt_serial_device *serial;/* */

    RT_ASSERT(dev != RT_NULL);
    serial = (struct rt_serial_device *)dev;/* 强制转为串口设备 */
    /* 判断命令 */
    switch (cmd)
    {
        /* 设备挂起 */
        case RT_DEVICE_CTRL_SUSPEND:
            /* suspend device */
            dev->flag |= RT_DEVICE_FLAG_SUSPENDED;
            break;
        /* 设备恢复 */
        case RT_DEVICE_CTRL_RESUME:
            /* resume device */
            dev->flag &= ~RT_DEVICE_FLAG_SUSPENDED;
            break;
        /* 设备配置 */
        case RT_DEVICE_CTRL_CONFIG:
            if (args)
            {
                struct serial_configure *pconfig = (struct serial_configure *) args;
                if (pconfig->bufsz != serial->config.bufsz && serial->parent.ref_count)
                {
                    /*can not change buffer size*/
                    return RT_EBUSY;
                }
                /* set serial configure */
                serial->config = *pconfig;
                if (serial->parent.ref_count)
                {
                    /* serial device has been opened, to configure it */
                    serial->ops->configure(serial, (struct serial_configure *) args);
                }
            }

            break;
        default :
            /* control device */
            ret = serial->ops->control(serial, cmd, args);
            break;
    }

    return ret;
}

/*
 *  串口框架注册
 *（1）串口设备
 *（2）设备名称
 *（3）设备标志
 *（4）用户数据
 */
rt_err_t rt_hw_serial_register(struct rt_serial_device *serial,
                               const char              *name,
                               rt_uint32_t              flag,
                               void                    *data)
{
    rt_err_t ret;
    struct rt_device *device;
    RT_ASSERT(serial != RT_NULL);

    device = &(serial->parent);/* 串口设备的father:设备  */

    device->type        = RT_Device_Class_Char;/* 设备类型 ：字符设备  */
    device->rx_indicate = RT_NULL;
    device->tx_complete = RT_NULL;

    device->init        = rt_serial_init;   /* 设备初始化方法 */
    device->open        = rt_serial_open;   /* 设备打开方法  */
    device->close       = rt_serial_close;  /* 设备关闭方法  */
    device->read        = rt_serial_read;   /* 设备读方法  */
    device->write       = rt_serial_write;  /* 设备写方法  */
    device->control     = rt_serial_control;/* 设备控制方法  */

    device->user_data   = data;/* 设备用户数据  */

    /* 注册设备到设备管理器 */
    ret = rt_device_register(device, name, flag);

    return ret;
}

/* 串口中断框架注册 */
void rt_hw_serial_isr(struct rt_serial_device *serial, int event)
{
    /* 中断判断 */
    switch (event & 0xff)
    {   /* 接收中断 */
        case RT_SERIAL_EVENT_RX_IND:
        {
            int ch = -1; /* 初始化接收数据  */
            rt_base_t level;
            struct rt_serial_rx_fifo* rx_fifo;

            /* 接收FIFO基地址  */
            rx_fifo = (struct rt_serial_rx_fifo*)serial->serial_rx;
            RT_ASSERT(rx_fifo != RT_NULL);

            while (1)
            {   /* 调用设备方法: 接收数据 */
                ch = serial->ops->getc(serial);
                if (ch == -1) break;


                /* 关全局中断 */
                level = rt_hw_interrupt_disable();
                /* 将接收到的数据存入buffer */
                rx_fifo->buffer[rx_fifo->put_index] = ch;
                /* 输出数据索引自增 */
                rx_fifo->put_index += 1;
                /* 接收的数据超出buffer空间 输出索引清零  */
                if (rx_fifo->put_index >= serial->config.bufsz) rx_fifo->put_index = 0;

                /* 输出字符索引等于输入字符索引  */
                if (rx_fifo->put_index == rx_fifo->get_index)
                {
                    /* 更新输入字符索引 */
                    rx_fifo->get_index += 1;
                    /* 接收数据已满 */
                    rx_fifo->is_full = RT_TRUE;
                    /* 输入索引超出buffer空间 输入索引清零 */
                    if (rx_fifo->get_index >= serial->config.bufsz) rx_fifo->get_index = 0;
                    /* 串口buffer检查 */
                    _serial_check_buffer_size();
                }

                /* 使能全局中断 */
                rt_hw_interrupt_enable(level);
            }

            /* 调用回调函数 */
            if (serial->parent.rx_indicate != RT_NULL)
            {
                rt_size_t rx_length;

                /* get rx length */
                level = rt_hw_interrupt_disable();
                rx_length = (rx_fifo->put_index >= rx_fifo->get_index)? (rx_fifo->put_index - rx_fifo->get_index):
                    (serial->config.bufsz - (rx_fifo->get_index - rx_fifo->put_index));
                rt_hw_interrupt_enable(level);

                if (rx_length)
                {
                    serial->parent.rx_indicate(&serial->parent, rx_length);
                }
            }
            break;
        }
        case RT_SERIAL_EVENT_TX_DONE:
        {
            struct rt_serial_tx_fifo* tx_fifo;

            tx_fifo = (struct rt_serial_tx_fifo*)serial->serial_tx;
            rt_completion_done(&(tx_fifo->completion));
            break;
        }
    }
}
