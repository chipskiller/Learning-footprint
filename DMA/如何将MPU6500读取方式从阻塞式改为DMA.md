好的，我将非常详细地解释如何将阻塞的SPI传输改为DMA传输。这个过程可以分解为几个关键步骤，我会逐一解释。

**核心思想：**

*   **阻塞传输 (`HAL_SPI_TransmitReceive`)**：CPU会一直等待SPI传输完成，期间不能做其他事情，效率低。
*   **DMA传输 (`HAL_SPI_TransmitReceive_DMA`)**：CPU告诉DMA硬件要传输什么数据、传输多少，然后DMA硬件自己去完成传输。CPU可以在这段时间内去做别的事情，传输完成后DMA会通知CPU（通常是通过中断）。这样CPU的效率就高了。

**需要做的改动：**

1.  **在CubeMX中配置DMA硬件。**
2.  **修改C代码，使用DMA函数并处理传输完成的通知。**

---

### **第一步：CubeMX 配置 DMA**

这是在图形化界面中告诉STM32的硬件，为SPI5外设分配DMA通道。

1.  **打开你的CubeMX项目**：确保你的SPI5已经配置好了（比如时钟、模式、引脚等）。
2.  **找到SPI5配置**：在左侧的外设列表或原理图中找到 `SPI5`。
3.  **进入SPI5设置**：点击 `SPI5`，进入其配置页面。
4.  **找到 "DMA Settings" 选项卡**：在SPI5的配置窗口中，切换到 "DMA Settings" 标签页。
5.  **添加DMA请求**：
    *   点击 "Add" 按钮来添加一个DMA请求。
    *   **配置TX (发送) DMA：**
        *   **Request (请求):** 选择 `SPI5_TX` (表示SPI5的发送请求)。
        *   **DMA (DMA控制器):** 通常选择 `DMA2` (STM32F4有DMA1和DMA2两个控制器)。
        *   **Channel (通道):** 选择一个空闲的通道，比如 `Channel 2`。注意不要与其他外设的DMA通道冲突。
        *   **Direction (方向):** 选择 `Memory to Peripheral` (数据从内存发送到SPI外设)。
        *   **Mode (模式):** 选择 `Normal` (一次传输完成后停止)。
        *   **Priority (优先级):** 选择 `High` (High, Medium, Low, Very High)。根据你的应用需求设置。
    *   **再次点击 "Add" 按钮添加另一个DMA请求。**
    *   **配置RX (接收) DMA：**
        *   **Request (请求):** 选择 `SPI5_RX` (表示SPI5的接收请求)。
        *   **DMA (DMA控制器):** 通常也选择 `DMA2`。
        *   **Channel (通道):** 选择一个与TX不同的空闲通道，比如 `Channel 3`。
        *   **Direction (方向):** 选择 `Peripheral to Memory` (数据从SPI外设接收进内存)。
        *   **Mode (模式):** 选择 `Normal`。
        *   **Priority (优先级):** 选择 `High`。
6.  **生成代码**：完成DMA配置后，点击 "Project" -> "Generate Code"。CubeMX会自动生成DMA的初始化代码。

**CubeMX做了什么？**

CubeMX生成的代码（主要在 `spi.c` 和 `main.c` 中）会：

*   初始化DMA控制器和通道，设置好源地址（内存）、目标地址（SPI数据寄存器）、传输方向、数据长度等。
*   将DMA通道与SPI5的TX和RX请求关联起来。
*   在 `HAL_SPI_MspInit` 函数中启用DMA时钟，并将DMA中断与CPU的NVIC（嵌套向量中断控制器）关联。

---

### **第二步：修改C代码**

现在需要修改 `bsp_imu.c` 文件，让代码使用DMA函数，并处理DMA传输完成的信号。

#### **1. 添加全局变量和头文件**

在 `bsp_imu.c` 文件的开头，需要添加一些变量来帮助DMA传输和同步。

