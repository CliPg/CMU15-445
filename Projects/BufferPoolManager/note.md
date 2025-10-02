# **项目一：缓冲池管理器（Buffer Pool Manager）**

你的第一个编程项目是实现 DBMS 的 **缓冲池管理器**。

缓冲池的职责是：

1. 在主存中的缓冲区与持久化存储之间移动物理数据页。
2. 作为缓存，将频繁使用的页保留在内存中以加快访问速度，并将未使用或冷数据页换出到存储设备中。



### **页面与帧的概念**

- **页面（Page）**：BusTub 中每一页大小固定为 **4096 字节（4 KB）**，缓冲池以 4 KB 为单位管理数据。

- **帧（Frame）**：是内存中的固定大小（4 KB）的缓冲块，用于存放一页数据。

- 区别：

  

  - 页面是逻辑（虚拟）的数据单元，可能存在于内存、磁盘，或者同时存在。
  - 帧是物理内存块（指针），用于实际存放一页数据。

  

### **I/O 操作抽象**

缓冲池执行的 I/O 操作对 DBMS 其他部分是透明的：

- 当执行引擎等组件通过唯一标识符 page_id_t 请求一页数据时，它不需要关心该页是否已在内存中，或是否需要从磁盘读取。
- 同样，缓冲池管理器也不需要理解这些页面的内容，只需知道数据的位置。



### **实现要求**

你的缓冲池实现必须 **线程安全**：

- 多个线程会并发访问缓冲池的内部数据结构。
- 需要使用 **latch（闩锁）**（在操作系统中称为“锁”）来保护临界区。



### **你必须实现以下存储管理组件**

1. **LRU-K 页面置换策略（LRU-K Replacement Policy）**
2. **磁盘调度器（Disk Scheduler）**
3. **缓冲池管理器（Buffer Pool Manager）**





## **任务 #1 —— LRU-K 页面置换策略**

该组件负责跟踪缓冲池中页面的使用情况，以确定哪些页面/帧应该从内存中淘汰并写回磁盘。你将实现一个名为 **LRUKReplacer** 的类：

- 头文件：src/include/buffer/lru_k_replacer.h
- 实现文件：src/buffer/lru_k_replacer.cpp

> **提示**：

- > **LRUKReplacer** 是一个独立的类，与其他 Replacer 类无关。

- > 你只需实现 **LRU-K** 置换策略，**无需实现** 普通 LRU 或 Clock 置换策略（即使对应文件已经存在）。

------



### **LRU-K 算法简介**

LRU-K 算法会淘汰 **后向 K 距离（backward k-distance）** 最大的帧。

- **后向 K 距离** = 当前时间戳与该帧 **第 K 次最近访问** 时间戳的差值。
- 如果某帧的历史访问次数少于 K 次，则将其后向 K 距离视为 **+∞**。
- 若多个帧的后向 K 距离都是 **+∞**，则淘汰**最早整体时间戳**的帧（即所有记录访问中时间最早的那一帧）。

------

### **其他要求**

- **LRUKReplacer** 的最大容量与缓冲池的大小相同（它为缓冲池中的所有帧提供占位符）。
- 但在任意时刻，并非所有帧都可被淘汰。
- **LRUKReplacer 的 size** = 当前**可被淘汰的帧数量**。
- 初始时，LRUKReplacer 为空，只有当帧被标记为“可淘汰”时，其 size 才会增加。
- 当帧被固定（pinned）或不再使用时，size 会减少。

------

### **需要实现的方法**

按照头文件中的定义，实现以下方法：

1. **Evict() -> std::optional<frame_id_t>**

   

   - 淘汰后向 K 距离最大的可淘汰帧。
   - 如果没有可淘汰帧，返回 std::nullopt。

   

   思路：

   首先判断`node_store_.size()`和`curr_size`是否为0，为0则返回`nullopt`

   然后根据LRU-K算法知道，对于每一帧分为访问次数小于k和等于k两种类型，如果所有帧的历史访问记录都等于k，则返回历史记录首项最小的帧（因为历史记录是根据当前时间戳确定的，list越后面越大，根据后向k距离可知即比较第一项），如果存在帧访问记录小于k，则只需要在访问次数小于k的帧之间比较。

   最后需要移除该帧。

   

