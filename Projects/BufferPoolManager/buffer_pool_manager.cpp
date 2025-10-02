//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager.h"

namespace bustub {

/**
 * @brief The constructor for a `FrameHeader` that initializes all fields to default values.
 *
 * See the documentation for `FrameHeader` in "buffer/buffer_pool_manager.h" for more information.
 *
 * @param frame_id The frame ID / index of the frame we are creating a header for.
 */
FrameHeader::FrameHeader(frame_id_t frame_id) : frame_id_(frame_id), data_(BUSTUB_PAGE_SIZE, 0) { Reset(); }

/**
 * @brief Get a raw const pointer to the frame's data.
 *
 * @return const char* A pointer to immutable data that the frame stores.
 */
auto FrameHeader::GetData() const -> const char * { return data_.data(); }

/**
 * @brief Get a raw mutable pointer to the frame's data.
 *
 * @return char* A pointer to mutable data that the frame stores.
 */
auto FrameHeader::GetDataMut() -> char * { return data_.data(); }

/**
 * @brief Resets a `FrameHeader`'s member fields.
 */
void FrameHeader::Reset() {
  std::fill(data_.begin(), data_.end(), 0);
  pin_count_.store(0);
  is_dirty_ = false;
}

/**
 * @brief Creates a new `BufferPoolManager` instance and initializes all fields.
 *
 * See the documentation for `BufferPoolManager` in "buffer/buffer_pool_manager.h" for more information.
 *
 * ### Implementation
 *
 * We have implemented the constructor for you in a way that makes sense with our reference solution. You are free to
 * change anything you would like here if it doesn't fit with you implementation.
 *
 * Be warned, though! If you stray too far away from our guidance, it will be much harder for us to help you. Our
 * recommendation would be to first implement the buffer pool manager using the stepping stones we have provided.
 *
 * Once you have a fully working solution (all Gradescope test cases pass), then you can try more interesting things!
 *
 * @param num_frames The size of the buffer pool.
 * @param disk_manager The disk manager.
 * @param k_dist The backward k-distance for the LRU-K replacer.
 * @param log_manager The log manager. Please ignore this for P1.
 */
BufferPoolManager::BufferPoolManager(size_t num_frames, DiskManager *disk_manager, size_t k_dist,
                                     LogManager *log_manager)
    : num_frames_(num_frames),
      next_page_id_(0),
      bpm_latch_(std::make_shared<std::mutex>()),
      replacer_(std::make_shared<LRUKReplacer>(num_frames, k_dist)),
      disk_scheduler_(std::make_shared<DiskScheduler>(disk_manager)),
      log_manager_(log_manager) {
  // Not strictly necessary...
  std::scoped_lock latch(*bpm_latch_);

  // Initialize the monotonically increasing counter at 0.
  next_page_id_.store(0);

  // Allocate all of the in-memory frames up front.
  frames_.reserve(num_frames_);

  // The page table should have exactly `num_frames_` slots, corresponding to exactly `num_frames_` frames.
  page_table_.reserve(num_frames_);

  // Initialize all of the frame headers, and fill the free frame list with all possible frame IDs (since all frames are
  // initially free).
  for (size_t i = 0; i < num_frames_; i++) {
    frames_.push_back(std::make_shared<FrameHeader>(i));
    free_frames_.push_back(static_cast<int>(i));
  }
}

/**
 * @brief Destroys the `BufferPoolManager`, freeing up all memory that the buffer pool was using.
 */
BufferPoolManager::~BufferPoolManager() = default;

/**
 * @brief Returns the number of frames that this buffer pool manages.
 */
auto BufferPoolManager::Size() const -> size_t { return num_frames_; }

/**
 * @brief Allocates a new page on disk.
 *
 * ### Implementation
 *
 * You will maintain a thread-safe, monotonically increasing counter in the form of a `std::atomic<page_id_t>`.
 * See the documentation on [atomics](https://en.cppreference.com/w/cpp/atomic/atomic) for more information.
 *
 * TODO(P1): Add implementation.
 *
 * @return The page ID of the newly allocated page.
 */
auto BufferPoolManager::NewPage() -> page_id_t {
  std::scoped_lock<std::mutex> lock(*bpm_latch_);
  // fetch_add原子地atomic变量加上指定值
  page_id_t new_page_id = next_page_id_.fetch_add(1);
  return new_page_id;
}

/**
 * @brief Removes a page from the database, both on disk and in memory.
 *
 * If the page is pinned in the buffer pool, this function does nothing and returns `false`. Otherwise, this function
 * removes the page from both disk and memory (if it is still in the buffer pool), returning `true`.
 *
 * ### Implementation
 *
 * Think about all of the places a page or a page's metadata could be, and use that to guide you on implementing this
 * function. You will probably want to implement this function _after_ you have implemented `CheckedReadPage` and
 * `CheckedWritePage`.
 *
 * You should call `DeallocatePage` in the disk scheduler to make the space available for new pages.
 *
 * TODO(P1): Add implementation.
 *
 * @param page_id The page ID of the page we want to delete.
 * @return `false` if the page exists but could not be deleted, `true` if the page didn't exist or deletion succeeded.
 */
auto BufferPoolManager::DeletePage(page_id_t page_id) -> bool {
  frame_id_t frame_id = page_table_[page_id];
  std::shared_ptr<FrameHeader> header = frames_[frame_id];
  if (header->pin_count_ != 0) {
    return false;
  }
  disk_scheduler_->DeallocatePage(page_id);
  free_frames_.push_back(frame_id);
  return true;
}

/**
 * @brief Acquires an optional write-locked guard over a page of data. The user can specify an `AccessType` if needed.
 *
 * If it is not possible to bring the page of data into memory, this function will return a `std::nullopt`.
 *
 * Page data can _only_ be accessed via page guards. Users of this `BufferPoolManager` are expected to acquire either a
 * `ReadPageGuard` or a `WritePageGuard` depending on the mode in which they would like to access the data, which
 * ensures that any access of data is thread-safe.
 *
 * There can only be 1 `WritePageGuard` reading/writing a page at a time. This allows data access to be both immutable
 * and mutable, meaning the thread that owns the `WritePageGuard` is allowed to manipulate the page's data however they
 * want. If a user wants to have multiple threads reading the page at the same time, they must acquire a `ReadPageGuard`
 * with `CheckedReadPage` instead.
 *
 * ### Implementation
 *
 * There are 3 main cases that you will have to implement. The first two are relatively simple: one is when there is
 * plenty of available memory, and the other is when we don't actually need to perform any additional I/O. Think about
 * what exactly these two cases entail.
 *
 * The third case is the trickiest, and it is when we do not have any _easily_ available memory at our disposal. The
 * buffer pool is tasked with finding memory that it can use to bring in a page of memory, using the replacement
 * algorithm you implemented previously to find candidate frames for eviction.
 *
 * Once the buffer pool has identified a frame for eviction, several I/O operations may be necessary to bring in the
 * page of data we want into the frame.
 *
 * There is likely going to be a lot of shared code with `CheckedReadPage`, so you may find creating helper functions
 * useful.
 *
 * These two functions are the crux of this project, so we won't give you more hints than this. Good luck!
 *
 * TODO(P1): Add implementation.
 *
 * @param page_id The ID of the page we want to write to.
 * @param access_type The type of page access.
 * @return std::optional<WritePageGuard> An optional latch guard where if there are no more free frames (out of memory)
 * returns `std::nullopt`, otherwise returns a `WritePageGuard` ensuring exclusive and mutable access to a page's data.
 */
  /**
 * @brief 获取某个数据页的可选写锁守卫（write-locked guard）。用户可以在需要时指定 `AccessType`。
 *
 * 如果无法将目标页加载到内存中，本函数会返回 `std::nullopt`。
 *
 * 数据页只能通过 page guard 来访问。`BufferPoolManager` 的使用者必须获取 `ReadPageGuard` 或
 * `WritePageGuard`，以此来确保数据访问是线程安全的。
 *
 * 在同一时刻，一个数据页只能被 **1 个 `WritePageGuard`** 持有并进行读/写操作。
 * 这允许对数据进行不可变或可变访问，也就是说持有该 `WritePageGuard` 的线程可以任意修改该页的数据。
 * 如果用户希望多个线程同时读取该页，则必须调用 `CheckedReadPage` 来获取多个 `ReadPageGuard`。
 *
 * ### 实现提示
 *
 * 你需要实现三种主要情况：
 * 1. **有足够的可用内存** —— 直接分配 frame 即可，比较简单；
 * 2. **不需要额外的 I/O** —— 说明数据已经在 buffer pool 中了，只需加锁即可；
 * 3. **没有容易获得的可用内存** —— 这是最复杂的情况。此时 buffer pool 需要通过之前实现的替换算法来
 *    找到候选 frame，并执行淘汰。可能需要做多次磁盘 I/O 才能把目标页加载进来。
 *
 * 当找到可用 frame 后，目标页的数据需要加载进该 frame 中。
 *
 * `CheckedWritePage` 和 `CheckedReadPage` 会有很多重复的逻辑，因此你可能需要写一些辅助函数来减少代码冗余。
 *
 * 这两个函数是本项目的核心，所以我们不会再给更多提示了。祝你好运！
 *
 * TODO(P1): 实现这里的逻辑。
 *
 * @param page_id 想要写入的页的 ID。
 * @param access_type 页访问的类型。
 * @return 如果内存不足（没有 free frame）则返回 `std::nullopt`；
 *         否则返回一个 `WritePageGuard`，它能保证对该页数据的独占且可变的访问。
 */

void BufferPoolManager::UpdateFrame(frame_id_t fid, page_id_t pid, bool is_write) {
  auto frame = frames_[fid];
  auto data = const_cast<char *>(frame->GetData());
  std::promise<bool> promise;
  auto future = promise.get_future();
  DiskRequest r{is_write, data, pid, std::move(promise)};
  disk_scheduler_->Schedule(std::move(r));
  future.get();
  frame->is_dirty_ = false;
}

std::optional<frame_id_t> BufferPoolManager::GetFrame(page_id_t page_id) {
    // 如果已经在缓冲区中
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        replacer_->RecordAccess(it->second);
        return it->second;
    }

