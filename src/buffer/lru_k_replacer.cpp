//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include "buffer/lru_k_replacer.h"
#include "algorithm"
#include "common/exception.h"
#include "optional"
namespace bustub {

/**
 *
 * TODO(P1): Add implementation
 *
 * @brief a new LRUKReplacer.
 * @param num_frames the maximum number of frames the LRUReplacer will be required to store
 */
LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

/**
 * TODO(P1): Add implementation
 *
 * @brief Find the frame with largest backward k-distance and evict that frame. Only frames
 * that are marked as 'evictable' are candidates for eviction.
 *
 * A frame with less than k historical references is given +inf as its backward k-distance.
 * If multiple frames have inf backward k-distance, then evict frame whose oldest timestamp
 * is furthest in the past.
 *
 * Successful eviction of a frame should decrement the size of replacer and remove the frame's
 * access history.
 *
 * @return true if a frame is evicted successfully, false if no frames can be evicted.
 */
auto LRUKReplacer::Evict(int *value) -> bool {
  std::lock_guard<std::mutex> guard(latch_);
  std::vector<frame_id_t> inf_candidates;
  for (auto &[frame_id, node] : node_store_) {
    if (node.is_evictable_ == false) {
      continue;
    }
    if (node.history_.size() < k_) {
      inf_candidates.emplace_back(frame_id);
    }
  }
  if (!inf_candidates.empty()) {
    auto victim_itere = std::min_element(inf_candidates.begin(), inf_candidates.end(), [&](frame_id_t a, frame_id_t b) {
      return node_store_[a].history_.front() < node_store_[b].history_.front();
    });

    frame_id_t victim_id = *victim_itere;
    *value = victim_id;
    node_store_.erase(victim_id);
    curr_size_--;
    return true;
  }

  frame_id_t victim_id = -1;
  size_t max_distance = 0;
  bool found = false;

  for (const auto &[frame_id, node] : node_store_) {
    if (node.is_evictable_ == false) {
      continue;
    }
    size_t kth_time = 0;
    auto it = node.history_.begin();
    std::advance(it, node.history_.size() - k_);
    kth_time = *it;
    size_t distance = current_timestamp_ - kth_time;

    if (!found || distance > max_distance) {
      victim_id = frame_id;
      max_distance = distance;
      found = true;
    }
  }

  if (found) {
    *value = node_store_[victim_id].fid_;
    node_store_.erase(victim_id);
    curr_size_--;
    return true;
  }

  return false;
}
/**
 * TODO(P1): Add implementation
 *
 * @brief Record the event that the given frame id is accessed at current timestamp.
 * Create a new entry for access history if frame id has not been seen before.
 *
 * If frame id is invalid (ie. larger than replacer_size_), throw an exception. You can
 * also use BUSTUB_ASSERT to abort the process if frame id is invalid.
 *
 * @param frame_id id of frame that received a new access.
 * @param access_type type of access that was received. This parameter is only needed for
 * leaderboard tests.
 */
void LRUKReplacer::RecordAccess(frame_id_t frame_id, [[maybe_unused]] AccessType access_type) {
  if (static_cast<size_t>(frame_id) >= replacer_size_) {
    throw std::invalid_argument("frame_id is out of range");
  }

  auto &node = node_store_[frame_id];
  if (!node.is_evictable_.has_value()) {
    node.is_evictable_ = false;  // 初始设置为不可驱逐
  }
  node.fid_ = frame_id;

  node_store_[frame_id].history_.push_back(current_timestamp_++);
  if (node_store_[frame_id].history_.size() > k_) {
    node_store_[frame_id].history_.pop_front();
  }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Toggle whether a frame is evictable or non-evictable. This function also
 * controls replacer's size. Note that size is equal to number of evictable entries.
 *
 * If a frame was previously evictable and is to be set to non-evictable, then size should
 * decrement. If a frame was previously non-evictable and is to be set to evictable,
 * then size should increment.
 *
 * If frame id is invalid, throw an exception or abort the process.
 *
 * For other scenarios, this function should terminate without modifying anything.
 *
 * @param frame_id id of frame whose 'evictable' status will be modified
 * @param set_evictable whether the given frame is evictable or not
 */
void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  if (node_store_.find(frame_id) == node_store_.end()) {
    return;
  }

  auto &node = node_store_[frame_id];
  if (!node.is_evictable_.has_value()) {
    node.is_evictable_ = false;  // 初始设置为不可驱逐
  }
  node.fid_ = frame_id;

  bool prev_evictable = node.is_evictable_.value();

  // 如果状态没有发生变化，不做任何事
  if (prev_evictable == set_evictable) {
    return;
  }

  // 状态发生变化，更新 curr_size_
  if (set_evictable) {
    curr_size_++;
  } else {
    curr_size_--;
  }

  node.is_evictable_ = set_evictable;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Remove an evictable frame from replacer, along with its access history.
 * This function should also decrement replacer's size if removal is successful.
 *
 * Note that this is different from evicting a frame, which always remove the frame
 * with largest backward k-distance. This function removes specified frame id,
 * no matter what its backward k-distance is.
 *
 * If Remove is called on a non-evictable frame, throw an exception or abort the
 * process.
 *
 * If specified frame is not found, directly return from this function.
 *
 * @param frame_id id of frame to be removed
 */
void LRUKReplacer::Remove(frame_id_t frame_id) {
  if (frame_id >= static_cast<frame_id_t>(node_store_.size())) {
    throw std::invalid_argument("Invalid frame_id");
  }
  if (!node_store_[frame_id].is_evictable_) {
    throw std::invalid_argument("Invalid frame_id");
  }
  auto &node = node_store_[frame_id];

  // 3. is_evictable_ 未初始化或为 false，则抛异常
  if (!node.is_evictable_.has_value() || node.is_evictable_.value() == false) {
    throw std::runtime_error("Attempt to remove non-evictable frame");
  }

  // 4. 减少 curr_size_ 并删除 node
  curr_size_--;
  node_store_.erase(frame_id);
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Return replacer's size, which tracks the number of evictable frames.
 *
 * @return size_t
 */
auto LRUKReplacer::Size() -> size_t { return curr_size_; }

}  // namespace bustub
