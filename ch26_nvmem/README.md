# NVMEM

## 代码知识点

NVMEM 是给 EEPROM、eFuse 这类存储用的。提供者填 `nvmem_config`：`size`、`reg_read`、`reg_write`，再 `devm_nvmem_register()`。消费者用 `nvmem_cell_read`。

本设备 256 字节，字长 1。读写就是 `memcpy` 到驱动里的数组。

## 代码架构

```
nvmem_config
  name = virt-nvmem
  size = 256
  reg_read / reg_write
devm_nvmem_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch26
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch26_nvmem_virt_nvmem.ko
insmod ok
+ lsmod | grep virt_nvmem
virt_nvmem             16384  0 
+ find /sys /dev /proc -name '*virt-nvmem*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_nvmem/virt-nvmem
/sys/bus/nvmem/devices/virt-nvmem
+ rmmod virt_nvmem
rmmod ok
[PASS] ch26_nvmem
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch26_nvmem`
- 能找到名字 `virt-nvmem`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch26
```
