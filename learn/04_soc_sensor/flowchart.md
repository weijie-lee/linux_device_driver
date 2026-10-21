# 写温度

```mermaid
graph TD
    a["store temperature"] --> b["regmap_write TEMP"]
    b --> c["irq_work 硬中断"]
    c --> d["threaded handler regmap_read"]
    d --> e["last_temp 与 irq_count"]
```
