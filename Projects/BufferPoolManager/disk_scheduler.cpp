//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// disk_scheduler.cpp
//
// Identification: src/storage/disk/disk_scheduler.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/disk/disk_scheduler.h"
#include "common/exception.h"
#include "storage/disk/disk_manager.h"

// make disk_scheduler_test -j `nproc`
// ./test/disk_scheduler_test
namespace bustub {

DiskScheduler::DiskScheduler(DiskManager *disk_manager) : disk_manager_(disk_manager) {
  // TODO(P1): remove this line after you have implemented the disk scheduler API

  // Spawn the background thread
  background_thread_.emplace([&] { StartWorkerThread(); });
}

DiskScheduler::~DiskScheduler() {
  // Put a `std::nullopt` in the queue to signal to exit the loop
  request_queue_.Put(std::nullopt);
  if (background_thread_.has_value()) {
    background_thread_->join();
  }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Schedules a request for the DiskManager to execute.
 *
 * @param r The request to be scheduled.
 */
void DiskScheduler::Schedule(DiskRequest r) {
  // 不能直接put(r),同时还需要move r
  //  std::move(r) 是一种“类型转换”，把左值变成右值引用，从而触发对象的 移动语义，避免昂贵的拷贝，提高性能。
  request_queue_.Put(std::make_optional(std::move(r)));
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Background worker thread function that processes scheduled requests.
 *
 * The background thread needs to process requests while the DiskScheduler exists, i.e., this function should not
 * return until ~DiskScheduler() is called. At that point you need to make sure that the function does return.
 */
void DiskScheduler::StartWorkerThread() {
  while (true) {
    auto opt_r = request_queue_.Get();
    if (!opt_r.has_value()) {
      break;  // 队列关闭，退出循环
    }
    DiskRequest r = std::move(opt_r.value());

    if (r.is_write_) {
      disk_manager_->WritePage(r.page_id_, r.data_);
    } else {
      disk_manager_->ReadPage(r.page_id_, r.data_);
    }

    r.callback_.set_value(true);
  }
}


}  // namespace bustub