    frame_id_t frame_id;
    auto get_free_frame = [&]() -> bool {
        if (!free_frames_.empty()) {
            frame_id = free_frames_.front();
            free_frames_.pop_front();
            return true;
        }
        return false;
    };
    // 先尝试空闲帧
    if (!get_free_frame()) {
        // 否则尝试驱逐
        auto evicted = replacer_->Evict();
        if (!evicted.has_value()) {
            return std::nullopt;  // 没有可用帧
        }
        frame_id = evicted.value();

        // 正在被占用的帧不能驱逐
        if (frames_[frame_id]->pin_count_ > 0) {
            return std::nullopt;
        }

        // 从 page_table 移除并刷回磁盘
        for (auto it = page_table_.begin(); it != page_table_.end(); ++it) {
            if (it->second == frame_id) {
                UpdateFrame(frame_id, it->first, true);
                page_table_.erase(it);
                break;
            }
        }
    }

    // 初始化新页面（读/写时）
    UpdateFrame(frame_id, page_id, false);

    replacer_->RecordAccess(frame_id);
    page_table_[page_id] = frame_id;
    return frame_id;
}


auto BufferPoolManager::CheckedWritePage(page_id_t page_id, AccessType access_type) -> std::optional<WritePageGuard> {
  auto frame_id = GetFrame(page_id);
  if (!frame_id.has_value()) {
    return std::nullopt;
  }
  auto frame = frames_[frame_id.value()];
  return WritePageGuard(page_id, frame, replacer_, bpm_latch_, disk_scheduler_);  
}

