# crypto

## 代码知识点

对称算法用 `skcipher_alg` 描述，`crypto_register_skcipher()` 注册。算法名 `virt-null`，驱动名 `virt-null-generic`。`encrypt` 和 `decrypt` 都通过 `skcipher_walk` 把源拷到目的，所以它是恒等变换。

`/proc/crypto` 能看到名字和优先级。用户空间一般经 AF_ALG 或内核里的 `crypto_alloc_skcipher()` 使用它。

## 代码架构

```
skcipher_alg
  cra_name = virt-null
  setkey / encrypt / decrypt
crypto_register_skcipher
/proc/crypto
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch40
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch40_crypto_virt_crypto.ko
insmod ok
+ lsmod | grep virt_crypto
virt_crypto            16384  0 
+ find /sys /dev /proc -name '*virt_crypto*' 2>/dev/null | head -n 30
/sys/module/virt_crypto
+ grep virt-null /proc/crypto
name         : virt-null
driver       : virt-null-generic
+ rmmod virt_crypto
rmmod ok
[PASS] ch40_crypto
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch40_crypto`
- `/proc/crypto` 里有 `virt-null`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch40
```
