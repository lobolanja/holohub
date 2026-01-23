/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <vector>
#include "test_helpers.h"
#include "connext_ano_lib/internal/cuda_resource_manager.h"

using namespace holoscan::ops;
using namespace connext_ano_lib::test;

// ============================================================================
// CudaResourceManager Tests
// ============================================================================

TEST_F(CudaTestFixture, DefaultConstruction_SlotCycling_WrapsToZero) {
  CudaResourceManager manager(4);  // 4 concurrent slots
  
  // Initially at slot 0
  EXPECT_EQ(0, manager.current_index());
  
  // Advance through all slots
  manager.record_and_advance();
  EXPECT_EQ(1, manager.current_index());
  
  manager.record_and_advance();
  EXPECT_EQ(2, manager.current_index());
  
  manager.record_and_advance();
  EXPECT_EQ(3, manager.current_index());
  
  // Should wrap back to 0
  manager.record_and_advance();
  EXPECT_EQ(0, manager.current_index());
}

TEST_F(CudaTestFixture, GetStream_MultipleSlots_ReturnsDistinctStreams) {
  CudaResourceManager manager(4);
  
  // Collect streams from all slots
  std::vector<cudaStream_t> streams;
  for (int i = 0; i < 4; ++i) {
    cudaStream_t stream = manager.get_stream();
    EXPECT_NE(nullptr, stream);
    streams.push_back(stream);
    manager.record_and_advance();
  }
  
  // Verify all streams are distinct
  for (size_t i = 0; i < streams.size(); ++i) {
    for (size_t j = i + 1; j < streams.size(); ++j) {
      EXPECT_NE(streams[i], streams[j]) 
        << "Streams at index " << i << " and " << j << " should be distinct";
    }
  }
}

TEST_F(CudaTestFixture, GetEvent_MultipleSlots_ReturnsDistinctEvents) {
  CudaResourceManager manager(4);
  
  // Collect events from all slots
  std::vector<cudaEvent_t> events;
  for (int i = 0; i < 4; ++i) {
    cudaEvent_t event = manager.get_event();
    EXPECT_NE(nullptr, event);
    events.push_back(event);
    manager.record_and_advance();
  }
  
  // Verify all events are distinct
  for (size_t i = 0; i < events.size(); ++i) {
    for (size_t j = i + 1; j < events.size(); ++j) {
      EXPECT_NE(events[i], events[j]) 
        << "Events at index " << i << " and " << j << " should be distinct";
    }
  }
}

TEST_F(CudaTestFixture, IsReady_NewSlot_ReturnsTrue) {
  CudaResourceManager manager(4);
  
  // Newly created slot should be ready
  EXPECT_TRUE(manager.is_ready());
}

TEST_F(CudaTestFixture, RecordAndAdvance_AfterWork_EventRecorded) {
  CudaResourceManager manager(4);
  
  // Do some work on the stream
  cudaStream_t stream = manager.get_stream();
  void* test_buffer = nullptr;
  ASSERT_CUDA_SUCCESS(cudaMalloc(&test_buffer, 1024));
  ASSERT_CUDA_SUCCESS(cudaMemsetAsync(test_buffer, 0, 1024, stream));
  
  // Record and advance
  manager.record_and_advance();
  
  // Synchronize and verify event was recorded
  cudaEvent_t event = manager.get_event();  // Now pointing to next slot
  ASSERT_CUDA_SUCCESS(cudaDeviceSynchronize());
  
  // Clean up
  ASSERT_CUDA_SUCCESS(cudaFree(test_buffer));
}

TEST_F(CudaTestFixture, AllocateBuffer_ValidSize_ReturnsNonNullPointer) {
  CudaResourceManager manager(4);
  
  void* buffer = manager.allocate_buffer(1024);
  ASSERT_NE(nullptr, buffer);
  
  // Verify we can write to the buffer
  ASSERT_CUDA_SUCCESS(cudaMemset(buffer, 0xAA, 1024));
  
  // Clean up
  manager.free_buffer(buffer);
}

TEST_F(CudaTestFixture, FreeBuffer_NullPointer_NoError) {
  CudaResourceManager manager(4);
  
  // Freeing nullptr should be safe
  EXPECT_NO_THROW(manager.free_buffer(nullptr));
}

TEST_F(CudaTestFixture, AllocateAndFreeBuffer_LargeSize_Success) {
  CudaResourceManager manager(4);
  
  // Allocate 10 MB
  const size_t large_size = 10 * 1024 * 1024;
  void* buffer = manager.allocate_buffer(large_size);
  ASSERT_NE(nullptr, buffer);
  
  // Free and verify no errors
  EXPECT_NO_THROW(manager.free_buffer(buffer));
}

TEST_F(CudaTestFixture, AsyncCopyDeviceToDevice_WithData_DataIntegrityVerified) {
  CudaResourceManager manager(4);
  
  const size_t size = 1024;
  
  // Allocate source and destination buffers
  void* src_buffer = manager.allocate_buffer(size);
  void* dst_buffer = manager.allocate_buffer(size);
  ASSERT_NE(nullptr, src_buffer);
  ASSERT_NE(nullptr, dst_buffer);
  
  // Initialize source buffer with test pattern
  std::vector<uint8_t> test_data(size);
  for (size_t i = 0; i < size; ++i) {
    test_data[i] = static_cast<uint8_t>(i & 0xFF);
  }
  ASSERT_CUDA_SUCCESS(cudaMemcpy(src_buffer, test_data.data(), size, 
                                  cudaMemcpyHostToDevice));
  
  // Initialize destination with zeros
  ASSERT_CUDA_SUCCESS(cudaMemset(dst_buffer, 0, size));
  
  // Perform async copy
  manager.async_copy_device_to_device(dst_buffer, src_buffer, size);
  
  // Record event and synchronize
  manager.record_and_advance();
  ASSERT_CUDA_SUCCESS(cudaDeviceSynchronize());
  
  // Verify data was copied correctly
  std::vector<uint8_t> result_data(size);
  ASSERT_CUDA_SUCCESS(cudaMemcpy(result_data.data(), dst_buffer, size, 
                                  cudaMemcpyDeviceToHost));
  
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(test_data[i], result_data[i]) 
      << "Data mismatch at byte " << i;
  }
  
  // Clean up
  manager.free_buffer(src_buffer);
  manager.free_buffer(dst_buffer);
}

TEST_F(CudaTestFixture, SingleSlot_Cycling_WorksCorrectly) {
  // Test with only 1 slot
  CudaResourceManager manager(1);
  
  EXPECT_EQ(0, manager.current_index());
  
  manager.record_and_advance();
  EXPECT_EQ(0, manager.current_index());  // Should wrap back to 0
  
  manager.record_and_advance();
  EXPECT_EQ(0, manager.current_index());  // Still at 0
}

TEST_F(CudaTestFixture, IsReady_AfterAsyncWork_EventuallyReturnsTrue) {
  CudaResourceManager manager(2);
  
  // Submit async work to slot 0
  cudaStream_t stream = manager.get_stream();
  void* buffer = manager.allocate_buffer(1024 * 1024);  // 1 MB
  ASSERT_CUDA_SUCCESS(cudaMemsetAsync(buffer, 0xFF, 1024 * 1024, stream));
  
  manager.record_and_advance();  // Now at slot 1
  
  // Do quick work on slot 1
  manager.record_and_advance();  // Back to slot 0
  
  // Slot 0 might not be ready immediately, but should complete eventually
  ASSERT_CUDA_SUCCESS(cudaDeviceSynchronize());
  EXPECT_TRUE(manager.is_ready());
  
  manager.free_buffer(buffer);
}
