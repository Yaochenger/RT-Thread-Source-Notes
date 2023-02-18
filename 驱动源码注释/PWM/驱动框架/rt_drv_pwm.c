/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-05-07     aozima       the first version
 * 2022-05-14     Stanley Lwin add pwm function
 * 2022-07-25     liYony       fix complementary outputs and add usage information in finsh
 * 2022-08-31     liYony       Add complementary output section to framework for management
 * 2022-09-24     qiyu         Add dead-time and phase configuration
 */

#include <rtdevice.h>
/* PWM控制 */
static rt_err_t _pwm_control(rt_device_t dev, int cmd, void *args)
{
    rt_err_t result = RT_EOK;
    /* 强制转为 PWM设备 */
    struct rt_device_pwm *pwm = (struct rt_device_pwm *)dev;
    /* PWM配置 */
    struct rt_pwm_configuration *configuration = (struct rt_pwm_configuration *)args;
    /* 判断命令 */
    switch (cmd)
    {
        /* */
        case PWMN_CMD_ENABLE:
            configuration->complementary = RT_TRUE;
            break;
        case PWMN_CMD_DISABLE:
            configuration->complementary = RT_FALSE;
            break;
        default:
            if(pwm->ops->control)
                result = pwm->ops->control(pwm, cmd, args);
            break;
    }

    return result;
}


/*
pos: channel
void *buffer: rt_uint32_t pulse[size]
size : number of pulse, only set to sizeof(rt_uint32_t).
*/
/* */
/* PWM设备框架读 API */
static rt_size_t _pwm_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_err_t result = RT_EOK;
    /* 强制设备类型转换 */
    struct rt_device_pwm *pwm = (struct rt_device_pwm *)dev;
    /* 获取数据保存地址 */
    rt_uint32_t *pulse = (rt_uint32_t *)buffer;
    /* 初始化配置 */
    struct rt_pwm_configuration configuration = {0};
    /* 设置PWM通道 */
    configuration.channel = (pos > 0) ? (pos) : (-pos);
    /* 判断控制函数是否被注册 */
    if (pwm->ops->control)
    {
        /* 执行脉冲带宽获取 */
        result = pwm->ops->control(pwm, PWM_CMD_GET,  &configuration);
        if (result != RT_EOK)
        {
            return 0;
        }
        /* 将读取到的脉冲带宽保存至buffer */
        *pulse = configuration.pulse;
    }

    return size;
}

