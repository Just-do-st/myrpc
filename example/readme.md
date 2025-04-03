# myrpc use example
callee: service provider
caller: is service caller

# protobuf service define
service define:
  friend
  user

# install protobuf
sudo apt install protobuf-compiler libprotobuf-dev

# install zookeeper and start/stop
sudo apt-get install zookeeperd
sudo systemctl start zookeeper
sudo systemctl stop zookeeper

# start example
./provider -i ~/myrpc/example/callee/callee.conf
./consumer -i ~/myrpc/example/caller/caller.conf 

/usr/share/zookeeper/bin/zkCli.sh