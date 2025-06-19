#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <numeric>
#include <chrono>
#include <random>
#include "../src/RingBuffer.h"

using namespace KhDetector;

class RingBufferTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize test data
        testData.resize(100);
        std::iota(testData.begin(), testData.end(), 1);
    }

    void TearDown() override
    {
        // Clean up
    }

    std::vector<int> testData;
};

// Test basic functionality with power-of-2 sizes
TEST_F(RingBufferTest, BasicFunctionality_PowerOf2)
{
    RingBuffer<int, 8> buffer;
    
    // Test initial state
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(buffer.capacity(), 7); // Size - 1
    
    // Test push and pop
    EXPECT_TRUE(buffer.push(42));
    EXPECT_FALSE(buffer.empty());
    EXPECT_EQ(buffer.size(), 1);
    
    int value = 0;
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0);
}

TEST_F(RingBufferTest, FillAndEmpty)
{
    RingBuffer<int, 8> buffer;
    
    // Fill the buffer
    for (int i = 0; i < 7; ++i) { // capacity is 7
        EXPECT_TRUE(buffer.push(i));
    }
    
    EXPECT_TRUE(buffer.full());
    EXPECT_FALSE(buffer.push(999)); // Should fail when full
    
    // Empty the buffer
    for (int i = 0; i < 7; ++i) {
        int value = -1;
        EXPECT_TRUE(buffer.pop(value));
        EXPECT_EQ(value, i);
    }
    
    EXPECT_TRUE(buffer.empty());
    int dummy;
    EXPECT_FALSE(buffer.pop(dummy)); // Should fail when empty
}

TEST_F(RingBufferTest, PeekFunctionality)
{
    RingBuffer<int, 8> buffer;
    
    // Test peek on empty buffer
    int value = -1;
    EXPECT_FALSE(buffer.peek(value));
    
    // Add some data and test peek
    EXPECT_TRUE(buffer.push(123));
    EXPECT_TRUE(buffer.push(456));
    
    // Peek should return first element without removing it
    EXPECT_TRUE(buffer.peek(value));
    EXPECT_EQ(value, 123);
    EXPECT_EQ(buffer.size(), 2); // Should still have 2 elements
    
    // Pop should return the same element
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 123);
    EXPECT_EQ(buffer.size(), 1);
    
    // Next peek should return second element
    EXPECT_TRUE(buffer.peek(value));
    EXPECT_EQ(value, 456);
}

TEST_F(RingBufferTest, MoveSemantics)
{
    RingBuffer<std::unique_ptr<int>, 8> buffer;
    
    // Test move push
    auto ptr = std::make_unique<int>(42);
    EXPECT_TRUE(buffer.push(std::move(ptr)));
    EXPECT_EQ(ptr, nullptr); // Should be moved
    
    // Test pop
    std::unique_ptr<int> result;
    EXPECT_TRUE(buffer.pop(result));
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(*result, 42);
}

TEST_F(RingBufferTest, BulkOperations)
{
    RingBuffer<int, 16> buffer;
    
    // Test bulk push
    std::vector<int> pushData = {1, 2, 3, 4, 5};
    size_t pushed = buffer.push_bulk(pushData.data(), pushData.size());
    EXPECT_EQ(pushed, 5);
    EXPECT_EQ(buffer.size(), 5);
    
    // Test bulk pop
    std::vector<int> popData(10);
    size_t popped = buffer.pop_bulk(popData.data(), popData.size());
    EXPECT_EQ(popped, 5);
    
    for (size_t i = 0; i < popped; ++i) {
        EXPECT_EQ(popData[i], pushData[i]);
    }
    
    EXPECT_TRUE(buffer.empty());
}

TEST_F(RingBufferTest, BulkOperations_Overflow)
{
    RingBuffer<int, 8> buffer; // capacity = 7
    
    // Try to push more than capacity
    std::vector<int> pushData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    size_t pushed = buffer.push_bulk(pushData.data(), pushData.size());
    EXPECT_EQ(pushed, 7); // Should only push capacity amount
    EXPECT_TRUE(buffer.full());
    
    // Pop some data
    std::vector<int> popData(3);
    size_t popped = buffer.pop_bulk(popData.data(), 3);
    EXPECT_EQ(popped, 3);
    
    // Now we should be able to push 3 more
    pushed = buffer.push_bulk(pushData.data() + 7, 3);
    EXPECT_EQ(pushed, 3);
    EXPECT_TRUE(buffer.full());
}

