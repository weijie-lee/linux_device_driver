# PCI 总线驱动 - 流程图

```mermaid
graph TD
    A["模块加载"] --> B["枚举 PCI 设备"]
    B --> C["探测设备"]
    C --> D["初始化 BAR"]
    D --> E["配置中断"]
    E --> F["启用 DMA"]
    F --> G["设备就绪"]
```
