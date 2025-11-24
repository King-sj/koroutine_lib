# 集成与打包（Integration & Packaging）

本章节说明三种常见的集成方式：

1. 源码集成（推荐） — `add_subdirectory`
2. 头文件 + 预编译库（`include` + `lib`）
3. 单头文件（single-header）+ 库

并解释静态库与共享库（static vs shared）的差异以及注意事项。

---

## 方式一：源码集成（`add_subdirectory`）

这是在 CMake 中最直接的集成方式。将 `koroutine_lib` 作为子模块或直接把源码放入你的仓库，然后在你的 `CMakeLists.txt` 中使用：

```cmake
# 假设你把 koroutine_lib 放在第三方目录
add_subdirectory(third_party/koroutine_lib)

# 你的目标
add_executable(my_app src/main.cpp)

target_link_libraries(my_app PRIVATE koroutinelib_static)
# 或者链接共享库
# target_link_libraries(my_app PRIVATE koroutinelib_shared)
```

优点：
- 最简单的调试与修改体验；
- 编译选项可与主工程一起控制；
- 可以直接使用内置 CMake targets（demos/tests）进行开发验证。

缺点：
- 每次构建都会编译库源码（开发阶段很方便，发布时可能不够轻量）。

---

## 方式二：`include` + `lib`（预编译库）

构建并安装库，然后在消费工程中引用已安装的头文件和库文件。

### 构建与安装（示例）

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /usr/local
```

安装后，你会在 `/usr/local/include/koroutine` 找到头文件，在 `/usr/local/lib` 找到 `libkoroutinelib.a`（静态）或 `libkoroutinelib.so`（共享）。

### 使用示例（消费方 CMake）

```cmake
find_path(KOROUTINE_INCLUDE_DIR koroutine/koroutine.h PATHS /usr/local/include)
find_library(KOROUTINE_LIB koroutinelib PATHS /usr/local/lib)

add_executable(my_app src/main.cpp)

target_include_directories(my_app PRIVATE ${KOROUTINE_INCLUDE_DIR})
target_link_libraries(my_app PRIVATE ${KOROUTINE_LIB})
```

### 静态库 vs 共享库

- 静态库 (`.a` / `.lib`)：
  - 优点：部署简单（单个二进制），没有运行时依赖；
  - 缺点：二进制体积较大，若多个程序都静态链接相同库则重复内存占用。

- 共享库 (`.so` / `.dylib` / `.dll`)：
  - 优点：运行时可重用，便于发布补丁；
  - 缺点：需要正确设置运行时链接路径（`LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` / `rpath`）。

---

## 方式三：单头文件（single-header）+ 库

本仓库包含一个生成单头文件的脚本和 CMake 目标：

- 运行 `cmake --build build --target koroutine_single_header` 会在 `build/include/koroutine/koroutine.hpp` 生成单头文件。

使用方法：
1.  将生成的 `koroutine.hpp` 放到你的项目 `include/` 下；
2.  链接 `koroutinelib_static` 或 `koroutinelib_shared`（库仍然需要，因为实现分离了一些平台/IO 引擎）；

优点：
- 对用户来说包含一个头文件非常方便；
- 便于在不想集成整套源码的场景下快速使用库。

缺点：
- 单头往往会变得很大，可能影响编译时间；
- 如果库的实现强依赖平台特性，单头可能仍需链接实现库。

---

## 额外建议

- 如果你打算把库打包为系统包（如 Debian/apt、Homebrew、vcpkg）：
  - 优先提供 `include/` 与 `lib/`（静态与共享均可），并确保 `cmake` 安装目标工作正常；
  - 提供版本号、LICENSE 与 CMake 的 `pkg-config` / `cmake` 配置帮助消费端查找。

- 对于快速原型或样例项目，使用单头 + 静态库是一种很好的折衷。

