# 分布式文件存储系统（C++ 版）架构设计全总结

## 一、 核心设计哲学

1. **去中心化对等架构 (P2P)：** 放弃 Master-Slave 模式，全集群节点对等，消除单点故障。
2. **AP 优先 (CAP 权衡)：** 针对文件存储场景，牺牲强一致性（CP）换取极高的吞吐量与可用性（AP），采用**最终一致性**模型。
3. **资源隔离与解耦：** 彻底实现“协议处理、业务逻辑、后台维护”的三层解耦，通过异步任务队列驱动。

------



## 二、 三层线程池架构 (Execution Model)

系统被划分为三个逻辑层，层间通过**高性能无锁任务队列**通信，实现 IO 与连接的完全脱离。

### 1. 第一层：接入层 (Ingress Layer)

- **组件：** HTTP Server (外部接口), gRPC Service (内部通讯)。
- **职责：** 协议解析、鉴权、流量分诊。采用 **Reactor/io_uring** 非阻塞模型。
- **逻辑：** 接收数据块并封装为 Task 对象，挂载 SessionContext 回调，丢入二层队列后立刻返回，不阻塞网络。

### 2. 第二层：核心存储层 (Core Storage Layer)

- **职责：** 处理“在线”业务。包括用户上传写盘（pwrite）、RocksDB 元数据提交、逻辑删除、同步通告（Notify）。
- **写盘场景：** 负责处理**用户实时上传**的数据。
- **优先级：** 最高。确保用户上传请求在毫秒级内完成本地持久化并返回响应。

### 3. 第三层：后台维护层 (Background Maintenance Layer)

- **职责：** 处理“离线/长程”任务。包括大文件物理补全（Pulling）、物理删除（GC）、全量数据巡检、水位线对齐。
- **写盘场景：** 负责处理**集群间数据同步**产生的写操作。
- **策略：** **IO 限流与降速**。使用低优先级线程，防止后台同步流量冲击实时业务。

------



## 三、 数据一致性与版本仲裁

### 1. 全局版本号 (Versioning)

- **格式：** 64-bit = (Timestamp << 32) | NodeID。
- **时钟方案：** 引入 **HLC（混合逻辑时钟）**。确保在单机时钟回拨或 NTP 漂移时，版本号依然严格单调递增。

### 2. 冲突解决 (Conflict Resolution)

- **策略：** **LWW (Last Writer Wins)**。
- **逻辑：** 高版本强制覆盖低版本。若时间戳相同，NodeID 大者胜出。这种确定性算法确保了全集群最终状态收敛。

### 3. 墓碑标记 (Tombstone)

- 删除操作仅在元数据中标记 Deleted 并升级版本号。物理文件的真实释放由第三层在安全期（如 7 天后）执行，防止数据因同步延迟而“复活”。

------



## 四、 存储引擎设计 (Metadata & Disk)

### 1. RocksDB 索引设计

采用多列族（Column Families）优化：

- **CF_METADATA:** file_id -> FileMeta（文件属性、Hash、物理路径）。
- **CF_VERSION:** VersionID -> file_id（版本有序索引，支持增量范围扫描）。
- **CF_SYNC_STATUS:** RemoteNodeID -> LastSyncedVersion（本地记录的对端同步水位线）。

### 2. 原子性保障

- **路径：** 物理数据存入 /tmp/ -> fsync -> rename 至存储目录 -> RocksDB WriteBatch 提交。
- **孤儿清理：** 第三层定时核对磁盘文件与 RocksDB 索引，清理无索引的冗余物理文件。

------



## 五、 集群同步机制

### 1. 实时通告 (Notify Update)

- 二层写盘成功后，立即异步发起 gRPC 通告。接收方收到后更新本地元数据索引，并根据需要向第三层派发拉取任务。

### 2. 水位线增量同步 (Watermark-based Sync)

- **心跳：** 每 2s 交换 Max_Version。
- **决策窗口 (Sliding Window)：** 发现落后时，开启 500ms 窗口收集多个节点状态，选出版本最高、网络最快的“导师节点”。
- **增量拉取：** 根据 Sync_Status 记录的水位线，向导师请求 (LastVersion, MaxVersion] 范围内的变更记录。

### 3. 读时修复 (Read-Repair)

- 当用户请求下载一个“元数据存在但物理文件尚未同步完成”的文件时，触发二层紧急派发任务给三层，三层优先拉取该文件，实现透明修复。

------



## 六、 关键优化技术 (C++ Optimization)

1. **零拷贝 (Zero-copy)：**
   - **传输：** 下载链路使用 sendfile()。
   - **内存：** 内部流转使用引用计数的 BufferPool，全链路通过智能指针传递内存地址，消除 memcpy。
2. **异步回调 (Callback Mechanism)：**
   - Task 对象携带 on_done 闭包，存储层执行完毕后触发，通知网络层通过 SessionContext 异步唤醒客户端连接。
3. **背压机制 (Backpressure)：**
   - 通过监控二层/三层任务队列长度，动态反馈至第一层 HTTP 接口。当积压严重时，直接返回 503 Service Unavailable 保护系统。
4. **IO 调度优化：**
   - 利用 fallocate 预分配磁盘空间，规避文件碎片；使用 pwrite 配合多线程实现文件块的并发乱序写入。

------



## 七、 总结：任务流向全景

- **用户上传：** Ingress(解析) -> Core(写盘/定版) -> Callback(响应成功) -> Background(同步通告)。
- **集群对齐：** Heartbeat(发现) -> Decision Window(选优) -> Core(写元数据) -> Background(拉取大文件实感)。
- **系统维护：** Timer(触发) -> Background(巡检/GC/清理)。

该架构通过**执行流（线程池）与数据流（任务对象）的分离**，将 C++ 的底层控制力转化为极高的 IO 并发能力，是仿造并超越 go-fastdfs 的核心路径。