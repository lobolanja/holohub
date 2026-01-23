/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include "test_helpers.h"
#include "connext_ano_lib/internal/cuda_resource_manager.h"

using namespace holoscan::ops;
using namespace connext_ano_lib::test;

// ============================================================================
// CudaStream RAII Tests
// ============================================================================

TEST_F(CudaTestFixture, CudaStream_Construction_CreatesValidHandle) {
  CudaStream stream;
  EXPECT_NE(nullptr, stream.get());
  
  // Verify stream is usable
  void* test_buffer = nullptr;
  ASSERT_CUDA_SUCCESS(cudaMalloc(&test_buffer, 1024));
  ASSERT_CUDA_SUCCESS(cudaMemsetAsync(test_buffer, 0, 1024, stream.get()));
  ASSERT_CUDA_SUCCESS(cudaStreamSynchronize(stream.get()));
  ASSERT_CUDA_SUCCESS(cudaFree(test_buffer));
}

TEST_F(CudaTestFixture, CudaStream_MoveConstructor_TransfersOwnership) {
  CudaStream stream1;
  cudaStream_t original_handle = stream1.get();
  ASSERT_NE(nullptr, original_handle);
  
  // Move construct stream2 from stream1
  CudaStream stream2(std::move(stream1));
  
  // stream2 should have the original handle
  EXPECT_EQ(original_handle, stream2.get());
  
  // stream1 should be null after move
  EXPECT_EQ(nullptr, stream1.get());
}

TEST_F(CudaTestFixture, CudaStream_MoveAssignment_TransfersOwnership) {
  CudaStream stream1;
  cudaStream_t original_handle = stream1.get();
  ASSERT_NE(nullptr, original_handle);
  
  CudaStream stream2;
  cudaStream_t stream2_original = stream2.get();
  ASSERT_NE(nullptr, stream2_original);
  
  // Move assign stream1 to stream2
  stream2 = std::move(stream1);
  
  // stream2 should have stream1's original handle
  EXPECT_EQ(original_handle, stream2.get());
  
  // stream1 should be null after move
  EXPECT_EQ(nullptr, stream1.get());
}

TEST_F(CudaTestFixture, CudaStream_Destructor_FreesResource) {
  cudaStream_t handle;
  {
    CudaStream stream;
    handle = stream.get();
    ASSERT_NE(nullptr, handle);
  }  // stream destroyed here
  
  // Attempting to use the handle should fail
  // Note: We can't directly verify destruction, but we can ensure no crashes
  cudaError_t err = cudaGetLastError();
  EXPECT_EQ(cudaSuccess, err);
}

TEST_F(CudaTestFixture, CudaStream_ImplicitConversion_WorksWithCudaAPIs) {
  CudaStream stream;
  
  // Implicit conversion to cudaStream_t should work
  void* test_buffer = nullptr;
  ASSERT_CUDA_SUCCESS(cudaMalloc(&test_buffer, 1024));
  ASSERT_CUDA_SUCCESS(cudaMemsetAsync(test_buffer, 0, 1024, stream));  // Implicit conversion
  ASSERT_CUDA_SUCCESS(cudaStreamSynchronize(stream));
  ASSERT_CUDA_SUCCESS(cudaFree(test_buffer));
}

// ============================================================================
// CudaEvent RAII Tests
// ============================================================================

TEST_F(CudaTestFixture, CudaEvent_Construction_CreatesValidHandle) {
  CudaEvent event;
  EXPECT_NE(nullptr, event.get());
  
  // Verify event is usable
  CudaStream stream;
  ASSERT_CUDA_SUCCESS(event.record(stream.get()));
  ASSERT_CUDA_SUCCESS(cudaEventSynchronize(event.get()));
}

TEST_F(CudaTestFixture, CudaEvent_MoveConstructor_TransfersOwnership) {
  CudaEvent event1;
  cudaEvent_t original_handle = event1.get();
  ASSERT_NE(nullptr, original_handle);
  
  // Move construct event2 from event1
  CudaEvent event2(std::move(event1));
  
  // event2 should have the original handle
  EXPECT_EQ(original_handle, event2.get());
  
  // event1 should be null after move
  EXPECT_EQ(nullptr, event1.get());
}

TEST_F(CudaTestFixture, CudaEvent_MoveAssignment_TransfersOwnership) {
  CudaEvent event1;
  cudaEvent_t original_handle = event1.get();
  ASSERT_NE(nullptr, original_handle);
  
  CudaEvent event2;
  cudaEvent_t event2_original = event2.get();
  ASSERT_NE(nullptr, event2_original);
  
  // Move assign event1 to event2
  event2 = std::move(event1);
  
  // event2 should have event1's original handle
  EXPECT_EQ(original_handle, event2.get());
  
  // event1 should be null after move
  EXPECT_EQ(nullptr, event1.get());
}

TEST_F(CudaTestFixture, CudaEvent_Destructor_FreesResource) {
  cudaEvent_t handle;
  {
    CudaEvent event;
    handle = event.get();
    ASSERT_NE(nullptr, handle);
  }  // event destroyed here
  
  cudaError_t err = cudaGetLastError();
  EXPECT_EQ(cudaSuccess, err);
}

