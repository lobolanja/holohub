/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../include/cuda_resource_manager.h"

namespace holoscan::ops {

CudaResourceManager::CudaResourceManager(int num_concurrent)
    : num_concurrent_(num_concurrent > 0 ? num_concurrent : DEFAULT_CONCURRENT) {
  streams_.reserve(num_concurrent_);
  events_.reserve(num_concurrent_);
  
  for (int i = 0; i < num_concurrent_; i++) {
    streams_.emplace_back();
    events_.emplace_back();
  }
}

bool CudaResourceManager::is_ready() const {
  return events_[cur_idx_].query() == cudaSuccess;
}

cudaStream_t CudaResourceManager::get_stream() const {
  return streams_[cur_idx_].get();
}

cudaEvent_t CudaResourceManager::get_event() const {
  return events_[cur_idx_].get();
}

void CudaResourceManager::record_and_advance() {
  events_[cur_idx_].record(streams_[cur_idx_].get());
  cur_idx_ = (cur_idx_ + 1) % num_concurrent_;
}

}  // namespace holoscan::ops