/**
 * @brief Acquires an optional read-locked guard over a page of data. The user can specify an `AccessType` if needed.
 *
 * If it is not possible to bring the page of data into memory, this function will return a `std::nullopt`.
 *
 * Page data can _only_ be accessed via page guards. Users of this `BufferPoolManager` are expected to acquire either a
 * `ReadPageGuard` or a `WritePageGuard` depending on the mode in which they would like to access the data, which
 * ensures that any access of data is thread-safe.
 *
 * There can be any number of `ReadPageGuard`s reading the same page of data at a time across different threads.
 * However, all data access must be immutable. If a user wants to mutate the page's data, they must acquire a
 * `WritePageGuard` with `CheckedWritePage` instead.
 *
 * ### Implementation
 *
 * See the implementation details of `CheckedWritePage`.
 *
 * TODO(P1): Add implementation.
 *
 * @param page_id The ID of the page we want to read.
 * @param access_type The type of page access.
 * @return std::optional<ReadPageGuard> An optional latch guard where if there are no more free frames (out of memory)
 * returns `std::nullopt`, otherwise returns a `ReadPageGuard` ensuring shared and read-only access to a page's data.
 */
auto BufferPoolManager::CheckedReadPage(page_id_t page_id, AccessType access_type) -> std::optional<ReadPageGuard> {
  auto frame_id = GetFrame(page_id);
  if (!frame_id.has_value()) {
    return std::nullopt;
  }
  replacer_->SetEvictable(frame_id.value(), false);
  auto frame = frames_[frame_id.value()];
  return ReadPageGuard(page_id, frame, replacer_, bpm_latch_, disk_scheduler_);
}