TEST_F(CudaTestFixture, CudaEvent_QueryMethod_ReturnsCorrectStatus) {
  CudaEvent event;
  CudaStream stream;
  
  // Record event on stream
  ASSERT_CUDA_SUCCESS(event.record(stream.get()));
  
  // Query should eventually return cudaSuccess when complete
  ASSERT_CUDA_SUCCESS(cudaStreamSynchronize(stream.get()));
  cudaError_t status = event.query();
  EXPECT_EQ(cudaSuccess, status);
}

TEST_F(CudaTestFixture, CudaEvent_ImplicitConversion_WorksWithCudaAPIs) {
  CudaEvent event;
  CudaStream stream;
  
  // Implicit conversion to cudaEvent_t should work
  ASSERT_CUDA_SUCCESS(cudaEventRecord(event, stream));  // Implicit conversion
  ASSERT_CUDA_SUCCESS(cudaEventSynchronize(event));
}

// ============================================================================
// CudaBuffer RAII Tests
// ============================================================================

TEST_F(CudaTestFixture, CudaBuffer_Construction_CreatesValidBuffer) {
  const size_t size = 1024;
  CudaBuffer buffer(size);
  
  EXPECT_NE(nullptr, buffer.data());
  EXPECT_EQ(size, buffer.size());
  
  // Verify buffer is usable
  ASSERT_CUDA_SUCCESS(cudaMemset(buffer.data(), 0xAA, size));
}

TEST_F(CudaTestFixture, CudaBuffer_MoveConstructor_TransfersOwnership) {
  const size_t size = 2048;
  CudaBuffer buffer1(size);
  void* original_ptr = buffer1.data();
  ASSERT_NE(nullptr, original_ptr);
  
  // Move construct buffer2 from buffer1
  CudaBuffer buffer2(std::move(buffer1));
  
  // buffer2 should have the original pointer
  EXPECT_EQ(original_ptr, buffer2.data());
  EXPECT_EQ(size, buffer2.size());
  
  // buffer1 should be null after move
  EXPECT_EQ(nullptr, buffer1.data());
  EXPECT_EQ(0u, buffer1.size());
}

TEST_F(CudaTestFixture, CudaBuffer_MoveAssignment_TransfersOwnership) {
  const size_t size1 = 1024;
  const size_t size2 = 2048;
  
  CudaBuffer buffer1(size1);
  void* original_ptr = buffer1.data();
  ASSERT_NE(nullptr, original_ptr);
  
  CudaBuffer buffer2(size2);
  void* buffer2_original = buffer2.data();
  ASSERT_NE(nullptr, buffer2_original);
  
  // Move assign buffer1 to buffer2
  buffer2 = std::move(buffer1);
  
  // buffer2 should have buffer1's original pointer and size
  EXPECT_EQ(original_ptr, buffer2.data());
  EXPECT_EQ(size1, buffer2.size());
  
  // buffer1 should be null after move
  EXPECT_EQ(nullptr, buffer1.data());
  EXPECT_EQ(0u, buffer1.size());
}

TEST_F(CudaTestFixture, CudaBuffer_Destructor_FreesResource) {
  void* ptr;
  {
    CudaBuffer buffer(1024);
    ptr = buffer.data();
    ASSERT_NE(nullptr, ptr);
    
    // Write to buffer to ensure it's valid
    ASSERT_CUDA_SUCCESS(cudaMemset(ptr, 0xFF, 1024));
  }  // buffer destroyed here
  
  cudaError_t err = cudaGetLastError();
  EXPECT_EQ(cudaSuccess, err);
}

TEST_F(CudaTestFixture, CudaBuffer_LargeAllocation_Success) {
  // Allocate 100 MB
  const size_t large_size = 100 * 1024 * 1024;
  CudaBuffer buffer(large_size);
  
  EXPECT_NE(nullptr, buffer.data());
  EXPECT_EQ(large_size, buffer.size());
  
  // Verify we can write to it (spot check)
  ASSERT_CUDA_SUCCESS(cudaMemset(buffer.data(), 0x55, 1024));
}

TEST_F(CudaTestFixture, CudaBuffer_DataIntegrity_WriteAndRead) {
  const size_t size = 256;
  CudaBuffer buffer(size);
  
  // Create test pattern
  std::vector<uint8_t> test_data(size);
  for (size_t i = 0; i < size; ++i) {
    test_data[i] = static_cast<uint8_t>(i);
  }
  
  // Copy to GPU buffer
  ASSERT_CUDA_SUCCESS(cudaMemcpy(buffer.data(), test_data.data(), size,
                                  cudaMemcpyHostToDevice));
  
  // Read back
  std::vector<uint8_t> result_data(size);
  ASSERT_CUDA_SUCCESS(cudaMemcpy(result_data.data(), buffer.data(), size,
                                  cudaMemcpyDeviceToHost));
  
  // Verify
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(test_data[i], result_data[i]) << "Mismatch at byte " << i;
  }
}
