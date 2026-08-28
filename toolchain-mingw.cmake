set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(LLVM_DIR "E:/mgd_tools/llvm-mingw-20260616-ucrt-x86_64")
set(MINGW_SYSROOT "E:/mgd_tools/llvm-mingw-20260616-ucrt-x86_64")

set(CMAKE_C_COMPILER "${LLVM_DIR}/bin/clang.exe")
set(CMAKE_CXX_COMPILER "${LLVM_DIR}/bin/x86_64-w64-mingw32-clang++.exe")
set(CMAKE_RC_COMPILER "${LLVM_DIR}/bin/llvm-rc.exe")
set(CMAKE_AR "${LLVM_DIR}/bin/llvm-ar.exe")
set(CMAKE_RANLIB "${LLVM_DIR}/bin/llvm-ranlib.exe")
set(CMAKE_LINKER "${LLVM_DIR}/bin/ld.lld.exe")

set(CMAKE_C_COMPILER_TARGET x86_64-w64-mingw32)
set(CMAKE_CXX_COMPILER_TARGET x86_64-w64-mingw32)

set(CMAKE_FIND_ROOT_PATH "${MINGW_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)