/*
pos: channel
void *buffer: rt_uint32_t pulse[size]
size : number of pulse, only set to sizeof(rt_uint32_t).
*/
/* */
/* PWM设备框架写 API */
static rt_size_t _pwm_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_err_t result = RT_EOK;
    /* 强制类型转换 */
    struct rt_device_pwm *pwm = (struct rt_device_pwm *)dev;
    /* 获取buffer的地址 */
    rt_uint32_t *pulse = (rt_uint32_t *)buffer;
    /* 初始化PWM设备配置 */
    struct rt_pwm_configuration configuration = {0};
    /* 设置PWM通道 */
    configuration.channel = (pos > 0) ? (pos) : (-pos);
    /* 判断控制函数是否被注册 */
    if (pwm->ops->control)
    {
        /* 读取设备的脉冲宽度 */
        result = pwm->ops->control(pwm, PWM_CMD_GET, &configuration);
        if (result != RT_EOK)
        {
            return 0;
        }
        /* 保存设备的脉冲宽度 */
        configuration.pulse = *pulse;
        /* 设置设备的脉冲宽度 */
        result = pwm->ops->control(pwm, PWM_CMD_SET, &configuration);
        if (result != RT_EOK)
        {
            return 0;
        }
    }

    return size;
}
/*  */
/*  PWM设备注册 */
rt_err_t rt_device_pwm_register(struct rt_device_pwm *device, const char *name, const struct rt_pwm_ops *ops, const void *user_data)
{
    rt_err_t result = RT_EOK;
    /*  初始化申请的空间 */
    rt_memset(device, 0, sizeof(struct rt_device_pwm));
    /*  将PWM设备注册到设备框架 */
    device->parent.init = RT_NULL;
    device->parent.open = RT_NULL;
    device->parent.close = RT_NULL;
    device->parent.read  = _pwm_read;       /* PWM设备读API */
    device->parent.write = _pwm_write;      /* PWM设备写API */
    device->parent.control = _pwm_control;  /* PWM设备控制API */
    /*  */
    device->parent.type         = RT_Device_Class_PWM; /* PWM设备 */
    device->ops                 = ops;                 /* PWM设备方法 */
    device->parent.user_data    = (void *)user_data;   /* userdata PWM设备未用 */
    /*  正式开始将PWM设备注册到设备框架 */
    result = rt_device_register(&device->parent, name, RT_DEVICE_FLAG_RDWR);

    return result;
}
/*  */
/*  PWM设备使能 */
rt_err_t rt_pwm_enable(struct rt_device_pwm *device, int channel)
{
    rt_err_t result = RT_EOK;
    /*  设备配置 */
    struct rt_pwm_configuration configuration = {0};

    if (!device)
    {
        return -RT_EIO;
    }

    /* Make it is positive num forever */
    /*  设置输出通道 */
    configuration.channel = (channel > 0) ? (channel) : (-channel);

    /* If channel is a positive number (0 ~ n), it means using normal output pin.
     * If channel is a negative number (0 ~ -n), it means using complementary output pin. */
    /*  通道使能
     * - channnel < 0
          * 使能互补通道  使能通道
     * - channel > 0
          * 关闭互补通道 使能通道
     */
    if(channel > 0)
    {
        result = rt_device_control(&device->parent, PWMN_CMD_DISABLE, &configuration);
    }
    else
    {
        result = rt_device_control(&device->parent, PWMN_CMD_ENABLE, &configuration);
    }

    result = rt_device_control(&device->parent, PWM_CMD_ENABLE, &configuration);

    return result;
}
/*  PWM失能 */
rt_err_t rt_pwm_disable(struct rt_device_pwm *device, int channel)
{
    rt_err_t result = RT_EOK;
    /*  设备配置 */
    struct rt_pwm_configuration configuration = {0};

    if (!device)
    {
        return -RT_EIO;
    }

    /* Make it is positive num forever */
    /*  设置输出通道 */
    configuration.channel = (channel > 0) ? (channel) : (-channel);

    /* If channel is a positive number (0 ~ n), it means using normal output pin.
     * If channel is a negative number (0 ~ -n), it means using complementary output pin. */
    /*  通道失能
     * - channnel < 0
          * 使能互补通道  关闭通道
     * - channel > 0
          * 关闭互补通道 关闭通道
     */
    if(channel > 0)
    {
        result = rt_device_control(&device->parent, PWMN_CMD_DISABLE, &configuration);
    }
    else
    {
        result = rt_device_control(&device->parent, PWMN_CMD_ENABLE, &configuration);
    }

    result = rt_device_control(&device->parent, PWM_CMD_DISABLE, &configuration);

    return result;
}
/*  PWM设备设置 */
rt_err_t rt_pwm_set(struct rt_device_pwm *device, int channel, rt_uint32_t period, rt_uint32_t pulse)
{
    rt_err_t result = RT_EOK;
    struct rt_pwm_configuration configuration = {0};

    if (!device)
    {
        return -RT_EIO;
    }
    /* 通道 */
    configuration.channel = (channel > 0) ? (channel) : (-channel);
    /* 周期 单位ns */
    configuration.period = period;
    /* 脉冲宽度 单位ns */
    configuration.pulse = pulse;
    /* 参数设置 */
    result = rt_device_control(&device->parent, PWM_CMD_SET, &configuration);

    return result;
}
/*  */
/*  设置PWM周期 */
rt_err_t rt_pwm_set_period(struct rt_device_pwm *device, int channel, rt_uint32_t period)
{
    rt_err_t result = RT_EOK;
    struct rt_pwm_configuration configuration = {0};

    if (!device)
    {
        return -RT_EIO;
    }
    /*  */
    configuration.channel = (channel > 0) ? (channel) : (-channel);
    configuration.period = period;
    result = rt_device_control(&device->parent, PWM_CMD_SET_PERIOD, &configuration);

    return result;
}
/*  设置PWM脉冲宽度 */
rt_err_t rt_pwm_set_pulse(struct rt_device_pwm *device, int channel, rt_uint32_t pulse)
{
    rt_err_t result = RT_EOK;
    struct rt_pwm_configuration configuration = {0};

    if (!device)
    {
        return -RT_EIO;
    }

    configuration.channel = (channel > 0) ? (channel) : (-channel);
    configuration.pulse = pulse;
    result = rt_device_control(&device->parent, PWM_CMD_SET_PULSE, &configuration);

    return result;
}