```c
#include "bsp_imu.h"
#include "ist8310_reg.h"
#include "stm32f4xx_hal.h"
#include <math.h>
#include "mpu6500_reg.h"
#include "spi.h"
// --- 添加 DMA 相关头文件 ---
#include "stm32f4xx_hal_dma.h" // 包含DMA相关的HAL库函数

// ... (你原来的 #define 定义) ...

// --- 新增的全局变量 ---
// DMA传输完成标志：当DMA完成一次传输时，中断服务程序会将此变量设为1
volatile uint8_t dma_transfer_complete = 0; // <--- 新增

// 临时存储单次传输的发送和接收字节
static uint8_t current_tx_byte = 0; // <--- 新增
static uint8_t current_rx_byte = 0; // <--- 新增

// 临时存储批量传输的发送和接收缓冲区指针及长度
static uint8_t* current_tx_buffer = NULL; // <--- 新增
static uint8_t* current_rx_buffer = NULL; // <--- 新增
static uint8_t current_dma_length = 0;    // <--- 新增

// ... (你原来的其他全局变量定义) ...
```

*   `#include "stm32f4xx_hal_dma.h"`：引入DMA库的函数。
*   `volatile uint8_t dma_transfer_complete`：这是一个标志。`volatile` 关键字告诉编译器，这个变量的值可能会在程序控制之外被改变（比如在中断里），所以每次使用时都要从内存里重新读取。当DMA传输完成时，中断服务程序会设置这个标志为1。
*   `current_tx_byte`, `current_rx_byte`：因为DMA需要一个内存地址来读取/写入数据，对于单字节传输，我们用这两个变量作为临时存储。
*   `current_tx_buffer`, `current_rx_buffer`, `current_dma_length`：对于多字节传输，我们需要知道缓冲区的地址和长度。

#### **2. 修改传输函数 (`mpu_write_byte`, `mpu_read_byte`, `mpu_read_bytes`)**

这三个函数原来使用 `HAL_SPI_TransmitReceive`，现在要改为使用 `HAL_SPI_TransmitReceive_DMA`。

**`mpu_write_byte` 修改示例：**

```c
uint8_t mpu_write_byte(uint8_t const reg, uint8_t const data)
{
    // --- DMA 修改步骤 ---
    // 1. 清除传输完成标志
    dma_transfer_complete = 0;

    // 2. 拉低片选信号 (开始SPI通信)
    MPU_NSS_LOW;

    // 3. 准备第一次传输：发送寄存器地址
    current_tx_byte = reg & 0x7F; // 准备发送的地址字节 (写操作，最低位为0)
    current_rx_byte = 0xFF;       // 接收字节无意义，填0xFF

    // 4. 启动DMA传输 (发送地址)
    if (HAL_SPI_TransmitReceive_DMA(&MPU_HSPI, &current_tx_byte, &current_rx_byte, 1) != HAL_OK)
    {
        // 如果启动失败，拉高片选并返回错误
        MPU_NSS_HIGH;
        return 1; // 传输失败
    }

    // 5. 等待第一次传输完成 (轮询标志)
    // CPU在这里等待，但可以进入低功耗模式 (WFI) 提高效率
    while (!dma_transfer_complete) { __WFI(); }

    // 6. 准备第二次传输：发送数据
    current_tx_byte = data;       // 准备发送的数据字节
    current_rx_byte = 0xFF;       // 接收字节无意义

    // 7. 清除标志，准备下一次传输
    dma_transfer_complete = 0;

    // 8. 启动DMA传输 (发送数据)
    if (HAL_SPI_TransmitReceive_DMA(&MPU_HSPI, &current_tx_byte, &current_rx_byte, 1) != HAL_OK)
    {
        MPU_NSS_HIGH;
        return 1; // 传输失败
    }

    // 9. 等待第二次传输完成
    while (!dma_transfer_complete) { __WFI(); }

    // 10. 拉高片选信号 (结束SPI通信)
    MPU_NSS_HIGH;

    return 0; // 传输成功
}
```

**`mpu_read_byte` 修改示例：**

