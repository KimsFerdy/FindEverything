#pragma once

#include "core/Index.h"
#include "core/Query.h"

#include <vector>

namespace fe {

bool matches(const Index& index, RecordId id, const Query& query);

// Brute-force scan of every record, split across CPU cores.
// threads = 0 means "use all cores". Results come back in index order.
std::vector<RecordId> search(const Index& index, const Query& query, unsigned threads = 0);

}  // namespace fe
