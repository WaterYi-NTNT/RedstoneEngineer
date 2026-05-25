# RedstoneEngineer

这是一个基于 Qt 6 和 C++ 实现的 Minecraft 红石电路仿真器。基于 Minecraft 1.21.1 版本制作。

## 构建方法

**依赖**：
- Qt 6.11 或更高版本
- CMake 3.16 或更高版本
- 支持 C++20 的编译器

```bash
cmake -B build
cmake --build build
```

### Linux 平台构建方式

将编译版本区分为 `Debug` 和 `Release` 模式，在编译时使用的 Flags 优化不同。

默认使用 `Debug` 模式进行编译，使用如下的命令：

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
```

当需要使用 `Release` 模式进行发布时：

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
cmake --build --preset package-deb-all
```
