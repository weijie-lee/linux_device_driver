# PWM

## 代码知识点

PWM 控制器实现 `struct pwm_ops`，用 `pwmchip_add()` 注册。用户空间在 `/sys/class/pwm/pwmchipN/` 里 `export` 通道，再写 `period` 和 `duty_cycle`。

本芯片报两路通道。`apply` 把周期和占空比记在驱动里并打印。没有引脚波形。

## 代码架构

```
pwmchip_add
  npwm = 2
  ops: request / free / apply / get_state
/sys/class/pwm/pwmchipN/
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch11
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch11_pwm_pwm_demo.ko
insmod ok
+ ls /sys/class/pwm
pwmchip0
+ dmesg | grep virt_pwm
[    4.665998] virt_pwm virt_pwm: probed, 2 PWM channels registered
[    4.666036] virt_pwm virt_pwm: sysfs: /sys/class/pwm/pwmchipX/ created
[    4.666184] virt_pwm: module loaded
+ rmmod pwm_demo
rmmod ok
[PASS] ch11_pwm
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch11_pwm`
- `/sys/class/pwm` 里有 `pwmchip`，dmesg 有 `PWM channels`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch11
```
