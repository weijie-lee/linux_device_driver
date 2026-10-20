# 引脚控制器 加载流程

1. 创建名为 `virt_pinctrl` 的 platform 设备。
2. 注册 platform 驱动，probe 完成子系统登记。
3. 若 probe 没有绑定设备，insmod 返回错误。
4. 卸载时先注销驱动，再销毁 platform 设备。
