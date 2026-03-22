# cmake_minimum_required(VERSION 3.10)

# project(cdfs)

# set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c++17 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")
# set(CMAKE_C_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c17 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

# SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")

# set(CMAKE_VERBOSE_MAKEFILE) #详细输出模式


# include_directories(.)
# include_directories(${CMAKE_CURRENT_SOURCE_DIR})
# include_directories(
#     ${PROJECT_SOURCE_DIR}          # 根目录（支持#include "file/MetadataStore.h"）
#     ${PROJECT_SOURCE_DIR}/file     # file目录（支持#include "MetadataStore.h"）
#     ${PROJECT_SOURCE_DIR}/common   # 通用工具头文件
#     ${PROJECT_SOURCE_DIR}/server   # 服务端头文件
#     ${PROJECT_SOURCE_DIR}/controller   # 客户端头文件
#     ${PROJECT_SOURCE_DIR}/config     # 测试程序头文件
# )

# link_directories(${PROJECT_SOURCE_DIR}/lib)

# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
# include (cmake/utils.cmake)



# # 2. 找到 pthread 库（跨平台方式）
# find_package(Threads REQUIRED)

# # 3. 找到 GTest 库（测试程序用）
# find_package(GTest REQUIRED)
# include_directories(${GTEST_INCLUDE_DIRS})

# # 4. 找到 leveldb 库
# find_library(LEVELDB_LIB leveldb)
# if(NOT LEVELDB_LIB)
#     message(FATAL_ERROR "leveldb not found! Install: sudo apt install libleveldb-dev")
# endif()
# set(LEVELDB_LIBRARIES ${LEVELDB_LIB})

# find_package(nlohmann_json REQUIRED)

# find_package(fmt REQUIRED)

# find_package(Boost REQUIRED)
# if (Boost_FOUND) 
#     include_directories(${Boost_INCLUDE_DIRS})

#         # 打印找到的Boost信息（可选，用于调试）
#     message(STATUS "Boost found: ${Boost_VERSION}")
#     message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
#     message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
# endif()

# find_package(OpenSSL REQUIRED)

# set(LIB_SRC
#     common/logger.cc
#     common/logStream.cc
#     common/Timestamp.cc
#     server/Router.cc
#     server/HttpSession.cc
#     server/UrlUtils.cc  
#     server/cServer.cc
#     file/MetadataStore.cpp
#     file/filestore.cpp
#     controller/FileCtl.cpp
#     server/MultipartParser.cc
#     common/global.cc
#     #test/url_utils_test.cpp !!!这是共享库，测试代码不应该放在这里 !!!
# )

# set(LIBS
#     cdfs                          # 自己的动态库
#     ${Boost_LIBRARIES}            # Boost
#     Threads::Threads              # pthread
#     ${LevelDB_LIBRARIES}          # leveldb（关键，解决leveldb符号缺失）
#     nlohmann_json::nlohmann_json  # json库
#     fmt::fmt                      # fmt库
#     OpenSSL::Crypto  # 核心：链接MD5所在的libcrypto库
# )

# #编译生成动态库
# add_library(cdfs SHARED ${LIB_SRC})
# set_target_properties(cdfs PROPERTIES
#     POSITION_INDEPENDENT_CODE ON  # 动态库必须加-fPIC
#     LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib
# )
# force_redefine_file_macro_for_sources(cdfs)
# #  链接所需的库
# target_link_libraries(cdfs
#     PRIVATE
#     ${Boost_LIBRARIES}        # 链接 Boost 库
#     Threads::Threads          # 链接 pthread 库
#     ${LEVELDB_LIBRARIES}      # 链接 leveldb 库
#     nlohmann_json::nlohmann_json
#     fmt::fmt
#     OpenSSL::Crypto  # 核心：链接MD5所在的libcrypto库
    
# )   


# add_executable(main main.cc)
# m_add_executable(test_log "test/test_log.cc" cdfs ${LIBS})

# # # add_executable(test_url_utils test/url_utils_test.cpp)
# # m_add_executable(test_url_utils "test/url_utils_test.cpp" cdfs ${LIBS})
# # target_link_libraries(test_url_utils  GTest::GTest GTest::Main)

# m_add_executable(server_test "test/server_test.cc" cdfs ${LIBS})

# m_add_executable(meta_test "test/meta_test.cc" cdfs ${LIBS})
# target_link_libraries(meta_test  GTest::GTest GTest::Main)

# SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)












# 静态库编译

cmake_minimum_required(VERSION 3.10)

project(cdfs)

