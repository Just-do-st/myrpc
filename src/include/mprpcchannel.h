#pragma once

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <unordered_map>
#include <unordered_set>

#include "zookeeperutil.h"

// for each service
class MprpcChannel : public google::protobuf::RpcChannel {
 public:
  //  构造函数中连接zk,启动监听缓存
  MprpcChannel(const std::string &service);
  // ~MprpcChannel();

  std::unordered_set<std::string> MprpcChannel::GetAddrfromCache(
      const std::string &);

  // 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送
  void CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController          *controller,
                  const google::protobuf::Message          *request,
                  google::protobuf::Message                *response,
                  google::protobuf::Closure                *done);

 private:
  ZkClient    zkCli;  // zookeeper连接
  std::string service_name;
};

void UpdateAddrCache(std::unordered_set<std::string> &&);

void watcher_children(zhandle_t *, int, int, const char *, void *);