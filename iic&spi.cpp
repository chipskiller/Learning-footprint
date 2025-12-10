//
// Created by CHIPS KILLER on 2025/12/7.
//UFT_8
/**
    *HAL 库通过 HAL_I2C_Xxx 函数族来完成 I2C 通信。常用的函数包括：
        HAL_I2C_Master_Transmit() / HAL_I2C_Master_Receive()：主机模式下发送/接收数据块。
        HAL_I2C_Mem_Write() / HAL_I2C_Mem_Read()：主机模式下对具有寄存器地址的从设备进行读写（适用于传感器类设备）。
        HAL_I2C_IsDeviceReady()：检测从设备是否应答（可用于检查设备连接）。
        以及相应的带 DMA 或中断的异步函数（如 HAL_I2C_Master_Transmit_DMA 等）。
