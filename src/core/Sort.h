#pragma once

#include <vector>

#include "core/Index.h"
#include "core/PathCache.h"

namespace fe {

enum class SortKey { Name, Path, Size, Modified, Count };

// the whole index pre-sorted by every column, built once per index (Everything does the same).
// sorting results is then just filtering one of these lists, no string compares per click
class SortOrders {
public:
    // all keys, in parallel. ~0.5s for 500k on my laptop
    void build(const Index& index, const PathCache& paths);

    bool empty() const { return orders_[0].empty(); }
    const std::vector<RecordId>& order(SortKey key) const { return orders_[static_cast<int>(key)]; }

private:
    std::vector<RecordId> orders_[static_cast<int>(SortKey::Count)];
};

// results can be any subset of the index
std::vector<RecordId> sortResults(const std::vector<RecordId>& results, size_t indexSize, const SortOrders& sorts,
                                  SortKey key, bool descending);

}  // namespace fe
