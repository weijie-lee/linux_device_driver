# DRM

## 代码知识点

DRM 设备从 `drm_dev_alloc()` 开始，准备 mode config 和最小的 KMS 对象后 `drm_dev_register()`。用户空间打开 `/dev/dri/cardN`。

本设备做最小注册，不设扫描输出时序。它和 `ch36_fb` 的 fbdev 是两套显示框架。

## 代码架构

```
drm_dev_alloc
drm_mode_config_init
drm_dev_register → /dev/dri/cardN
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch67
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch67_drm_virt_drm.ko
insmod ok
+ lsmod | grep virt_drm
virt_drm               16384  0 
drm                   622592  2 virt_drm
+ find /sys /dev /proc -name '*virt_drm*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_drm
/sys/bus/platform/devices/virt_drm
/sys/bus/platform/drivers/virt_drm
/sys/bus/platform/drivers/virt_drm/virt_drm
/sys/module/virt_drm
/sys/module/virt_drm/drivers/platform:virt_drm
/sys/module/drm/holders/virt_drm
+ rmmod virt_drm
rmmod ok
[PASS] ch67_drm
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch67_drm`
- 能找到 `virt_drm`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch67
```
