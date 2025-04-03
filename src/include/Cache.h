#pragma once

#include <string>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

// 缓存管理器类
class CacheManager {
 private:
  // 缓存一个服务和机器IP的map,获取时更新,变化时擦除
  // 节点对应一个服务,可能多个方法
  std::unordered_map<std::string, std::unordered_set<std::string>>
                            service_node_map;
  mutable std::shared_mutex rw_mtx;  // 替换mutex为读写锁

  // 私有构造函数，防止外部实例化
  CacheManager() = default;
  CacheManager(const CacheManager &) = delete;
  CacheManager(CacheManager &&) = delete;

 public:
  // 单例模式：获取唯一实例
  static CacheManager &getInstance() {
    static CacheManager instance;
    return instance;
  }

 public:
  // 获取缓存的IP地址
  std::unordered_set<std::string> get_service_IP(
      const std::string &service) const {
    std::shared_lock lock(rw_mtx);
    auto             it = service_node_map.find(service);

    if (it == service_node_map.end())
      return {};
    else
      return it->second;
  }

  // 完美转发,支持左值和右值
  template <typename ServiceType, typename AddrSetType>
  void set_service_IP(ServiceType &service, AddrSetType &&addrs) {
    std::unique_lock lock(rw_mtx);
    service_node_map[service] = std::forward<AddrSetType>(addrs);
  }

  // void set_service_IP(std::string              &service,
  //                    std::unordered_set<std::string> &&addrs) {
  //   std::unique_lock lock(rw_mtx);
  //   service_node_map[service] = std::move(addrs);
  // }
};