/**
 * @brief A wrapper around `CheckedWritePage` that unwraps the inner value if it exists.
 *
 * If `CheckedWritePage` returns a `std::nullopt`, **this function aborts the entire process.**
 *
 * This function should **only** be used for testing and ergonomic's sake. If it is at all possible that the buffer pool
 * manager might run out of memory, then use `CheckedPageWrite` to allow you to handle that case.
 *
 * See the documentation for `CheckedPageWrite` for more information about implementation.
 *
 * @param page_id The ID of the page we want to read.
 * @param access_type The type of page access.
 * @return WritePageGuard A page guard ensuring exclusive and mutable access to a page's data.
 */
auto BufferPoolManager::WritePage(page_id_t page_id, AccessType access_type) -> WritePageGuard {
  auto guard_opt = CheckedWritePage(page_id, access_type);
  if (!guard_opt.has_value()) {
    fmt::println(stderr, "\n`CheckedWritePage` failed to bring in page {}\n", page_id);
    std::abort();
  }

  return std::move(guard_opt).value();
}

/**
 * @brief A wrapper around `CheckedReadPage` that unwraps the inner value if it exists.
 *
 * If `CheckedReadPage` returns a `std::nullopt`, **this function aborts the entire process.**
 *
 * This function should **only** be used for testing and ergonomic's sake. If it is at all possible that the buffer pool
 * manager might run out of memory, then use `CheckedPageWrite` to allow you to handle that case.
 *
 * See the documentation for `CheckedPageRead` for more information about implementation.
 *
 * @param page_id The ID of the page we want to read.
 * @param access_type The type of page access.
 * @return ReadPageGuard A page guard ensuring shared and read-only access to a page's data.
 */
auto BufferPoolManager::ReadPage(page_id_t page_id, AccessType access_type) -> ReadPageGuard {
  auto guard_opt = CheckedReadPage(page_id, access_type);

  if (!guard_opt.has_value()) {
    fmt::println(stderr, "\n`CheckedReadPage` failed to bring in page {}\n", page_id);
    std::abort();
  }

  return std::move(guard_opt).value();
}

