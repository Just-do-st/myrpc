#include "mprpcapplication.h"
#include "logger.h"
#include <iostream>
#include <unistd.h>
#include <string>

MprpcConfig MprpcApplication::m_config;

void ShowArgsHelp() {
  std::cout << "format: command -i <configfile>" << std::endl;
}

void MprpcApplication::Init(int argc, char **argv) {
  if (argc < 2) {
    ShowArgsHelp();
    exit(EXIT_FAILURE);
  }

  int         c = 0;
  std::string config_file;
  while ((c = getopt(argc, argv, "i:")) != -1) {
    switch (c) {
      case 'i':
        config_file = optarg;
        break;
      case '?':
        ShowArgsHelp();
        exit(EXIT_FAILURE);
      case ':':
        ShowArgsHelp();
        exit(EXIT_FAILURE);
      default:
        break;
    }
  }

  // 开始加载配置文件了 rpcserver_ip=  rpcserver_port   zookeeper_ip=
  // zookepper_port=
  m_config.LoadConfigFile(config_file.c_str());

  LOG_INFO("LoadConfigFile Success!");
  LOG_INFO("rpcserver_ip: %s", m_config.Load("rpcserverip").c_str());
  LOG_INFO("rpcserver_port: %s", m_config.Load("rpcserverport").c_str());
  LOG_INFO("zookeeper_ip: %s", m_config.Load("zookeeperip").c_str());
  LOG_INFO("zookeeper_port: %s", m_config.Load("zookeeperport").c_str());

}

MprpcApplication &MprpcApplication::GetInstance() {
  static MprpcApplication app;
  return app;
}

MprpcConfig &MprpcApplication::GetConfig() {
  return m_config;
}