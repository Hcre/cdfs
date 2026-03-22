#ifndef __NODE_H__
#define __NODE_H__

#include "const.h"

//节点状态
struct NodeStatus {
    std::string address_;
    bool isActive;
    //自身负载
    int load;
};


class NodeManager {
public:
    typedef std::shared_ptr<NodeStatus> PNode; 

    //随机获取一个节点
    PNode getOneNode();
    //更新自身节点状态
    int updateNodeStatus();   

private:
    //自身节点的地址
    //todo, 将地址封装为一个类
    std::string address_;
    //节点集群
    std::vector<PNode> nodes;
};

#endif