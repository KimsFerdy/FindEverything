#pragma once

#include "core/Index.h"
#include "core/PathCache.h"

#include <vector>

namespace fe {

enum class SortKey { Name, Path, Size, Modified, Count };

// The whole index pre-sorted by every column, built once per index (like
// Everything does). Sorting search results is then a linear filter of one of
// these lists instead of an O(k log k) sort with string comparisons per click.
class SortOrders {
public:
    // Sorts all keys in parallel. ~0.5 s for 500k entries.
    void build(const Index& index, const PathCache& paths);

    bool empty() const { return orders_[0].empty(); }
    const std::vector<RecordId>& order(SortKey key) const { return orders_[static_cast<int>(key)]; }

private:
    std::vector<RecordId> orders_[static_cast<int>(SortKey::Count)];
};

// Returns `results` (any subset of the index) ordered by `key`.
std::vector<RecordId> sortResults(const std::vector<RecordId>& results, size_t indexSize, const SortOrders& sorts,
                                  SortKey key, bool descending);

}  // namespace fe
