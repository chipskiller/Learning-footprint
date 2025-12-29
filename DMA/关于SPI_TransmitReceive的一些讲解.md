这是一个非常好的问题！这涉及到SPI接口的工作机制和HAL库的设计逻辑。让我详细解释：

## 根本原因：SPI是全双工同步通信

**SPI通信的本质是时钟驱动下的双向同步传输**：
- 主设备发送1位的同时，也会收到1位
- 发送和接收必须同时进行，不能只发不收或只收不发
- 即使你只想读取数据，也必须发送"虚拟数据"来产生时钟信号

## HAL库参数设计的灵活性

HAL库的`HAL_SPI_TransmitReceive`函数被设计为既能处理简单变量，也能处理数组：

### 场景1：单独变量（第一次调用）
```c
HAL_SPI_TransmitReceive(&MPU_HSPI, &tx, &rx, 1, 1);
```
- **`&tx`**：指向单个字节变量的指针
- **`&rx`**：指向单个字节变量的指针
- 函数内部会把这个指针当作1字节数组来处理
- 这种写法在只需要传输1字节时更简洁

### 场景2：数组（第二次调用）
```c
HAL_SPI_TransmitReceive(&MPU_HSPI, tx_buff, pData, len, 1);
```
- **`tx_buff`**：数组名本身就是指针（指向数组首元素）
- **`pData`**：指向接收缓冲区的指针
- 适合传输多个字节的批量操作

## 为什么你的代码中两者混用？

### 第一次调用（单字节）：
```c
// 目的：发送读命令，同时丢弃接收到的第一个字节
HAL_SPI_TransmitReceive(&MPU_HSPI, &tx, &rx, 1, 1);
```
- 发送：读命令（寄存器地址 | 0x80）
- 接收：MPU返回的第一个字节（通常是虚拟数据或无意义数据）
- 这个接收到的`rx`被丢弃了，但**必须接收**以完成SPI时钟周期

### 第二次调用（多字节）：
```c
// 目的：发送虚拟时钟，接收实际数据
HAL_SPI_TransmitReceive(&MPU_HSPI, tx_buff, pData, len, 1);
```
- 发送：`len`个字节的虚拟数据（产生时钟）
- 接收：`len`个字节的实际传感器数据到`pData`缓冲区

## 内存布局对比

```
// 单变量方式（第一次调用）
          发送缓冲区         接收缓冲区
地址：    [tx变量地址]  ->  [rx变量地址]
数据：    0x80|regAddr      丢弃的数据

// 数组方式（第二次调用）
          发送缓冲区         接收缓冲区
地址：    tx_buff[0]  ->  pData[0]
          tx_buff[1]  ->  pData[1]
          ...              ...
          tx_buff[n]  ->  pData[n]
数据：    虚拟数据（如0x00）  实际传感器数据
```

## 代码优化的建议

实际上，你这段代码可以更简洁，因为第一次调用的`rx`没有被使用：

```c
// 更高效的写法：合并为一次调用
uint8_t mpu_read_bytes(uint8_t regAddr, uint8_t* pData, uint8_t len)
{
    uint8_t tx_buffer[len + 1];
    uint8_t rx_buffer[len + 1];
    
    // 第一个字节：读命令
    tx_buffer[0] = regAddr | 0x80;
    
    // 后续字节：虚拟数据（产生时钟）
    for(int i = 1; i <= len; i++) {
        tx_buffer[i] = 0x00;  // 任意值，仅用于产生时钟
    }
    
    MPU_NSS_LOW;
    HAL_SPI_TransmitReceive(&MPU_HSPI, tx_buffer, rx_buffer, len + 1, HAL_MAX_DELAY);
    MPU_NSS_HIGH;
    
    // 复制有效数据（跳过第一个字节）
    for(int i = 0; i < len; i++) {
        pData[i] = rx_buffer[i + 1];
    }
    
    return 0;
}
```

## 总结

| 特性 | 单变量方式 | 数组方式 |
|------|-----------|----------|
| 适用场景 | 单字节传输 | 多字节传输 |
| 内存占用 | 小 | 大 |
| 代码简洁性 | 高 | 中 |
| 灵活性 | 低 | 高 |

HAL库通过**指针参数**的设计，统一了这两种使用方式，让开发者可以根据实际情况选择最合适的传输方式。