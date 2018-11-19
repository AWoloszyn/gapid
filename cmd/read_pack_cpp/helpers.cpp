#include "helpers2.h"
#include "core/cc/interval_list.h"

gapii::VulkanImports mImports;
gapii::VulkanState mState;
std::map<uintptr_t, std::pair<void*, uintptr_t>> _remapped_ranges;
std::list<pending_write> _pending_writes;