cmake_minimum_required(VERSION 3.13)
project(Ex4)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(uthash)

add_executable(test
        nwebdir/favicon.ico
        nwebdir/index.html
        nwebdir/nigel.jpg
        nwebdir/nweb.log
        ofiles/dkv_client.o
        ofiles/establishment.o
        ofiles/helper.o
        ofiles/kv_client.o
        ofiles/memoryCache.o
        ofiles/memoryPool.o
        ofiles/nweb.o
        ofiles/server.o
        ofiles/test.o
        uthash/utarray.h
        uthash/uthash.h
        uthash/utlist.h
        uthash/utringbuffer.h
        uthash/utstack.h
        uthash/utstring.h
        infiniband-connection/dkv_client.c
        infiniband-connection/dkv_client.h
        infiniband-connection/establishment.c
        infiniband-connection/establishment.h
        utils/helper.c
        utils/helper.h
        infiniband-connection/kv_client.c
        infiniband-connection/kv_client.h
        utils/memoryCache.c
        utils/memoryCache.h
        utils/memoryPool.c
        utils/memoryPool.h
        http-server/nweb.c
        http-server/nweb.h
        README.md
        infiniband-connection/server.c
        infiniband-connection/server.h
        test.c)