2. **RecordAccess(frame_id_t frame_id)**

   

   - 记录指定帧在当前时间戳被访问。
   - 每当 BufferPoolManager 中的某页被固定（pinned）后，调用此方法。

   

   思路：

   每个帧的历史记录是一个长度小于等于k的list，访问指定帧时就在list后面加上当前时间戳，如果长度大于k时，就需要去掉首项。（这样做可能只是为了实现简便，如果要保留所有的历史记录可能就需要向后遍历到第k项）

   当前时间戳需要自增。

   需要上锁，避免同时访问一个帧或同时访问不同帧导致时间戳相同。

   

3. **Remove(frame_id_t frame_id)**

   

   - 清除与指定帧相关的所有访问历史。
   - 仅当 BufferPoolManager 中的某页被删除时调用。

   

   思路：

   删除指定帧，要上锁。

   如果该帧可驱逐，curr_size需要自减

   

4. **SetEvictable(frame_id_t frame_id, bool set_evictable)**

   

   - 控制某帧是否可被淘汰。
   - 同时更新 LRUKReplacer 的 size。
   - 例如：当某页的 pin 计数降到 0 时，其对应帧应标记为可淘汰。

   

   思路：

   当前帧可淘汰，curr_size自增，反之自减

   

5. **Size() -> size_t**

   

   - 返回当前可被淘汰的帧的数量。

   即curr_size

------



### **其他说明**

- 实现细节完全由你决定。
- 允许使用 **STL 容器**（如 std::unordered_map, std::list, std::deque 等）。
- 假设你的数据结构不会耗尽内存（⚠️ 但在任务 #3 的缓冲池实现中，不能做此假设）。
- 必须确保你的实现 **线程安全**。



## 任务 #2 - 磁盘调度器 (Disk Scheduler)





这个组件负责对 **DiskManager** 的读写操作进行调度。你需要实现一个类 **DiskScheduler**，它的头文件在

src/include/storage/disk/disk_scheduler.h，实现文件在

src/storage/disk/disk_scheduler.cpp。



### 磁盘调度器的作用

- 磁盘调度器可以被其他组件使用（在这里是 **BufferPoolManager**，也就是任务 #3 中的缓冲池管理器）。
- 它负责将磁盘请求（**DiskRequest 结构体**，已经在 src/include/storage/disk/disk_scheduler.h 定义）加入队列，等待处理。
- 磁盘调度器会维护一个 **后台工作线程 (background worker thread)**，这个线程负责处理所有已经调度的请求。



### 调度机制

- 磁盘调度器内部会维护一个 **共享队列 (channel)**，用来存放需要处理的 DiskRequest。
- 一个线程将请求加入队列；
- 后台工作线程会从队列中取出请求，并交给 **DiskManager** 执行。
- 我们已经在 src/include/common/channel.h 提供了一个 **Channel 类**，它是线程安全的，可以直接用来在多个线程之间共享数据。当然，如果你愿意，也可以自己实现。



------



#### **构造与析构**

- DiskScheduler 的 **构造函数**和**析构函数**已经实现好：

  

  - 构造函数会创建后台工作线程；
  - 析构函数会等待并回收（join）这个线程。

  

- 你只需要实现两个方法：



------



#### **你需要实现的方法**

1. **Schedule(DiskRequest r)**

   

   - 把一个请求放入调度器队列中，等待磁盘管理器执行。

   - DiskRequest 结构体中包含：

     

     - 请求类型（读/写）；
     - 数据应该读入或写出的内存地址；
     - 需要操作的页面 ID；
     - 一个 **std::promise**，它的值应当在请求处理完成后设置为 true。

     

   - std::promise/std::future 是 C++ 提供的一种异步回调机制，允许发起请求的线程等待或获知请求什么时候完成。