/*  设置死区时间 */
rt_err_t rt_pwm_set_dead_time(struct rt_device_pwm *device, int channel, rt_uint32_t dead_time)
{
    rt_err_t result = RT_EOK;
    struct rt_pwm_configuration configuration = {0};

    if (!device)
    {
        return -RT_EIO;
    }

    configuration.channel = (channel > 0) ? (channel) : (-channel);
    configuration.dead_time = dead_time;
    result = rt_device_control(&device->parent, PWM_CMD_SET_DEAD_TIME, &configuration);

    return result;
}

/* 设置相位 */
rt_err_t rt_pwm_set_phase(struct rt_device_pwm *device, int channel, rt_uint32_t phase)
{
    rt_err_t result = RT_EOK;
    struct rt_pwm_configuration configuration = {0};

    if (!device)
    {
        return -RT_EIO;
    }

    configuration.channel = (channel > 0) ? (channel) : (-channel);
    configuration.phase = phase;
    result = rt_device_control(&device->parent, PWM_CMD_SET_PHASE, &configuration);

    return result;
}
/*  获取PWM配置  */
rt_err_t rt_pwm_get(struct rt_device_pwm *device, struct rt_pwm_configuration *cfg)
{
    rt_err_t result = RT_EOK;

    if (!device)
    {
        return -RT_EIO;
    }
    /*  获取设备参数 */
    result = rt_device_control(&device->parent, PWM_CMD_GET, cfg);

    return result;
}

#ifdef RT_USING_FINSH
#include <stdlib.h>
#include <string.h>
#include <finsh.h>

