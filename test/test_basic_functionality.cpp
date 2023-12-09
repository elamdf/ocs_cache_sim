#include <gtest/gtest.h>

#include "ocs_cache_sim/lib/basic_ocs_cache.h"
#include "ocs_cache_sim/lib/ocs_structs.h"

#define ASSERT_OK(expr) ASSERT_EQ(expr, OCSCache::Status::OK);

// Demonstrate some basic assertions.
TEST(BasicSuite, BasicBackingStoreFunctionality) {
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
  ASSERT_FALSE(hit); // we haven't materialized the parent page of the end of this acccess
  EXPECT_EQ(ocs_cache->getPerformanceStats().accesses, 4);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_misses, 2);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_hits, 1);

  // The other page should be materialized + cached.
  ASSERT_OK(ocs_cache->handleMemoryAccess({100, PAGE_SIZE}, &hit));
  ASSERT_TRUE(hit); // we haven't materialized the parent page of the end of this acccess
  EXPECT_EQ(ocs_cache->getPerformanceStats().accesses, 6);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_misses, 2);
  EXPECT_EQ(ocs_cache->getPerformanceStats().backing_store_hits, 3);
}