/**
 * @brief Flushes a page's data out to disk unsafely.
 *
 * This function will write out a page's data to disk if it has been modified. If the given page is not in memory, this
 * function will return `false`.
 *
 * You should not take a lock on the page in this function.
 * This means that you should carefully consider when to toggle the `is_dirty_` bit.
 *
 * ### Implementation
 *
 * You should probably leave implementing this function until after you have completed `CheckedReadPage` and
 * `CheckedWritePage`, as it will likely be much easier to understand what to do.
 *
 * TODO(P1): Add implementation
 *
 * @param page_id The page ID of the page to be flushed.
 * @return `false` if the page could not be found in the page table, otherwise `true`.
 */
 /**
 * @brief 不加锁地将某个页面的数据刷新到磁盘。
 *
 * 如果该页面已经被修改，则本函数会把页面的数据写回磁盘。
 * 如果给定的页面不在内存中，则返回 `false`。
 *
 * 在本函数中你 **不应该** 对页面加锁。  
 * 这意味着你需要仔细考虑在什么时候修改 `is_dirty_` 标志位。
 *
 * ### 实现提示
 *
 * 建议你在完成 `CheckedReadPage` 和 `CheckedWritePage` 后，再来实现这个函数，
 * 这样会更容易理解该怎么写。
 *
 * @param page_id 需要被刷新的页面 ID。
 * @return 如果该页面在页表中找不到，则返回 `false`；否则返回 `true`。
 */
auto BufferPoolManager::FlushPageUnsafe(page_id_t page_id) -> bool {
  if (page_table_.find(page_id) == page_table_.end()) {
    return false;
  }
  frame_id_t frame_id = page_table_[page_id];
  auto frame = frames_[frame_id];
  if (!frame->is_dirty_) {
    return true;
  }
  UpdateFrame(frame_id, page_id, true);
  return true;
}

/**
 * @brief Flushes a page's data out to disk safely.
 *
 * This function will write out a page's data to disk if it has been modified. If the given page is not in memory, this
 * function will return `false`.
 *
 * You should take a lock on the page in this function to ensure that a consistent state is flushed to disk.
 *
 * ### Implementation
 *
 * You should probably leave implementing this function until after you have completed `CheckedReadPage`,
 * `CheckedWritePage`, and `Flush` in the page guards, as it will likely be much easier to understand what to do.
 *
 * TODO(P1): Add implementation
 *
 * @param page_id The page ID of the page to be flushed.
 * @return `false` if the page could not be found in the page table, otherwise `true`.
 */
 /**
 * @brief 安全地将某个页面的数据刷新到磁盘。
 *
 * 如果该页面已经被修改，则本函数会把页面的数据写回磁盘。
 * 如果给定的页面不在内存中，则返回 `false`。
 *
 * 在本函数中你需要对页面加锁，以确保写回磁盘的是一致状态的数据。
 *
 * ### 实现提示
 *
 * 建议你在完成 `CheckedReadPage`、`CheckedWritePage` 和页面保护对象（page guards）中的 `Flush` 
 * 之后，再来实现这个函数，这样会更容易理解该怎么写。
 *
 * @param page_id 需要被刷新的页面 ID。
 * @return 如果该页面在页表中找不到，则返回 `false`；否则返回 `true`。
 */
auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool {
  std::scoped_lock<std::mutex> lock(*bpm_latch_);
  if (page_table_.find(page_id) == page_table_.end()) {
    return false;
  }
  frame_id_t frame_id = page_table_[page_id];
  auto frame = frames_[frame_id];
  if (!frame->is_dirty_) {
    return true;
  }
  UpdateFrame(frame_id, page_id, true);
  return true;
}

/**
 * @brief Flushes all page data that is in memory to disk unsafely.
 *
 * You should not take locks on the pages in this function.
 * This means that you should carefully consider when to toggle the `is_dirty_` bit.
 *
 * ### Implementation
 *
 * You should probably leave implementing this function until after you have completed `CheckedReadPage`,
 * `CheckedWritePage`, and `FlushPage`, as it will likely be much easier to understand what to do.
 *
 * TODO(P1): Add implementation
 */
 /**
 * @brief 不安全地将内存中的所有页面数据刷新到磁盘。
 *
 * 在这个函数中你**不应该**对页面加锁。  
 * 这意味着你需要仔细考虑在什么时候修改 `is_dirty_` 标志位。
 *
 * ### 实现提示
 *
 * 你最好在完成 `CheckedReadPage`、`CheckedWritePage` 和 `FlushPage` 之后，再来实现这个函数，
 * 因为那时你会更容易理解应该如何实现它。
 *
 * TODO(P1): 添加实现
 */
