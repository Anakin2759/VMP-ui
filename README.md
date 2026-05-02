# PmkUi

PmkUi 是从 PestManKill 项目中独立出来的 C++23 UI 静态库。仓库当前只保留 UI 库、UI 必要资源、可构建示例和 UI 单元测试；旧客户端网络集成、服务端、共享协议、KCP 网络层和通用工具 target 已从有效构建拓扑中移除。

## 目录

```text
src/ui/              UI 静态库源码与内置资源
example/ui_demo/     原客户端界面迁移后的示例程序
tests/unittest/ui/   UI 单元测试
cmake/               UI 构建辅助脚本
docs/                迁移规划、协调记录和测试报告
```

## 构建

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target ui
cmake --build build --target example_ui_demo
```

默认开启 `BUILD_EXAMPLES`，会生成 `example_ui_demo`。如只构建库，可配置 `-DBUILD_EXAMPLES=OFF`。

## 测试

```bash
cmake -G Ninja -B build-tests -DCMAKE_BUILD_TYPE=Debug -DENABLE_BUILD_TESTS=ON
cmake --build build-tests --target ui_tests
ctest --test-dir build-tests --output-on-failure
```

测试入口只保留 `ui_tests`，不再构建旧网络测试。

## 依赖

有效构建仅加载 UI 所需第三方：ASIO、cmrc、EnTT、SDL3、stb、Yoga、Eigen、FreeType、HarfBuzz、spdlog，以及测试用 GoogleTest。

`example_ui_demo` 只链接 `ui`，不链接旧 `utils`、`shared`、`net`、`server`，也不链接 absl、json、mimalloc 或 kcp。