```c
uint8_t mpu_read_byte(uint8_t const reg)
{
    uint8_t rx_data = 0; // 用于存储读取到的值

    // --- DMA 修改步骤 ---
    dma_transfer_complete = 0;
    MPU_NSS_LOW;

    // 1. 发送读取地址
    current_tx_byte = reg | 0x80; // 准备发送的地址字节 (读操作，最低位为1)
    current_rx_byte = 0xFF;       // 接收字节无意义

    if (HAL_SPI_TransmitReceive_DMA(&MPU_HSPI, &current_tx_byte, &current_rx_byte, 1) != HAL_OK)
    {
        MPU_NSS_HIGH;
        return 0xFF; // 传输失败，返回无效值
    }
    while (!dma_transfer_complete) { __WFI(); }

    // 2. 发送空字节以接收数据
    current_tx_byte = 0xFF; // 发送空字节
    current_rx_byte = 0xFF; // 准备接收

    dma_transfer_complete = 0;

    if (HAL_SPI_TransmitReceive_DMA(&MPU_HSPI, &current_tx_byte, &current_rx_byte, 1) != HAL_OK)
    {
        MPU_NSS_HIGH;
        return 0xFF; // 传输失败，返回无效值
    }
    while (!dma_transfer_complete) { __WFI(); }

    rx_data = current_rx_byte; // 获取接收到的数据

    MPU_NSS_HIGH;
    return rx_data;
}
```

**`mpu_read_bytes` 修改示例：**

```c
uint8_t mpu_read_bytes(uint8_t const regAddr, uint8_t* pData, uint8_t len)
{
    if (len == 0) return 0; // 如果长度为0，直接返回成功

    dma_transfer_complete = 0;
    MPU_NSS_LOW;

    // 1. 发送读取地址
    current_tx_byte = regAddr | 0x80; // 准备发送的地址字节
    current_rx_byte = 0xFF;

    if (HAL_SPI_TransmitReceive_DMA(&MPU_HSPI, &current_tx_byte, &current_rx_byte, 1) != HAL_OK)
    {
        MPU_NSS_HIGH;
        return 1; // 传输失败
    }
    while (!dma_transfer_complete) { __WFI(); }

    // 2. 准备接收多字节数据
    // 将要发送的缓冲区（全填0xFF用于接收）和接收缓冲区指针保存到全局变量
    current_tx_buffer = tx_buff; // 使用你已定义的全局数组 tx_buff[14]
    current_rx_buffer = pData;   // 用户传入的接收缓冲区
    current_dma_length = len;    // 要接收的字节数

    // 填充发送缓冲区，通常发送空字节 (0xFF) 来接收数据
    for(int i = 0; i < len; i++) {
        tx_buff[i] = 0xFF; // 填充发送缓冲区
    }

    dma_transfer_complete = 0;

    // 3. 启动DMA传输接收数据
    // 注意：这里使用的是全局变量的指针
    if (HAL_SPI_TransmitReceive_DMA(&MPU_HSPI, (uint8_t*)current_tx_buffer, (uint8_t*)current_rx_buffer, current_dma_length) != HAL_OK)
    {
        MPU_NSS_HIGH;
        return 1; // 传输失败
    }
    while (!dma_transfer_complete) { __WFI(); } // 等待传输完成

    MPU_NSS_HIGH;
    return 0; // 传输成功
}
```

**关键点解释：**

*   **`HAL_SPI_TransmitReceive_DMA`**: 这个函数告诉DMA开始传输。它**立即返回**，不会等待传输完成。如果返回 `HAL_OK`，说明DMA任务已成功启动。
*   **`while (!dma_transfer_complete) { __WFI(); }`**: 这是等待DMA完成的方式。`__WFI()` 指令让CPU进入等待中断模式，降低功耗，直到DMA完成传输并触发中断。
*   **`dma_transfer_complete`**: 这个标志必须在启动DMA**之前**清零，然后在DMA完成**之后**由中断服务程序置1。

#### **3. 编写DMA中断服务程序 (ISR)**

这是最关键的部分。当DMA完成传输后，它需要通知CPU。这个通知是通过**中断**实现的。

你**必须**在 `stm32f4xx_it.c` 文件中找到或编写正确的DMA中断处理函数。

1.  **打开 `stm32f4xx_it.c`**：这个文件通常包含所有外设的中断处理函数。
2.  **找到对应DMA通道的中断函数**：在CubeMX中，你为SPI5的RX分配了 `DMA2` 的 `Channel 3`。那么对应的中断函数通常是 `DMA2_Stream3_IRQHandler` (对于STM32F4系列，DMA通道对应Stream)。
3.  **编写中断处理逻辑**：

