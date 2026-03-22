#ifndef __METAFILESET_H__
#define __METAFILESET_H__
#include "const.h"

struct MetaFile;

//内存区meta集
class MetaFileSet {
public:
    //查询
    const MetaFile& searchByUUid(const std::string&);

    //更新,线程安全
    void update(MetaFile meta);

    //删除,逻辑删除,线程安全
    void deleteByUUid(const std::string&);

    //若已满，运用LNU算法
    void addNewMeta(MetaFile meta);

private:
    //更换为具有淘汰策略的
    std::map<std::string, MetaFile> _meta_map;
    size_t _size;
     
};



#endif