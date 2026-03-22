#include "KVStore.h"

void KVStore::set(const std::string& key, const std::string& value) {
    std::unique_lock lock(mutex_);
    data_[key] = value;
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    std::shared_lock lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    return it->second;
}

bool KVStore::del(const std::string& key) {
    std::unique_lock lock(mutex_);
    return data_.erase(key) > 0;
}
