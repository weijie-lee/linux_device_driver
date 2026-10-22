# framebuffer

## 代码知识点

fbdev 分配 `fb_info`，填 `fb_var_screeninfo` 和 `fb_fix_screeninfo`，`register_framebuffer()` 得到 `/dev/fbN`。显存是 `vzalloc` 的一块 320×240、16 位的缓冲。

`fb_fillrect` 用 `cfb_fillrect`。它会读 `pseudo_palette`。这个指针必须指向可写的调色板，不能是 NULL。`fb_check_var` 只接受这一种分辨率。

## 代码架构

```
framebuffer_alloc
screen_base = vzalloc(320*240*2)
fix.id = virtfb
pseudo_palette = 16 色
register_framebuffer → /dev/fbN
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch36
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch36_fb_virt_fb.ko
insmod ok
+ cat /sys/class/graphics/fb*/name
virtfb
+ echo 0 > /sys/class/vtconsole/vtcon1/bind
+ echo 0 > /sys/class/vtconsole/vtcon0/bind
+ rmmod virt_fb
rmmod ok
[PASS] ch36_fb
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch36_fb`
- `cat` 读到 `virtfb`。fbcon 占着模块时先把 `vtcon1/bind` 和 `vtcon0/bind` 写成 0，再 `rmmod virt_fb` 得到 `rmmod ok`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch36
```
