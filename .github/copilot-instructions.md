# CDFS (Chunked Distributed File System) - AI Coding Guidelines

## Architecture Overview
CDFS is a distributed file storage system with HTTP API. Core components:
- **Server**: Boost ASIO-based HTTP server (`server/cServer.h`, `server/HttpSession.h`) handling requests via `Router.cc`
- **File Storage**: `FileStore` manages chunked file storage with MD5-based IDs; `MetadataStore` uses LevelDB for metadata persistence
- **Common Utils**: Muduo-inspired logging (`LOG_INFO << "msg"`), timestamps, singletons in `common/`

Data flow: HTTP requests → Router → FileStore (saves to disk + metadata to LevelDB) → responses

## Build Workflow
- Dependencies: Boost, LevelDB, nlohmann_json, fmt, GTest
- Build: `cd build && cmake .. && make` (outputs to `bin/` and `lib/`)
- Run server: `./bin/main` (placeholder; actual server logic in `cServer`)
- Tests: `./bin/meta_test` (GTest-based, uses temp dirs for isolation)

## Coding Conventions
- Namespace: `cdfs`
- Logging: Use `LOG_INFO`, `LOG_ERROR` macros (auto-include file/line)
- File IDs: 32-char MD5 strings
- Config: `StoreConfig` struct for settings (e.g., root_dir="./data/store")
- Error handling: Exceptions in server code, optional returns for file ops
- Includes: Relative paths like `#include "file/MetadataStore.h"`

## Key Patterns
- Sharding: Files split into shards with separate MD5s (`save_shard` method)
- Metadata: JSON-serialized `MetaFile` structs stored in LevelDB
- HTTP Routing: `Router.cc` maps paths to handlers (e.g., file upload/download)
- Threading: ASIO async operations, shared_mutex for metadata access

## Dependencies & Integration
- LevelDB: For persistent metadata (install via `sudo apt install libleveldb-dev`)
- Boost ASIO: Networking (async_accept in `cServer::start_accept`)
- JSON: nlohmann_json for serialization
- Testing: GTest fixtures with temp directories (see `test/meta_test.cc`)

Reference: `CMakeLists.txt` for build deps, `file/filestore.cpp` for storage logic</content>
<parameter name="filePath">/home/huishuohuademao/workspace/cdfs/.github/copilot-instructions.md