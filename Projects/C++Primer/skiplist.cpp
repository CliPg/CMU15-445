//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// skiplist.cpp
//
// Identification: src/primer/skiplist.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/skiplist.h"
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "common/macros.h"
#include "fmt/core.h"
#include <mutex>

std::mutex mutex_;


namespace bustub {

/** @brief Checks whether the container is empty. */
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::Empty() -> bool {
  return header_->links_[0] == nullptr;
}

/** @brief Returns the number of elements in the skip list. */
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::Size() -> size_t {
  return size_;
}

/**
 * @brief Iteratively deallocate all the nodes.
 *
 * We do this to avoid stack overflow when the skip list is large.
 *
 * If we let the compiler handle the deallocation, it will recursively call the destructor of each node,
 * which could block up the the stack.
 */
SKIPLIST_TEMPLATE_ARGUMENTS void SkipList<K, Compare, MaxHeight, Seed>::Drop() {
  for (size_t i = 0; i < MaxHeight; i++) {
    auto curr = std::move(header_->links_[i]);
    while (curr != nullptr) {
      // std::move sets `curr` to the old value of `curr->links_[i]`,
      // and then resets `curr->links_[i]` to `nullptr`.
      curr = std::move(curr->links_[i]);
    }
  }
}

/**
 * @brief Removes all elements from the skip list.
 *
 * Note: You might want to use the provided `Drop` helper function.
 */
SKIPLIST_TEMPLATE_ARGUMENTS void SkipList<K, Compare, MaxHeight, Seed>::Clear() {
  Drop();
  header_ = std::make_shared<SkipNode>(MaxHeight);
  height_ = 1;
  size_ = 0;
}

/**
 * @brief Inserts a key into the skip list.
 *
 * Note: `Insert` will not insert the key if it already exists in the skip list.
 *
 * @param key key to insert.
 * @return true if the insertion is successful, false if the key already exists.
 */
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::Insert(const K &key) -> bool {

  // 上锁，实现并发操作
  std::lock_guard<std::mutex> lock(mutex_);

  if (Contains(key)) {
    return false;
  }

  std::shared_ptr<SkipNode> curr = header_;
  std::vector<std::shared_ptr<SkipNode>> update(MaxHeight);

  for (int level = static_cast<int>(height_) - 1; level >= 0; level--) {
    while (true) {
      auto next = curr->Next(level);
      if (next == nullptr || compare_(key, next->Key())) {
        update[level] = curr;
        break;
      }
      curr = next;
    }
  }

  size_t node_height = RandomHeight();

  if (node_height > height_) {
    for (size_t i = height_; i < node_height; i++) {
      update[i] = header_;
    }
    height_ = static_cast<uint32_t>(node_height);
  }

  auto new_node = std::make_shared<SkipNode>(node_height, key);

  for (size_t i = 0; i < node_height; i++) {
    new_node->SetNext(i, update[i]->Next(i));
    update[i]->SetNext(i, new_node);
  }

  size_++;
  return true;
}

/**
 * @brief Erases the key from the skip list.
 *
 * @param key key to erase.
 * @return bool true if the element got erased, false otherwise.
 */
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::Erase(const K &key) -> bool {

  std::lock_guard<std::mutex> lock(mutex_);

  std::shared_ptr<SkipNode> curr = header_;
  std::vector<std::shared_ptr<SkipNode>> update(MaxHeight);

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
  
  curr = curr->Next(0);
  //curr = update[0];
  
  if (curr == nullptr || compare_(key, curr->Key()) || compare_(curr->Key(), key)) {
    return false;
  }


  for (size_t i = 0; i < curr->Height(); i++) {
    if (update[i]->Next(i) == curr) {
      update[i]->SetNext(i, curr->Next(i));
    }
  }

  while (height_ > 1 && header_->Next(height_ - 1) == nullptr) {
    height_--;
  }

  size_--;
  return true;
}

/**
 * @brief Checks whether a key exists in the skip list.
 *
 * @param key key to look up.
 * @return bool true if the element exists, false otherwise.
 */
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::Contains(const K &key) -> bool {
  // Following the standard library: Key `a` and `b` are considered equivalent if neither compares less
  // than the other: `!compare_(a, b) && !compare_(b, a)`.

  auto curr = header_;

  for (int level = static_cast<int>(height_) - 1; level >= 0; level--) {
    while (true) {
      auto next = curr->Next(level);
      if (next == nullptr || compare_(key, next->Key())) {
        break;
      }
      if (!(compare_(key, next->Key()) || compare_(next->Key(), key))) {
        return true;
      }
      curr = next;
    }
  }
  return false;
}

/**
 * @brief Prints the skip list for debugging purposes.
 *
 * Note: You may modify the functions in any way and the output is not tested.
 */
SKIPLIST_TEMPLATE_ARGUMENTS void SkipList<K, Compare, MaxHeight, Seed>::Print() {
  auto node = header_->Next(LOWEST_LEVEL);
  while (node != nullptr) {
    fmt::println("Node {{ key: {}, height: {} }}", node->Key(), node->Height());
    node = node->Next(LOWEST_LEVEL);
  }
}

/**
 * @brief Generate a random height. The height should be cappped at `MaxHeight`.
 * Note: we implement/simulate the geometric process to ensure platform independence.
 */
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::RandomHeight() -> size_t {
  // Branching factor (1 in 4 chance), see Pugh's paper.
  static constexpr unsigned int branching_factor = 4;
  // Start with the minimum height
  size_t height = 1;
  while (height < MaxHeight && (rng_() % branching_factor == 0)) {
    height++;
  }
  return height;
}

/**
 * @brief Gets the current node height.
 */
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::SkipNode::Height() const -> size_t {
  return links_.size();
}

/**
 * @brief Gets the next node by following the link at `level`.
 *
 * @param level index to the link.
 * @return std::shared_ptr<SkipNode> the next node, or `nullptr` if such node does not exist.
 */
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::SkipNode::Next(size_t level) const
    -> std::shared_ptr<SkipNode> {
      if (level >= links_.size()) {
        return nullptr;
      }
      return links_[level];
}

/**
 * @brief Set the `node` to be linked at `level`.
 *
 * @param level index to the link.
 */
SKIPLIST_TEMPLATE_ARGUMENTS void SkipList<K, Compare, MaxHeight, Seed>::SkipNode::SetNext(
    size_t level, const std::shared_ptr<SkipNode> &node) {
      if (level >= links_.size()) {
        return;
      }
      links_[level] = node;
}

/** @brief Returns a reference to the key stored in the node. */
SKIPLIST_TEMPLATE_ARGUMENTS auto SkipList<K, Compare, MaxHeight, Seed>::SkipNode::Key() const -> const K & {
  return key_ ;
}

// Below are explicit instantiation of template classes.
template class SkipList<int>;
template class SkipList<std::string>;
template class SkipList<int, std::greater<>>;
template class SkipList<int, std::less<>, 8>;

}  // namespace bustub
