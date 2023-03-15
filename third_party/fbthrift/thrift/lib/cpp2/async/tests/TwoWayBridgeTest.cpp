/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <thrift/lib/cpp2/async/TwoWayBridge.h>

#include <folly/portability/GTest.h>

#include <folly/synchronization/Baton.h>

namespace apache {
namespace thrift {

TEST(AtomicQueueTest, Basic) {
  folly::Baton<> producerBaton;
  folly::Baton<> consumerBaton;

  struct Consumer {
    void consume() { baton.post(); }
    void canceled() { ADD_FAILURE() << "canceled() shouldn't be called"; }
    folly::Baton<> baton;
  };
  detail::twowaybridge_detail::AtomicQueue<Consumer, int> atomicQueue;
  Consumer consumer;

  std::thread producerThread([&] {
    producerBaton.wait();
    producerBaton.reset();

    atomicQueue.push(1);

    producerBaton.wait();
    producerBaton.reset();

    atomicQueue.push(2);
    atomicQueue.push(3);
    consumerBaton.post();
  });

  EXPECT_TRUE(atomicQueue.wait(&consumer));
  producerBaton.post();
  consumer.baton.wait();
  consumer.baton.reset();

  {
    auto q = atomicQueue.getMessages();
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(1, q.front());
    q.pop();
    EXPECT_TRUE(q.empty());
  }

  producerBaton.post();
  consumerBaton.wait();
  consumerBaton.reset();

  EXPECT_FALSE(atomicQueue.wait(&consumer));
  {
    auto q = atomicQueue.getMessages();
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(2, q.front());
    q.pop();
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(3, q.front());
    q.pop();
    EXPECT_TRUE(q.empty());
  }

  EXPECT_TRUE(atomicQueue.wait(&consumer));
  EXPECT_EQ(atomicQueue.cancelCallback(), &consumer);

  EXPECT_TRUE(atomicQueue.wait(&consumer));
  EXPECT_EQ(atomicQueue.cancelCallback(), &consumer);

  EXPECT_EQ(atomicQueue.cancelCallback(), nullptr);

  producerThread.join();
}

TEST(AtomicQueueTest, Canceled) {
  struct Consumer {
    void consume() { ADD_FAILURE() << "consume() shouldn't be called"; }
    void canceled() { canceledCalled = true; }
    bool canceledCalled{false};
  };
  detail::twowaybridge_detail::AtomicQueue<Consumer, int> atomicQueue;
  Consumer consumer;

  EXPECT_TRUE(atomicQueue.wait(&consumer));
  atomicQueue.close();
  EXPECT_TRUE(consumer.canceledCalled);
  EXPECT_TRUE(atomicQueue.isClosed());

  EXPECT_TRUE(atomicQueue.getMessages().empty());
  EXPECT_TRUE(atomicQueue.isClosed());

  atomicQueue.push(42);

  EXPECT_TRUE(atomicQueue.getMessages().empty());
  EXPECT_TRUE(atomicQueue.isClosed());
}

TEST(AtomicQueueTest, Stress) {
  struct Consumer {
    void consume() { baton.post(); }
    void canceled() { ADD_FAILURE() << "canceled() shouldn't be called"; }
    folly::Baton<> baton;
  };
  detail::twowaybridge_detail::AtomicQueue<Consumer, int> atomicQueue;
  auto getNext = [&atomicQueue,
                  queue = detail::twowaybridge_detail::Queue<int>()]() mutable {
    Consumer consumer;
    if (queue.empty()) {
      if (atomicQueue.wait(&consumer)) {
        consumer.baton.wait();
      }
      queue = atomicQueue.getMessages();
      EXPECT_FALSE(queue.empty());
    }
    auto next = queue.front();
    queue.pop();
    return next;
  };

  constexpr ssize_t kNumIters = 100000;
  constexpr ssize_t kSynchronizeEvery = 1000;

  std::atomic<ssize_t> producerIndex{0};
  std::atomic<ssize_t> consumerIndex{0};

  std::thread producerThread([&] {
    for (producerIndex = 1; producerIndex <= kNumIters; ++producerIndex) {
      atomicQueue.push(producerIndex);

      if (producerIndex % kSynchronizeEvery == 0) {
        while (producerIndex > consumerIndex.load(std::memory_order_relaxed)) {
          std::this_thread::yield();
        }
      }
    }
  });

  for (consumerIndex = 1; consumerIndex <= kNumIters; ++consumerIndex) {
    EXPECT_EQ(consumerIndex, getNext());

    if (consumerIndex % kSynchronizeEvery == 0) {
      while (consumerIndex > producerIndex.load(std::memory_order_relaxed)) {
        std::this_thread::yield();
      }
    }
  }

  producerThread.join();
}

} // namespace thrift
} // namespace apache