思路：

把请求put到队列即可



对于move(r),因为DiskRequest用了make_unique禁止拷贝

```
#include <iostream>
#include <string>
#include <utility>  // std::move

int main() {
    std::string s = "hello world";
    std::string a = s;             // 拷贝，s 还是 "hello world"
    std::string b = std::move(s);  // 移动，b = "hello world"，s 变成空

    std::cout << "s = " << s << "\n";  // s 已经被“搬空”，可能输出空字符串
    std::cout << "b = " << b << "\n";  // b = hello world
}

```



------



2. **StartWorkerThread()**



- 后台工作线程的启动函数。

- 构造函数中会创建线程，并让它执行这个方法。

- 线程会不断循环：

  

  - 从队列里取出请求；
  - 调用 **DiskManager** 去执行请求；
  - 请求完成后，设置该 DiskRequest 的 **std::promise**，通知发起者任务已完成。

  

- 这个函数在调度器销毁（析构）前不会返回。

思路：

设置一个循环，不断取出队列，直到取完。

然后用disk_manager读或写页面。

完成后需要set callback，表示任务完成。





------





#### **提示**





- DiskRequest 的 std::promise 就是回调机制：

  

  - 发起请求的线程可以通过对应的 **std::future** 等待结果；
  - 当后台线程处理完成请求后，调用 promise.set_value(true) 来通知。

  

- 你可以参考 disk_scheduler_test.cpp 看测试代码是如何使用 promise/future 的。

- 实现时必须保证线程安全（例如多线程同时调用 Schedule 时，不能产生竞态条件）。





------



✅ **总结**

你要写的就是一个**生产者-消费者模型**：



- Schedule() 是生产者，把请求放到队列里；
- StartWorkerThread() 是消费者，从队列里取请求并处理；
- 处理完成后用 std::promise 通知请求发起方。



## 任务 #3 - 缓冲池管理器（Buffer Pool Manager）





最后，你需要实现缓冲池管理器 (**BufferPoolManager**)！

正如本页面开头所说，缓冲池管理器的职责是：



- 使用 **DiskScheduler** 从磁盘读取数据库页面并存放到内存；
- 当被显式要求，或在需要淘汰页面腾出空间时，将**脏页（dirty pages）**写回磁盘。





你的 BufferPoolManager 实现会用到你在前两个任务里写的 **LRUKReplacer** 和 **DiskScheduler**：



- **LRUKReplacer** 负责记录页面访问时间，从而决定在需要腾出空间时，应该淘汰哪个 frame；
- **DiskScheduler** 会调度页面的读写请求并调用 **DiskManager** 执行真正的磁盘操作。





我们提供了一个辅助类 **FrameHeader**，用于管理内存中的 frame。所有对页面数据的访问都必须通过 FrameHeader。



- FrameHeader::GetData 方法会返回一个指向该 frame 内存的原始指针，DiskScheduler / DiskManager 会用它来把物理页的内容拷贝到内存。
- 缓冲池管理器本身并不关心页面的具体内容，它只知道 **page_id** 和 **FrameHeader** 的对应关系。
- 同一个 FrameHeader 在系统生命周期内会被重复利用来存储不同的页面。





------





### **并发控制（Concurrency）**





在实现多线程的 BufferPoolManager 时，必须保证数据访问的同步，避免以下情况：



1. 线程 T1 从磁盘加载页面 X1 到 frame，并修改成新版本 X2；

2. 线程 T2 也从磁盘加载同一个页面 X1 到另一个 frame，并修改成另一个版本 X3；

3. T2 写回磁盘，把 X3 覆盖到磁盘；

4. T1 写回磁盘，把旧的 X2 再次覆盖到磁盘；

   👉 结果就是数据竞争（data race），磁盘上的数据就乱掉了。





