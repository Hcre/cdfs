#ifndef FILECTL_H
#define FILECTL_H

#include "file/filestore.h"
#include "const.h"
#include "server/HttpContext.h"


namespace cdfs {
class FileCtl : public std::enable_shared_from_this<FileCtl> {
private:

    static inline std::atomic<int> concurrent_uploads_{0};
    const int MAX_CONCURRENT_UPLOADS = 20;

    std::shared_ptr<FileStore> fileStore_;
    std::shared_ptr<MetadataStore> metadataStore_;
public:
    FileCtl(std::shared_ptr<FileStore> fileStore, std::shared_ptr<MetadataStore> metadataStore);

    //check if the file exists
    void handleCheck(const HttpContext& ctx, ResponseSender sender);

    void handleUpload(const HttpContext& ctx, ResponseSender sender);

    void handleDownload(const HttpContext& ctx, ResponseSender sender);

    //检查分片
    void handleCheckChunk(const HttpContext& ctx, ResponseSender sender);
    
    //分片传输
    void handleChunkUpload(const HttpContext& ctx, ResponseSender sender);

    void handleMerge(const HttpContext& ctx, ResponseSender sender);
};


}
#endif