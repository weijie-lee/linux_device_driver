# IIO

## 代码知识点

IIO 用 `iio_dev` 描述一块传感器。通道是 `iio_chan_spec`，这里是一路索引为 0 的电压，带 `RAW` 和 `SCALE`。`read_raw` 在 `IIO_CHAN_INFO_RAW` 时返回 1234。

`devm_iio_device_register()` 之后，sysfs 在 `/sys/bus/iio/devices/iio:deviceN/`。直接模式 `INDIO_DIRECT_MODE` 表示读一次通道就返回，不用缓冲。

## 代码架构

```
iio_chan_spec
  type = IIO_VOLTAGE，channel = 0
read_raw
  RAW → 1234
  SCALE → 0 + 1000 微
devm_iio_device_alloc
devm_iio_device_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch51
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch51_iio_virt_iio.ko
insmod ok
+ lsmod | grep virt_iio
virt_iio               16384  0 
industrialio           98304  1 virt_iio
+ find /sys /dev /proc -name '*virt_iio*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_iio
/sys/bus/platform/devices/virt_iio
/sys/bus/platform/drivers/virt_iio
/sys/bus/platform/drivers/virt_iio/virt_iio
/sys/module/industrialio/holders/virt_iio
/sys/module/virt_iio
/sys/module/virt_iio/drivers/platform:virt_iio
+ cat /sys/bus/iio/devices/iio:device*/in_voltage0_raw
1234
+ rmmod virt_iio
rmmod ok
[PASS] ch51_iio
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch51_iio`
- `in_voltage0_raw` 是 `1234`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch51
```
