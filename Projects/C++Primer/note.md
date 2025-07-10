# 项目说明
在这个项目中，您将实现一个跳表（Skip List），这是一种用于实现有序集（Ordered Set）的抽象数据类型的不同方法。与需要在插入和删除时进行再平衡的平衡搜索树（如红黑树、AVL树或伸展树）不同，跳表通过使用随机数生成器来决定每个节点将拥有的层数，从而在不进行再平衡的情况下维持其结构属性，并实现了对数时间复杂度的查找、插入和删除操作。

## 基本概念：
有序集是一种非常有用的抽象数据类型，它能够在保持键有序的同时，允许高效的查找、插入和删除操作。
挑战：如果使用普通的链表来实现有序集，那么这些操作的时间复杂度将是线性的，因为需要遍历每个节点才能到达目标位置。
解决方案：跳表通过引入多层次的链接，使每一层都可以“跳过”一定数量的元素，从而加快遍历速度。例如，顶层链接可以跳过一半的节点，进一步减少需要检查的节点数量。

## 学习目标
完成本项目后，您将能够：
实现一个维护有序集接口的跳表。
更深入地理解现代C++的基础知识以及STL容器。
学习处理并发的技术。

# 跳表
## 什么是跳表
跳表（Skip List）是一种基于链表的数据结构，旨在通过“跳跃”节点来提高查找效率，从而实现类似于二分查找的对数性能。它是由 William Pugh 于 1990 年提出的，是一种支持快速搜索、插入和删除操作的概率型数据结构。

跳表可用于实现有序集合（如 std::set）或映射（如 std::map）等抽象数据结构的底层实现。

## 跳表结构核心

- 在普通有序链表的基础上，引入多层索引。

- 每一层都是一个“跳跃层”，用于快速跳过若干元素。

- 最底层是完整的有序链表，越高层的节点越少。

- 每个节点的“高度”是随机决定的，从而保证平衡性。

**示例：**
高度为4的跳表
```
Level 3:     [H] ------------> [30]
Level 2:     [H] ----> [10] -->[30]
Level 1:     [H] ----> [10] -->[20] -->[30]
Level 0:     [H] --> [5] -->[10] -->[20] -->[30] -->[40]

```

- `H`: 头节点
- 每个节点的高度决定它出现在哪些层
- 通过逐层条跳跃，查找效率提高到O(logn)
- 最底层是完整有序链表，保证数据完整性
- 节点高度是随机决定的，比如[10]出现在0、1、2层，因此高度为3
- 查找是时，从最高层查找，如果目标值比当前节点小，往下跳一层，如果比当前节点大，往又走

# 代码实现

## skiplist.h


## skiplist.cpp
```cpp
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::Empty() -> bool {
  return header_->links_[0] == nullptr;
}
```

`SKIPLIST_TEMPLATE_ARGUMENTS`是一个宏，它在`skiplist.h`定义的，
```cpp
#define SKIPLIST_TEMPLATE_ARGUMENTS \
  template <typename K, typename Compare, size_t MaxHeight, uint32_t Seed>
```
意思是：这是一个 模板类的函数定义，它属于 SkipList<K, Compare, MaxHeight, Seed> 这个类。

`auto SkipList<...>::Empty() -> bool`
这是一个跳表类的成员函数，表示我们正在定义 SkipList 的 Empty() 方法。

auto ... -> bool 是 C++11 的新语法，和 bool Empty() const 等价。

这里表示这个函数返回一个 bool 类型，即 true 或 false。

## 插入和删除的思路
```cpp
//Erase

  for (int level = static_cast<int>(height_) - 1; level >= 0; level--) {
    while (true) {
      auto next = curr->Next(level);
      if (next == nullptr || compare_(key, next->Key())) {
        break;
      }
      if (!(compare_(key, next->Key()) || compare_(next->Key(), key))) {
        update[level] = curr;
        break;
      }
      curr = next;
    }
  }
```
自顶向下找到目标节点的前一个节点

## 并发处理
```cpp
std::lock_guard<std::mutex> lock(mutex_);
```
在插入和删除函数中上锁


# 测试
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
$ make -j`nproc`

$ cd build
$ make -j$(nproc) skiplist_test
$ ./test/skiplist_test