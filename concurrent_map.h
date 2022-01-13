#pragma once
#include <string>
#include <mutex>
#include <map>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:

    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Access(std::mutex& value_mutex, Value& value) : ref_to_value(value), guard_(value_mutex) {
        }

        Value& ref_to_value;
    private:
        std::lock_guard<std::mutex> guard_;
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count),
        size_(bucket_count) {
    }

    Access operator[](const Key& key) {
        const size_t pos = static_cast<uint64_t>(key) % size_;;
        return Access(buckets_[pos].mutex_, buckets_[pos].map_[key]);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> OrdinaryMap;
        for (size_t i = 0; i < size_; ++i) {
            std::lock_guard<std::mutex> lg(buckets_[i].mutex_);
            OrdinaryMap.insert(buckets_[i].map_.begin(), buckets_[i].map_.end());
        }
        return OrdinaryMap;
    }

    void erase(const Key& key) {
        std::lock_guard lg(buckets_[static_cast<uint64_t>(key) % size_].mutex_);
        buckets_[static_cast<uint64_t>(key) % size_].map_.erase(key);
    }

private:
    struct Bucket {
        std::map<Key, Value> map_;
        std::mutex mutex_;
    };

    std::vector<Bucket> buckets_;
    size_t size_ = 0;
};