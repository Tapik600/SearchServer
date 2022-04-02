#pragma once

#include <cstdlib>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
  public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        ~Access() {
            guard.unlock();
        }

        Value &ref_to_value;
        std::mutex &guard;
    };

    explicit ConcurrentMap(size_t bucket_count) {
        buckets_ = std::vector<Bucket>(bucket_count);
    };

    Access operator[](const Key &key) {
        const size_t bucket_num = static_cast<size_t>(key) % buckets_.size();

        buckets_[bucket_num].mtx_.lock();
        if (!buckets_[bucket_num].map_.count(key)) {
            buckets_[bucket_num].map_.insert({key, Value()});
        }
        return {buckets_[bucket_num].map_[key], buckets_[bucket_num].mtx_};
    };

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto &[bucket_map, bucket_mtx] : buckets_) {
            bucket_mtx.lock();
            result.merge(std::move(bucket_map));
            bucket_mtx.unlock();
        }
        return result;
    };

  private:
    struct Bucket {
        std::map<Key, Value> map_;
        std::mutex mtx_;
    };
    std::vector<Bucket> buckets_;
};