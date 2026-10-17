#pragma once

#include "core/Index.h"
#include "core/PathCache.h"
#include "core/Query.h"

#include <vector>

namespace fe {

// Brute-force scan of every record, split across CPU cores.
// threads = 0 means "use all cores". Results come back in index order.
// `paths` is needed for path matching; without it, path queries match names only.
std::vector<RecordId> search(const Index& index, const Query& query, unsigned threads = 0,
                             const PathCache* paths = nullptr);

}  // namespace fe
