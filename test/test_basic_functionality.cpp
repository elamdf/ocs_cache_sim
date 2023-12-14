#include <gtest/gtest.h>

#include "ocs_cache_sim/lib/basic_ocs_cache.h"
#include "ocs_cache_sim/lib/clock_eviction_ocs_cache.h"
#include "ocs_cache_sim/lib/far_memory_cache.h"
#include "ocs_cache_sim/lib/ocs_structs.h"

#define ASSERT_OK(expr) ASSERT_EQ(expr, OCSCache::Status::OK);

// Demonstrate some basic assertions.
TEST(BasicSuite, BasicBackingStoreFunctionality) {
    // TODO parameterize and hit all the OCSCache subclasses
  OCSCache *ocs_cache = new BasicOCSCache(
      /*num_pools=*/100, /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/1,
      /*max_conrreutn_backing_store_nodes*/ 100);
  std::vector<mem_access> accesses;
  bool hit;

  // We should miss on the first access
  ASSERT_OK(ocs_cache->handleMemoryAccess({1, 1}, &hit));
  ASSERT_FALSE(hit);
  EXPECT_EQ(ocs_cache->getPerformanceStats().accesses, 1);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_misses, 1);

  // The backing store should have cached the address
  ASSERT_OK(ocs_cache->handleMemoryAccess({1, 1}, &hit));
  ASSERT_TRUE(hit);
  EXPECT_EQ(ocs_cache->getPerformanceStats().accesses, 2);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_misses, 1);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_hits, 1);

  // The backing store size should be PAGE_SIZE big (with page-aligned base
  // address)
  ASSERT_OK(ocs_cache->handleMemoryAccess({100, 1}, &hit));
  ASSERT_TRUE(hit);
  EXPECT_EQ(ocs_cache->getPerformanceStats().accesses, 3);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_misses, 1);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_hits, 2);
}

// Demonstrate some basic assertions.
TEST(BasicSuite, BackingStorePageBoundary) {
  OCSCache *ocs_cache = new BasicOCSCache(
      /*num_pools=*/100, /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/1,
      /*max_conrreutn_backing_store_nodes*/ 100);
  std::vector<mem_access> accesses;
  bool hit;

  // We should miss on the first access
  ASSERT_OK(ocs_cache->handleMemoryAccess({1, 1}, &hit));
  ASSERT_FALSE(hit);
  EXPECT_EQ(ocs_cache->getPerformanceStats().accesses, 1);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_misses, 1);

  // The backing store should have cached the page
  ASSERT_OK(ocs_cache->handleMemoryAccess({PAGE_SIZE - 1, 1}, &hit));
  ASSERT_TRUE(hit);
  EXPECT_EQ(ocs_cache->getPerformanceStats().accesses, 2);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_misses, 1);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_hits, 1);

  // The backing store size should miss on an access that crosses a page
  // boundary when only one of the pages is materialized/cached.
  ASSERT_OK(ocs_cache->handleMemoryAccess({100, PAGE_SIZE}, &hit));
  ASSERT_FALSE(hit); // we haven't materialized the parent page of the end of
                     // this acccess
  EXPECT_EQ(ocs_cache->getPerformanceStats().accesses, 4);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_misses, 2);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_hits, 1);

  // The other page should be materialized + cached.
  ASSERT_OK(ocs_cache->handleMemoryAccess({100, PAGE_SIZE}, &hit));
  ASSERT_TRUE(hit); // we haven't materialized the parent page of the end of
                    // this acccess
  EXPECT_EQ(ocs_cache->getPerformanceStats().accesses, 6);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_misses, 2);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_hits, 3);
}

TEST(BasicSuite, TestClockPolicy) {
  ClockOCSCache *ocs_cache = new ClockOCSCache(
      /*num_pools=*/100, /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/1,
      /*max_conrreutn_backing_store_nodes*/ 2);
  std::vector<mem_access> accesses;
  perf_stats expected_stats;
  bool hit;

  // We should miss on the first access
  ASSERT_OK(ocs_cache->handleMemoryAccess({1, 1}, &hit));
  ASSERT_FALSE(hit);
  expected_stats.accesses++;
  expected_stats.backing_store_misses++;
  EXPECT_EQ(ocs_cache->getPerformanceStats(), expected_stats);

  // The backing store should have cached the page
  ASSERT_OK(ocs_cache->handleMemoryAccess({PAGE_SIZE - 1, 1}, &hit));
  ASSERT_TRUE(hit);
  expected_stats.accesses++;
  expected_stats.backing_store_hits++;
  EXPECT_EQ(ocs_cache->getPerformanceStats(), expected_stats);

  // Access a new page to fill the cache (size 2)
  ASSERT_OK(ocs_cache->handleMemoryAccess({PAGE_SIZE, 1}, &hit));
  ASSERT_FALSE(hit);
  expected_stats.accesses++;
  expected_stats.backing_store_misses++;
  EXPECT_EQ(ocs_cache->getPerformanceStats(), expected_stats);

  // Access the original page, it should still be in cache
  ASSERT_OK(ocs_cache->handleMemoryAccess({1, 1}, &hit));
  ASSERT_TRUE(hit);
  expected_stats.accesses++;
  expected_stats.backing_store_hits++;
  EXPECT_EQ(ocs_cache->getPerformanceStats(), expected_stats);

  // Access a third page, should evict the first page (all ref bits set, that's where the hand is pointing)
  ASSERT_OK(ocs_cache->handleMemoryAccess({3*PAGE_SIZE, 1}, &hit));
  ASSERT_FALSE(hit);
  expected_stats.accesses++;
  expected_stats.backing_store_misses++;
  EXPECT_EQ(ocs_cache->getPerformanceStats(), expected_stats);

  EXPECT_EQ(ocs_cache->getClockHand(/*ocs_cache=*/false), 1);

  // Second page should still be a hit
  ASSERT_OK(ocs_cache->handleMemoryAccess({PAGE_SIZE, 1}, &hit));
  ASSERT_TRUE(hit);
  expected_stats.accesses++;
  expected_stats.backing_store_hits++;
  EXPECT_EQ(ocs_cache->getPerformanceStats(), expected_stats);

}

TEST(BasicSuite, TestDRAMAccessOCSCache) {
    // TODO this should run for all subclasses, copying it for FarMemorycache is a hack
  OCSCache *ocs_cache = new BasicOCSCache(
      /*num_pools=*/100, /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/1,
      /*max_conrreutn_backing_store_nodes*/ 2);
  std::vector<mem_access> accesses;
  perf_stats expected_stats;
  bool hit;

  // We should miss on the first access
  ASSERT_OK(ocs_cache->handleMemoryAccess({STACK_FLOOR+1, 1}, &hit));
  ASSERT_TRUE(hit);
  expected_stats.accesses++;
  expected_stats.dram_hits++;
  EXPECT_EQ(ocs_cache->getPerformanceStats(), expected_stats);

}

TEST(BasicSuite, TestDRAMAccessFarMemCache) {
    // TODO this should run for all subclasses, copying it for FarMemorycache is a hack
  OCSCache *ocs_cache = new FarMemCache(
      
      /*max_conrreutn_backing_store_nodes*/ 2);
  std::vector<mem_access> accesses;
  perf_stats expected_stats;
  bool hit;

  // We should miss on the first access
  ASSERT_OK(ocs_cache->handleMemoryAccess({STACK_FLOOR+1, 1}, &hit));
  ASSERT_TRUE(hit);
  expected_stats.accesses++;
  expected_stats.dram_hits++;
  EXPECT_EQ(ocs_cache->getPerformanceStats(), expected_stats);

}
