# PWM 控制器 - 流程图

```mermaid
graph TD
    A["模块加载"] --> B["初始化 PWM"]
    B --> C["设置周期"]
    C --> D["设置占空比"]
    D --> E["启用 PWM"]
    E --> F["输出 PWM 波形"]
    F --> G["用户调整参数"]
    G --> D
```