static int pwm(int argc, char **argv)
{
    rt_err_t result = -RT_ERROR;
    char *result_str;
    static struct rt_device_pwm *pwm_device = RT_NULL;
    struct rt_pwm_configuration cfg = {0};

    if(argc > 1)
    {
        if(!strcmp(argv[1], "probe"))
        {
            if(argc == 3)
            {
                pwm_device = (struct rt_device_pwm *)rt_device_find(argv[2]);
                result_str = (pwm_device == RT_NULL) ? "failure" : "success";
                rt_kprintf("probe %s %s\n", argv[2], result_str);
            }
            else
            {
                rt_kprintf("pwm probe <device name>                  - probe pwm by name\n");
            }
        }
        else
        {
            if(pwm_device == RT_NULL)
            {
                rt_kprintf("Please using 'pwm probe <device name>' first.\n");
                return -RT_ERROR;
            }
            if(!strcmp(argv[1], "enable"))
            {
                if(argc == 3)
                {
                    result = rt_pwm_enable(pwm_device, atoi(argv[2]));
                    result_str = (result == RT_EOK) ? "success" : "failure";
                    rt_kprintf("%s channel %d is enabled %s \n", pwm_device->parent.parent.name, atoi(argv[2]), result_str);
                }
                else
                {
                    rt_kprintf("pwm enable <channel>                     - enable pwm channel\n");
                    rt_kprintf("    e.g. MSH >pwm enable  1              - PWM_CH1  nomal\n");
                    rt_kprintf("    e.g. MSH >pwm enable -1              - PWM_CH1N complememtary\n");
                }
            }
            else if(!strcmp(argv[1], "disable"))
            {
                if(argc == 3)
                {
                    result = rt_pwm_disable(pwm_device, atoi(argv[2]));
                }
                else
                {
                    rt_kprintf("pwm disable <channel>                    - disable pwm channel\n");
                }
            }
            else if(!strcmp(argv[1], "get"))
            {
                cfg.channel = atoi(argv[2]);
                result = rt_pwm_get(pwm_device, &cfg);
                if(result == RT_EOK)
                {
                    rt_kprintf("Info of device [%s] channel [%d]:\n",pwm_device, atoi(argv[2]));
                    rt_kprintf("period      : %d\n", cfg.period);
                    rt_kprintf("pulse       : %d\n", cfg.pulse);
                    rt_kprintf("Duty cycle  : %d%%\n",(int)(((double)(cfg.pulse)/(cfg.period)) * 100));
                }
                else
                {
                    rt_kprintf("Get info of device: [%s] error.\n", pwm_device);
                }
            }
            else if (!strcmp(argv[1], "set"))
            {
                if(argc == 5)
                {
                    result = rt_pwm_set(pwm_device, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
                    rt_kprintf("pwm info set on %s at channel %d\n",pwm_device,(rt_base_t)atoi(argv[2]));
                }
                else
                {
                    rt_kprintf("Set info of device: [%s] error\n", pwm_device);
                    rt_kprintf("Usage: pwm set <channel> <period> <pulse>\n");
                }
            }
            else if(!strcmp(argv[1], "phase"))
            {
                if(argc == 4)
                {
                    result = rt_pwm_set_phase(pwm_device, atoi(argv[2]),atoi(argv[3]));
                    result_str = (result == RT_EOK) ? "success" : "failure";
                    rt_kprintf("%s phase is set %d \n", pwm_device->parent.parent.name, (rt_base_t)atoi(argv[3]));
                }
            }
            else if(!strcmp(argv[1], "dead_time"))
            {
                if(argc == 4)
                {
                    result = rt_pwm_set_dead_time(pwm_device, atoi(argv[2]),atoi(argv[3]));
                    result_str = (result == RT_EOK) ? "success" : "failure";
                    rt_kprintf("%s dead_time is set %d \n", pwm_device->parent.parent.name, (rt_base_t)atoi(argv[3]));
                }
            }
            else
            {
                rt_kprintf("Usage: \n");
                rt_kprintf("pwm probe      <device name>                - probe pwm by name\n");
                rt_kprintf("pwm enable     <channel>                    - enable pwm channel\n");
                rt_kprintf("pwm disable    <channel>                    - disable pwm channel\n");
                rt_kprintf("pwm get        <channel>                    - get pwm channel info\n");
                rt_kprintf("pwm set        <channel> <period> <pulse>   - set pwm channel info\n");
                rt_kprintf("pwm phase      <channel> <phase>            - set pwm phase\n");
                rt_kprintf("pwm dead_time  <channel> <dead_time>        - set pwm dead time\n");
                result = - RT_ERROR;
            }
        }
    }
    else
    {
        rt_kprintf("Usage: \n");
        rt_kprintf("pwm probe      <device name>               - probe pwm by name\n");
        rt_kprintf("pwm enable     <channel>                   - enable pwm channel\n");
        rt_kprintf("pwm disable    <channel>                   - disable pwm channel\n");
        rt_kprintf("pwm get        <channel>                   - get pwm channel info\n");
        rt_kprintf("pwm set        <channel> <period> <pulse>  - set pwm channel info\n");
        rt_kprintf("pwm phase      <channel> <phase>           - set pwm phase\n");
        rt_kprintf("pwm dead_time  <channel> <dead_time>       - set pwm dead time\n");
        result = - RT_ERROR;
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(pwm, pwm [option]);

#endif /* RT_USING_FINSH */