TEST_F(RingBufferTest, ClearOperation)
{
    RingBuffer<int, 8> buffer;
    
    // Add some data
    for (int i = 0; i < 5; ++i) {
        buffer.push(i);
    }
    
    EXPECT_EQ(buffer.size(), 5);
    EXPECT_FALSE(buffer.empty());
    
    // Clear the buffer
    buffer.clear();
    
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
}

TEST_F(RingBufferTest, WrapAround)
{
    RingBuffer<int, 4> buffer; // capacity = 3
    
    // Fill buffer
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(buffer.push(i));
    }
    
    // Pop one element
    int value;
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 0);
    
    // Push another element (should wrap around)
    EXPECT_TRUE(buffer.push(100));
    
    // Pop remaining elements
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 100);
    
    EXPECT_TRUE(buffer.empty());
}

// Test with different data types
TEST_F(RingBufferTest, DifferentDataTypes)
{
    // Test with float
    RingBuffer<float, 8> floatBuffer;
    EXPECT_TRUE(floatBuffer.push(3.14f));
    float f;
    EXPECT_TRUE(floatBuffer.pop(f));
    EXPECT_FLOAT_EQ(f, 3.14f);
    
    // Test with struct
    struct TestStruct {
        int a;
        double b;
        bool operator==(const TestStruct& other) const {
            return a == other.a && b == other.b;
        }
    };
    
    RingBuffer<TestStruct, 8> structBuffer;
    TestStruct test{42, 3.14};
    EXPECT_TRUE(structBuffer.push(test));
    TestStruct result;
    EXPECT_TRUE(structBuffer.pop(result));
    EXPECT_EQ(result, test);
}

// Multi-threaded test
TEST_F(RingBufferTest, SingleProducerSingleConsumer)
{
    RingBuffer<int, 1024> buffer;
    const int numItems = 10000;
    std::vector<int> producedData(numItems);
    std::vector<int> consumedData;
    consumedData.reserve(numItems);
    
    // Initialize producer data
    std::iota(producedData.begin(), producedData.end(), 1);
    
    // Producer thread
    std::thread producer([&]() {
        for (int item : producedData) {
            while (!buffer.push(item)) {
                std::this_thread::yield();
            }
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int item;
        while (consumedData.size() < numItems) {
            if (buffer.pop(item)) {
                consumedData.push_back(item);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Verify data integrity
    EXPECT_EQ(consumedData.size(), numItems);
    EXPECT_EQ(producedData, consumedData);
}

// Stress test with random operations
TEST_F(RingBufferTest, StressTest)
{
    RingBuffer<int, 256> buffer;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    
    const int numOperations = 100000;
    int pushCount = 0;
    int popCount = 0;
    int value;
    
    for (int i = 0; i < numOperations; ++i) {
        if (dis(gen) == 0) { // Push
            if (buffer.push(i)) {
                pushCount++;
            }
        } else { // Pop
            if (buffer.pop(value)) {
                popCount++;
            }
        }
    }
    
    // Drain remaining items
    while (buffer.pop(value)) {
        popCount++;
    }
    
    EXPECT_EQ(pushCount, popCount);
    EXPECT_TRUE(buffer.empty());
}

// Test boundary conditions
TEST_F(RingBufferTest, BoundaryConditions)
{
    // Test with smallest possible size
    RingBuffer<int, 2> smallBuffer; // capacity = 1
    
    EXPECT_TRUE(smallBuffer.push(1));
    EXPECT_TRUE(smallBuffer.full());
    EXPECT_FALSE(smallBuffer.push(2));
    
    int value;
    EXPECT_TRUE(smallBuffer.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(smallBuffer.empty());
}

// Test error conditions
TEST_F(RingBufferTest, ErrorConditions)
{
    RingBuffer<int, 8> buffer;
    
    // Test bulk operations with null pointers
    EXPECT_EQ(buffer.push_bulk(nullptr, 5), 0);
    EXPECT_EQ(buffer.pop_bulk(nullptr, 5), 0);
    
    // Test bulk operations with zero count
    std::vector<int> data(5);
    EXPECT_EQ(buffer.push_bulk(data.data(), 0), 0);
    EXPECT_EQ(buffer.pop_bulk(data.data(), 0), 0);
} 