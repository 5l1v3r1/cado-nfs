
# SSE-3
message(STATUS "Testing whether sse-3 code can be used")
try_run(sse3_runs sse3_compiles
            ${CADO_NFS_BINARY_DIR}/config
            ${CADO_NFS_SOURCE_DIR}/config/sse3.c)
if(sse3_compiles)
    if (sse3_runs MATCHES FAILED_TO_RUN)
        message(STATUS "Testing whether sse-3 code can be used -- No")
        set (HAVE_SSE3 0)
    else (sse3_runs MATCHES FAILED_TO_RUN)
        message(STATUS "Testing whether sse-3 code can be used -- Yes")
        set (HAVE_SSE3 1)
    endif (sse3_runs MATCHES FAILED_TO_RUN)
else(sse3_compiles)
    try_run(sse3_runs sse3_compiles
        ${CADO_NFS_BINARY_DIR}/config
        ${CADO_NFS_SOURCE_DIR}/config/sse3.c
        COMPILE_DEFINITIONS -msse3)
    if(sse3_compiles)
        if (sse3_runs MATCHES FAILED_TO_RUN)
            message(STATUS "Testing whether sse-3 code can be used -- No")
            set (HAVE_SSE3 0)
        else (sse3_runs MATCHES FAILED_TO_RUN)
            message(STATUS "Testing whether sse-3 code can be used -- Yes, with -msse3")
            set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse3")
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
            set (HAVE_SSE3 1)
        endif (sse3_runs MATCHES FAILED_TO_RUN)
    else(sse3_compiles)
        message(STATUS "Testing whether sse-3 code can be used -- No")
        set (HAVE_SSE3 0)
    endif(sse3_compiles)
endif(sse3_compiles)
