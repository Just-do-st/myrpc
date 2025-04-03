#include "mprpcchannel.h"
#include <string>
#include "rpcheader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "mprpcapplication.h"
#include "zookeeperutil.h"
#include "Cache.h"
#include <random>

// 递归获取并输出服务的所有IP地址
// void get_and_print_ips(zhandle_t *zh, const char *path) {
//   struct String_vector children;
//   int rc = zoo_get_children(zh, path, 0, &children);  // 获取子节点列表
//   if (rc == ZOK) {
//     std::cout << "Service: " << path << " contains IPs:" << std::endl;
//     for (int i = 0; i < children.count; ++i) {
//       std::string child_path = std::string(path) + "/" +
//       children.data[i]; char        buffer[1024]; int         buffer_len
//       = sizeof(buffer); struct Stat stat; int         rc_data =
//           zoo_get(zh, child_path.c_str(), 0, buffer, &buffer_len, &stat);
//       if (rc_data == ZOK) {
//         std::string node_data(buffer, buffer_len);
//         std::cout << "  - " << child_path << ": " << node_data <<
//         std::endl;
//       } else {
//         std::cerr << "Failed to get data from node " << child_path
//                   << ", error: " << rc_data << std::endl;
//       }
//     }
//     deallocate_String_vector(&children);  // 释放子节点列表内存
//   } else {
//     std::cerr << "Failed to get children of " << path << ", error: " <<
//     rc
//               << std::endl;
//   }
// }

// Watcher回调函数
std::string extract_ip_from_data(const char *data, int data_len) {
  if (!data || data_len <= 0) return "";  // 数据为空时返回空字符串
  return std::string(data, data_len);     // 假设数据内容直接是IP地址
}

void UpdateAddrCache(std::string                       service_name,
                     std::unordered_set<std::string> &&ips_found) {
  CacheManager &C = CacheManager::getInstance();
  C.set_service_IP(service_name, std::move(ips_found));
}

// 获取子节点并注册Watcher
void watch_node_change(zhandle_t *zh, const char *path) {
  std::string service_name(path + 1);

  // 1. 获取最新节点列表, 同时再注册watcher

  struct String_vector children;
  int rc = zoo_wget_children(zh, path, watcher_children, nullptr, &children);
  if (rc == ZOK) {  // 服务发现
    char                            buffer[256];
    int                             buffer_len = sizeof(buffer);
    std::unordered_set<std::string> ips_found;

    std::cout << "methods of service " << path << ":" << std::endl;
    // 遍历子节点，获取每个子节点的数据（IP地址）
    for (int i = 0; i < children.count; ++i) {
      std::string method = std::string(path) + "/" + children.data[i];

      int data_rc =
          zoo_get(zh, method.c_str(), 0, buffer, &buffer_len, nullptr);
      if (data_rc == ZOK) {
        std::string ip = extract_ip_from_data(buffer, buffer_len);
        if (!ip.empty()) {
          std::cout << "  - ip of " << method << " is " << ip << std::endl;
          ips_found.insert(ip);  // 将提取的IP地址加入新IP集合
        }
      } else {
        std::cerr << "Failed to get data for node: " << method << std::endl;
      }
    }

    // 2. 更新缓存

    UpdateAddrCache(service_name, std::move(ips_found));

    deallocate_String_vector(&children);
  } else {
    std::cerr << "Failed to get children of service " << path
              << ", error: " << rc << std::endl;
  }
}

void watcher_children(zhandle_t *zh, int type, int state, const char *path,
                      void *watcherCtx) {
  if (type == ZOO_CHILD_EVENT) { watch_node_change(zh, path); }
}

// 构造函数, zk连接
MprpcChannel::MprpcChannel(const std::string &service_name)
    : service_name(service_name) {
  zkCli.Start();  // 连接zk,注册监听

  // 监听 /services 服务下,方法和节点IP变化, 删除缓存
  watch_node_change(zkCli.get_zhandle_t(), ("/" + service_name).c_str());
}

std::unordered_set<std::string> MprpcChannel::GetAddrfromCache(
    const std::string &service) {
  CacheManager &C = CacheManager::getInstance();
  // 已经拷贝的结果避免二次拷贝
  return std::move(C.get_service_IP(service));
}