void BufferPoolManager::FlushAllPagesUnsafe() {
  for (auto [page_id, frame_id] : page_table_) {
    auto frame = frames_[frame_id];
    if (!frame->is_dirty_) {
      continue;
    }
    UpdateFrame(frame_id, page_id, true);
  }
}

/**
 * @brief Flushes all page data that is in memory to disk safely.
 *
 * You should take locks on the pages in this function to ensure that a consistent state is flushed to disk.
 *
 * ### Implementation
 *
 * You should probably leave implementing this function until after you have completed `CheckedReadPage`,
 * `CheckedWritePage`, and `FlushPage`, as it will likely be much easier to understand what to do.
 *
 * TODO(P1): Add implementation
 */
void BufferPoolManager::FlushAllPages() {
  std::scoped_lock<std::mutex> lock(*bpm_latch_);
  for (auto [page_id, frame_id] : page_table_) {
    auto frame = frames_[frame_id];
    if (!frame->is_dirty_) {
      continue;
    }
    UpdateFrame(frame_id, page_id, true);
  }
}

/**
 * @brief Retrieves the pin count of a page. If the page does not exist in memory, return `std::nullopt`.
 *
 * This function is thread safe. Callers may invoke this function in a multi-threaded environment where multiple threads
 * access the same page.
 *
 * This function is intended for testing purposes. If this function is implemented incorrectly, it will definitely cause
 * problems with the test suite and autograder.
 *
 * # Implementation
 *
 * We will use this function to test if your buffer pool manager is managing pin counts correctly. Since the
 * `pin_count_` field in `FrameHeader` is an atomic type, you do not need to take the latch on the frame that holds the
 * page we want to look at. Instead, you can simply use an atomic `load` to safely load the value stored. You will still
 * need to take the buffer pool latch, however.
 *
 * Again, if you are unfamiliar with atomic types, see the official C++ docs
 * [here](https://en.cppreference.com/w/cpp/atomic/atomic).
 *
 * TODO(P1): Add implementation
 *
 * @param page_id The page ID of the page we want to get the pin count of.
 * @return std::optional<size_t> The pin count if the page exists, otherwise `std::nullopt`.
 */
 /**
 * @brief 获取某个页面的 pin count。如果该页面不存在于内存中，则返回 `std::nullopt`。
 *
 * 该函数是线程安全的。调用者可以在多线程环境下调用该函数，即使多个线程同时访问同一个页面。
 *
 * 该函数主要用于测试目的。如果该函数实现错误，一定会导致测试用例和自动评分系统出问题。
 *
 * # 实现提示
 *
 * 我们会用这个函数来测试你的缓冲池管理器是否正确管理了 pin count。由于 `FrameHeader` 中的
 * `pin_count_` 字段是一个原子类型，你在读取目标页面所在 frame 的值时 **不需要**对该 frame 上锁。
 * 你只需要使用原子的 `load` 操作即可安全地读取存储的值。不过，你仍然需要获取缓冲池的全局锁。
 *
 * 如果你对原子类型不熟悉，可以参考 C++ 官方文档：
 * [atomic](https://en.cppreference.com/w/cpp/atomic/atomic)。
 *
 * TODO(P1): 需要补充实现
 *
 * @param page_id 想要获取 pin count 的页面 ID。
 * @return std::optional<size_t> 如果页面存在，返回 pin count，否则返回 `std::nullopt`。
 */
auto BufferPoolManager::GetPinCount(page_id_t page_id) -> std::optional<size_t> {
  if (page_table_.find(page_id) == page_table_.end()) {
    return std::nullopt;
  }
  frame_id_t frame_id = page_table_[page_id];
  auto frame = frames_[frame_id];
  return frame->pin_count_;
}

}  // namespace bustub
