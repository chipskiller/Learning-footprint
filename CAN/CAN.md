# 关于“CAN发送”的寄存器
- 先解释这三个标志的含义 
以某个邮箱 x（比如 0 号邮箱）为例：

  - RQCPx（Request Completed）=1

    - 这次发送请求“处理完了”，不管成功还是失败。

  - TMEx（Transmit Mailbox Empty）=1

    - 这个邮箱现在是空闲的，里面已经没有待发的数据。

  - TXOKx（Transmission OK）

     - 1：这次发送被认为是成功（发完并收到了 ACK）；

     - 0：这次发送被认为是失败（被中止、禁止重发、无 ACK 等）。