/*
header_size + service_name method_name args_size + args
*/
// 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                              google::protobuf::RpcController *controller,
                              const google::protobuf::Message *request,
                              google::protobuf::Message       *response,
                              google::protobuf::Closure       *done) {
  const google::protobuf::ServiceDescriptor *sd = method->service();
  std::string service_name = std::string(sd->name());     // service_name
  std::string method_name = std::string(method->name());  // method_name

  // 获取参数的序列化字符串长度 args_size
  uint32_t    args_size = 0;
  std::string args_str;
  if (request->SerializeToString(&args_str)) {
    args_size = args_str.size();
  } else {
    controller->SetFailed("serialize request error!");
    return;
  }

  // 定义rpc的请求header
  mprpc::RpcHeader rpcHeader;
  rpcHeader.set_service_name(service_name);
  rpcHeader.set_method_name(method_name);
  rpcHeader.set_args_size(args_size);

  uint32_t    header_size = 0;
  std::string rpc_header_str;
  if (rpcHeader.SerializeToString(&rpc_header_str)) {
    header_size = rpc_header_str.size();
  } else {
    controller->SetFailed("serialize rpc header error!");
    return;
  }

  // 组织待发送的rpc请求的字符串
  std::string send_rpc_str;
  send_rpc_str.insert(0, std::string((char *)&header_size, 4));  // header_size
  send_rpc_str += rpc_header_str;                                // rpcheader
  send_rpc_str += args_str;                                      // args

  // 打印调试信息
  std::cout << "============================================" << std::endl;
  std::cout << "header_size: " << header_size << std::endl;
  std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
  std::cout << "service_name: " << service_name << std::endl;
  std::cout << "method_name: " << method_name << std::endl;
  std::cout << "args_size: " << args_size << std::endl;
  std::cout << "============================================" << std::endl;

  // 使用tcp编程，完成rpc方法的远程调用
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == clientfd) {
    char errtxt[512] = {0};
    sprintf(errtxt, "create socket error! errno:%d", errno);
    controller->SetFailed(errtxt);
    return;
  }

  // 读取配置文件rpcserver的信息
  // std::string ip =
  // MprpcApplication::GetInstance().GetConfig().Load("rpcserverip"); uint16_t
  // port =
  // atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
  // rpc调用方想调用service_name的method_name服务，需要查询zk上该服务所在的host信息
  std::string method_path = "/" + service_name + "/" + method_name;

  // Use cache. if in map?
  CacheManager                   &C = CacheManager::getInstance();
  std::unordered_set<std::string> addrs = C.get_service_IP(method_path);

  // TODO: do load balance
  std::string                     host_data;
  std::unordered_set<std::string> IPs = GetAddrfromCache(service_name);

  if (!IPs.empty()) {
    // return one IP
    // 1. 随机选择负载均衡
    static std::random_device       rd;
    static std::mt19937             gen(rd());
    std::uniform_int_distribution<> dis(0, IPs.size() - 1);

    auto it = IPs.begin();
    std::advance(it, dis(gen));  // 移动迭代器到随机位置
    host_data = *it;
  } else {
    host_data = zkCli.GetData(method_path.c_str());
  }

  // 127.0.0.1:8000
  if (host_data == "") {
    controller->SetFailed(method_path + " is not exist!");
    return;
  }
  int idx = host_data.find(":");
  if (idx == -1) {
    controller->SetFailed(method_path + " address is invalid!");
    return;
  }
  std::string ip = host_data.substr(0, idx);
  uint16_t    port =
      atoi(host_data.substr(idx + 1, host_data.size() - idx).c_str());

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

  // 连接rpc服务节点
  if (-1 ==
      connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
    close(clientfd);
    char errtxt[512] = {0};
    sprintf(errtxt, "connect error! errno:%d", errno);
    controller->SetFailed(errtxt);
    return;
  }

  // 发送rpc请求
  if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0)) {
    close(clientfd);
    char errtxt[512] = {0};
    sprintf(errtxt, "send error! errno:%d", errno);
    controller->SetFailed(errtxt);
    return;
  }

  // 接收rpc请求的响应值

  if (done == nullptr) {
    char recv_buf[1024] = {0};
    int  recv_size = 0;

    if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0))) {
      close(clientfd);
      char errtxt[512] = {0};
      sprintf(errtxt, "recv error! errno:%d", errno);
      controller->SetFailed(errtxt);
      return;
    }

    // 反序列化rpc调用的响应数据
    // std::string response_str(recv_buf, 0, recv_size); //
    // bug出现问题，recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败 if
    // (!response->ParseFromString(response_str))
    if (!response->ParseFromArray(recv_buf, recv_size)) {
      close(clientfd);
      // char errtxt[512] = {0};
      std::string errtxt =
          "parse error! response_str: " + std::string(recv_buf);
      // snprintf(errtxt, sizeof(errtxt), "parse error! response_str:%s",
      // std::string(recv_buf));
      controller->SetFailed(errtxt);
      return;
    }
  } else {
    // // 异步调用逻辑
    // std::thread([=]()
    //             {
    //       char recv_buf[1024];
    //       int recv_size = recv(clientfd, recv_buf, 1024, 0);
    //       if (recv_size == -1) {
    //           controller->SetFailed("RPC call failed: recv error");
    //       } else if (!response->ParseFromArray(recv_buf, recv_size)) {
    //           controller->SetFailed("RPC call failed: response parse error");
    //       }
    //       // 调用用户定义的回调函数
    //       done->Run(); })
    //     .detach();
  }
}