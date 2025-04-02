#aux_source_directory(. SRC_LIST)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # 设置静态库路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # 设置动态库路径


set(SRC_LIST 
    mprpcapplication.cc 
    rpcprovider.cc 
    mprpcconfig.cc
    rpcheader.pb.cc 
    mprpcchannel.cc
    mprpccontroller.cc
    logger.cc
    zookeeperutil.cc
    )
# muduo是静态库，因此这里也生成静态库
add_library(mprpc ${SRC_LIST})
target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)