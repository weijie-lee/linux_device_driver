# probe 里的顺序

```mermaid
graph TD
    a["dma_set_mask_and_coherent"] --> b["coherent 分配、写入、释放"]
    b --> c["map_single、sync、unmap"]
    c --> d["map_sg、unmap"]
    d --> e["写 sysfs report"]
```