# 添加静态链接C++标准库的编译标志
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c++17 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations -static-libstdc++ -static-libgcc")
set(CMAKE_C_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c17 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")
# 静态链接的链接器标志
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -pthread")

set(CMAKE_VERBOSE_MAKEFILE) #详细输出模式

include_directories(.)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(
    ${PROJECT_SOURCE_DIR}          # 根目录（支持#include "file/MetadataStore.h"）
    ${PROJECT_SOURCE_DIR}/file     # file目录（支持#include "MetadataStore.h"）
    ${PROJECT_SOURCE_DIR}/common   # 通用工具头文件
    ${PROJECT_SOURCE_DIR}/server   # 服务端头文件
    ${PROJECT_SOURCE_DIR}/controller   # 客户端头文件
    ${PROJECT_SOURCE_DIR}/config     # 测试程序头文件
)

link_directories(${PROJECT_SOURCE_DIR}/lib)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include (cmake/utils.cmake)

# 2. 找到 pthread 库（跨平台方式）
find_package(Threads REQUIRED)

# 3. 找到 GTest 库（测试程序用）
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# 4. 找到 leveldb 库（优先找静态库）
find_library(LEVELDB_LIB NAMES libleveldb.a leveldb)
if(NOT LEVELDB_LIB)
    message(FATAL_ERROR "leveldb static library not found! Install: sudo apt install libleveldb-dev")
endif()
set(LEVELDB_LIBRARIES ${LEVELDB_LIB})

find_package(nlohmann_json REQUIRED)

find_package(fmt REQUIRED)

# 查找Boost静态库（明确组件）
find_package(Boost REQUIRED COMPONENTS system thread filesystem) 
if (Boost_FOUND) 
    include_directories(${Boost_INCLUDE_DIRS})
    # 打印找到的Boost信息（可选，用于调试）
    message(STATUS "Boost found: ${Boost_VERSION}")
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
endif()

# ✅ 修正：仅查找OpenSSL，直接使用原生目标，不手动找静态库
find_package(OpenSSL REQUIRED)

# ✅ 修正：LIBS变量中，目标名直接用，变量用${}展开
set(LIBS
    ${Boost_LIBRARIES}            # Boost变量（需${}）
    Threads::Threads              # pthread目标（无需${}）
    ${LEVELDB_LIBRARIES}          # leveldb变量（需${}）
    nlohmann_json::nlohmann_json  # json目标（无需${}）
    fmt::fmt                      # fmt目标（无需${}）
    OpenSSL::Crypto               # OpenSSL目标（无需${}）
    ${GTEST_LIBRARIES}            # GTest变量（需${}）
)

# 编译生成静态库（替换SHARED为STATIC）
add_library(cdfs STATIC 
    common/logger.cc
    common/logStream.cc
    common/Timestamp.cc
    server/Router.cc
    server/HttpSession.cc
    server/UrlUtils.cc  
    server/cServer.cc
    file/MetadataStore.cpp
    file/filestore.cpp
    controller/FileCtl.cpp
    server/MultipartParser.cc
     common/global.cc
)
set_target_properties(cdfs PROPERTIES
    ARCHIVE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib # 静态库输出路径（.a文件）
)
force_redefine_file_macro_for_sources(cdfs)

# ✅ 修正：链接cdfs库，目标名直接用
target_link_libraries(cdfs
    PRIVATE
    ${Boost_LIBRARIES}        # Boost变量（需${}）
    Threads::Threads          # pthread目标（无需${}）
    ${LEVELDB_LIBRARIES}      # leveldb变量（需${}）
    nlohmann_json::nlohmann_json
    fmt::fmt
    OpenSSL::Crypto           # OpenSSL目标（无需${}）
)   

add_executable(main main.cc)
# 链接main到静态cdfs库
target_link_libraries(main PRIVATE cdfs ${LIBS})

add_executable(test_log test/test_log.cc)
# 静态链接自定义cdfs库 + 所有依赖库
target_link_libraries(test_log
    PRIVATE
    cdfs          # 静态链接自定义cdfs.a库
    ${LIBS}       # 链接Boost/leveldb/OpenSSL等依赖
)

# ========== 替换2：server_test 保持原生语法（无需m_add_executable） ==========
add_executable(server_test test/server_test.cc)
target_link_libraries(server_test
    PRIVATE
    cdfs          # 静态链接自定义cdfs.a库
    ${LIBS}       # 链接Boost/leveldb/OpenSSL等依赖
)



# 输出路径
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(ARCHIVE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib) # 静态库输出路径