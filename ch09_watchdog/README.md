# 看门狗

## 代码知识点

看门狗框架是 `struct watchdog_device` 加 `watchdog_ops`。`start` 开始倒计时，`ping` 喂狗，`set_timeout` 改超时，`stop` 停止。`watchdog_register_device()` 创建 `/dev/watchdogN`。

打开这个节点就会 start。本客户机里如果只打开、不喂狗，超时后驱动会走复位路径。实验只确认节点和注册日志，不 `open`。

## 代码架构

```
watchdog_register_device
  ops: start / stop / ping / set_timeout
  /dev/watchdog0
rmmod
  watchdog_unregister_device
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch09
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch09_watchdog_watchdog_demo.ko
insmod ok
+ ls /dev/watchdog /dev/watchdog0
/dev/watchdog
/dev/watchdog0
+ dmesg | grep virt_wdt
[    4.526897] virt_wdt: registered, timeout=30 seconds, nowayout=0
+ rmmod watchdog_demo
rmmod ok
[PASS] ch09_watchdog
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch09_watchdog`
- `/dev/watchdog` 存在，dmesg 有 `registered`。不要在这个实验里打开这个设备节点。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch09
```