因此我们必须保证：



- **同一个页面在内存中只能有一个副本**；
- 不能在有线程正在访问页面时把它驱逐（evict）出去。





为此，需要维护一个 **pin count（引用计数 / 固定计数）**：



- pin count 表示当前有多少线程正在访问该页面；
- 只要 pin count > 0，缓冲池管理器就**不能驱逐**这个页面；
- pin count 可以通过 **FrameHeader::pin_count_** （一个原子变量）来维护；
- 注意：pin_count_ 和 LRUKReplacer::SetEvictable 是分开的，你需要自己确保两者保持一致。





此外，每个 FrameHeader 还有一个 is_dirty_ 标志：



- 如果页面在内存中被修改过，就要标记为 dirty；
- 当页面被驱逐时，如果是 dirty，就必须先写回磁盘。





------





### **PageGuard（页面守卫）**





你还需要实现两个 RAII 对象：



- **ReadPageGuard**：提供线程安全的只读访问；
- **WritePageGuard**：提供线程安全的读写访问。





它们的职责是：



- 当创建时，自动 pin 住页面；
- 当销毁时，自动 unpin 页面，必要时刷新到磁盘；
- 确保线程不会忘记释放页面，避免死锁或内存泄漏。





建议你和 BufferPoolManager 的方法一起实现，比如：



- CheckedReadPage 和 CheckedWritePage 返回对应的 Guard；
- 你也可以先实现 GetPinCount，然后拼凑一个最简单的 PageGuard 测试通过的版本，再慢慢完善。





------





### **需要实现的方法**







#### **PageGuard（在** 

#### **page_guard.h**

####  **和** 

#### **page_guard.cpp**

#### **）：**





- ReadPageGuard::ReadPageGuard()
- ReadPageGuard::ReadPageGuard(ReadPageGuard &&that)
- ReadPageGuard::operator=(ReadPageGuard &&that) -> ReadPageGuard &
- ReadPageGuard::Flush()
- ReadPageGuard::Drop()
- WritePageGuard::WritePageGuard()
- WritePageGuard::WritePageGuard(WritePageGuard &&that)
- WritePageGuard::operator=(WritePageGuard &&that) -> WritePageGuard &
- WritePageGuard::Flush()
- WritePageGuard::Drop()





> 这些方法实现了 **移动语义（move semantics）** 和 **RAII**。两个类几乎完全相同，只有少量差异。



------





#### **BufferPoolManager（在** 

#### **buffer_pool_manager.h**

####  **和** 

#### **buffer_pool_manager.cpp**

#### **）：**





- NewPage() -> page_id_t
- DeletePage(page_id_t page_id) -> bool
- CheckedWritePage(page_id_t page_id) -> std::optional<WritePageGuard>
- CheckedReadPage(page_id_t page_id) -> std::optional<ReadPageGuard>
- FlushPageUnsafe(page_id_t page_id) -> bool
- FlushPage(page_id_t page_id) -> bool
- FlushAllPagesUnsafe()
- FlushAllPages()
- GetPinCount(page_id_t page_id)





每个函数在源码文件里都有详细的注释，强烈建议你仔细阅读。



------





### **并发性能要求**





- 所有 BufferPoolManager 的公有方法，可以在开始时加锁，在结束时解锁，这样就能保证安全（不用追求极致性能）；
- 但要避免死锁，有些地方可能需要提前释放锁；
- 不需要非常高效，但必须合理，否则后续实验（比如 QPS.1 / QPS.2 基准测试）会出问题。





------



⚡总结：



1. BufferPoolManager 是**核心模块**，负责磁盘与内存之间的页面调度。
2. 要结合 **LRUKReplacer**（决定驱逐页面）和 **DiskScheduler**（调度磁盘读写）。
3. 要维护 **page table**（page_id → frame 的映射）、**pin_count** 和 **is_dirty**。
4. 要实现 **RAII PageGuard**，确保线程安全和资源自动释放。