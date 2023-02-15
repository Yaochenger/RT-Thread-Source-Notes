### SPI设备框架使用指南 

- 实例化一个SPI设备

  ```c
  struct rt_spi_device *spi_dev_w25q;
  ```

- 通过name查询SPI设备基地址

  ```c
  spi_dev_w25q = (struct rt_spi_device *)rt_device_find(name);
  ```

- 配置message

  ```c
       struct rt_spi_message msg1, msg2;
          msg1.send_buf   = &w25x_read_id;
          msg1.recv_buf   = RT_NULL;
          msg1.length     = 1;
          msg1.cs_take    = 1;
          msg1.cs_release = 0;
          msg1.next       = &msg2;
          msg2.send_buf   = RT_NULL;
          msg2.recv_buf   = id;
          msg2.length     = 5;
          msg2.cs_take    = 0;
          msg2.cs_release = 1;
          msg2.next       = RT_NULL;
  ```

- 发送

  ```c
  rt_spi_transfer_message(spi_dev_w25q, &msg1);
  ```

  

