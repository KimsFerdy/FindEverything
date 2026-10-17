#pragma once

#include <vector>

#include "core/Index.h"
#include "core/PathCache.h"
#include "core/Query.h"

namespace fe {

// brute force over every record, split across cores. results come back in index order.
// threads = 0 -> all cores. paths is only needed for path matching
std::vector<RecordId> search(const Index& index, const Query& query, unsigned threads = 0,
                             const PathCache* paths = nullptr);

}  // namespace fe