```c
// --- 在 stm32f4xx_it.c 文件中 ---

// ... (其他中断处理函数) ...

/**
  * @brief This function handles DMA2 Stream 3 interrupt request for SPI5_RX.
  *        你需要根据CubeMX的配置找到正确的中断函数名。
  *        例如，如果你的RX Channel是Stream 3，函数名就是 DMA2_Stream3_IRQHandler。
  */
void DMA2_Stream3_IRQHandler(void) // <--- 确保这是你为SPI5_RX分配的Channel对应的中断
{
  /* USER CODE BEGIN DMA2_Stream3_IRQn 0 */

  // --- 关键：检查是否是SPI5_RX的DMA传输完成中断 ---
  // 1. 检查传输完成 (Transfer Complete) 标志是否被设置
  // 2. 检查传输完成中断是否被使能 (IT Source)
  // hdma_spi5_rx 是CubeMX生成的DMA句柄变量名，请确保与spi.c中定义的一致
  if (__HAL_DMA_GET_FLAG(&hdma_spi5_rx, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_spi5_rx)) && 
      __HAL_DMA_GET_IT_SOURCE(&hdma_spi5_rx, DMA_IT_TC))
  {
      // 清除传输完成中断标志
      __HAL_DMA_CLEAR_FLAG(&hdma_spi5_rx, __HAL_DMA_GET_TC_FLAG_INDEX(&hdma_spi5_rx));

      // 设置全局标志，通知主程序DMA传输已完成
      dma_transfer_complete = 1; // <--- 设置标志
  }
  else
  {
      // 如果是其他中断 (比如错误中断)，则调用默认的HAL处理函数
      HAL_DMA_IRQHandler(&hdma_spi5_rx); // <--- 调用默认处理
  }

  /* USER CODE END DMA2_Stream3_IRQn 0 */
  // 即使我们自己处理了TC中断，也最好调用一次HAL_DMA_IRQHandler
  // 以确保其他可能的DMA中断（如错误中断）被正确处理。
  HAL_DMA_IRQHandler(&hdma_spi5_rx); // <--- 调用HAL库处理其他可能的中断
  /* USER CODE BEGIN DMA2_Stream3_IRQn 1 */

  /* USER CODE END DMA2_Stream3_IRQn 1 */
}

// 注意：如果你的TX Channel (例如 Stream 2) 也需要单独处理，
// 你可能需要添加类似的 DMA2_Stream2_IRQHandler。
// 但是，对于全双工SPI传输，通常RX的中断就足以标志整个传输周期的结束，
// 因为RX需要等待TX发送的数据才能接收。所以只处理RX中断通常就足够了。
// 但如果遇到问题，可以考虑也处理TX中断。

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
```

**中断处理函数关键点解释：**

*   **`DMA2_Stream3_IRQHandler`**: 这是CPU响应DMA2的Stream 3中断的入口函数。函数名必须与你在CubeMX中为SPI5_RX分配的DMA Channel对应的中断向量匹配。
*   **`__HAL_DMA_GET_FLAG`**: 检查DMA硬件寄存器中的传输完成标志位是否被置位。
*   **`__HAL_DMA_GET_IT_SOURCE`**: 检查在初始化时是否使能了传输完成中断 (`DMA_IT_TC`)。
*   **`__HAL_DMA_CLEAR_FLAG`**: **非常重要**！必须在处理完中断后清除标志位，否则中断会一直触发。
*   **`dma_transfer_complete = 1;`**: 这是中断服务程序的核心任务：设置我们在主函数中等待的标志。
*   **`HAL_DMA_IRQHandler`**: 调用HAL库的通用DMA处理函数，它可以处理错误中断等其他情况。

**总结:**

1.  CubeMX配置DMA硬件并生成初始化代码。
2.  在主程序中，使用 `HAL_SPI_TransmitReceive_DMA` 替换 `HAL_SPI_TransmitReceive`。
3.  使用一个全局 `volatile` 标志 `dma_transfer_complete` 来同步主程序和DMA传输。
4.  在主程序启动DMA后，等待 `dma_transfer_complete` 标志被置位。
5.  在 `stm32f4xx_it.c` 中编写DMA中断服务程序，当传输完成时将 `dma_transfer_complete` 标志置位。

这样，CPU就可以在等待SPI传输完成时执行其他任务，提高了效率。