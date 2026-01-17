# 创建对象
![img.png](img.png)
> - 传感器等需要时间初始化的外设，使用这种方法在HAL库初始化完之后创建对象

## C++编译时会先初始化变量，因为librm库变量是写到globals中的，而globals是一个全局指针，在globals在主进程中被new之前直接调用globals中变量的话，会使用globals指针，而此时globals未被new，会造成空指针，从而进入HardFault