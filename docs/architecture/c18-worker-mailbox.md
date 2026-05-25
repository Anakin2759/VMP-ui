# C18 Worker Mailbox 实现规划评审

> 日期：2026-05-25
> 输入来源：任务描述中的拟定方案 + 代码阅读（UiRuntime.hpp / RuntimeFacade.hpp/.cpp / TaskChain.hpp / ThreadPool.hpp）
> 作用范围：`src/core/WorkerMailbox.hpp`（新增）、`src/core/UiRuntime.hpp`、`src/core/RuntimeFacade.hpp/.cpp`、`src/core/TaskChain.hpp`

---

## 评审结论摘要

方案主干**可行**：双缓冲 mailbox + 主线程帧首 flush 是处理多写一读 ECS 竞争的标准模式，与现有 `UiRuntimeScope`/`ActiveRuntimeState` 的 pointer-swap 机制吻合良好，接入成本低。

但存在 **2 个高风险点**（若不处理会导致 UB）和 **4 个中等问题**，需在实施前裁定。

注释：MPSC queue + 双缓冲/批量交换（swap） 无锁编程

---

## 六项评审问题裁定

### Q1 — ABI/链接层面：header-only class with mutex, std::vector

**结论：无 ABI 问题，但建议 flush() 移入 .cpp。**

- `std::mutex` 和 `std::vector<WorkerCommand>` 均为标准库类型，ABI 稳定。
- `WorkerCommand` 包含 `std::move_only_function`（不可复制），`std::vector` 仅需移动语义；`swap` 操作合法，编译期无障碍。
- `UiRuntime` 已声明为不可移动/复制，`WorkerMailbox`（因 mutex 不可移动）作为成员不引入新的限制。
- **建议**：`push()` 逻辑简单（lock + emplace_back），可内联在 .hpp；`flush()` 涉及 variant dispatch + 回调执行，代码量较重，**建议放入 `WorkerMailbox.cpp`** 以减少编译时间膨胀并隔离包含链（`entt/entt.hpp` 已被 UiRuntime.hpp 传递包含，此处无额外代价，但 flush 实现链接单点更易调试）。

---

### Q2 — swap 期间并发 push 是否安全，下一帧能否正确处理

**结论：逻辑正确，但实现必须严格分两阶段——swap 在锁内，执行在锁外。**

正确的 `flush` 骨架：

```cpp
void WorkerMailbox::flush(entt::registry& reg) {
    // Phase 1: 在锁内 swap，立即释放锁
    {
        std::lock_guard lock(m_mutex);
        std::swap(m_writeBuffer, m_readBuffer);
        // 此后 m_writeBuffer 已为空，workers 可立即继续 push
    }
    // Phase 2: 在锁外执行，主线程独占 m_readBuffer，无竞争
    for (auto& cmd : m_readBuffer) {
        std::visit(overloaded{
            [&](worker::RegistryCmd& c) { c.apply(reg); },
            [&](worker::InvokeCmd&  c) { c.invoke();    }
        }, cmd);
    }
    m_readBuffer.clear();
}
```

- flush 期间（Phase 2），worker 向空的 `m_writeBuffer` push：**安全**，下一帧 flush 时再 swap 处理。
- `m_readBuffer` 在 Phase 2 由主线程独占，无需加锁。
- **风险**：若实现者把 `m_readBuffer.clear()` 放在锁内，会不必要地阻塞 workers；若 swap 在锁外，才会产生真实竞争——务必保证 swap 是临界区内的唯一操作。

---

### Q3 — WorkerMailbox 放 UiRuntime vs entt::registry ctx()

**裁定：放 `UiRuntime`，不放 ctx()。**

| 维度                 | UiRuntime 成员                                 | entt::registry ctx()                                      |
| -------------------- | ---------------------------------------------- | --------------------------------------------------------- |
| 生命周期控制         | 与 UiRuntime 完全同生共死，析构顺序可控        | ctx 对象析构在 registry 销毁时，顺序不透明                |
| 访问权限隔离         | friend RuntimeFacade 已存在，天然限制访问方    | ctx() 对所有持有 registry 引用的代码开放，权限粒度粗      |
| flush 接收 registry& | 主线程从 UiRuntime 取 enttRegistry()，路径清晰 | ctx 内 flush 需持有 registry 引用，形成自引用循环         |
| 语义匹配             | WorkerMailbox 是"运行时基础设施"，非"实体数据" | ctx() 适合轻量的跨系统共享数据，不适合含 mutex 的调度对象 |

---

### Q4 — tryMailbox() 为 nullptr 时 skip 还是 assert

**裁定：skip + 单次 spdlog::warn（debug/release 均适用），不 assert。**

理由：

- TaskChain 是框架层，`nullptr` 在测试场景（未挂载 UiRuntimeScope 的单元测试）下合法出现。
- assert 会导致测试环境崩溃，与现有 `tryFrame()`/`tryState()` 返回 nullptr-safe 的防御风格不一致。
- 参照 `Registry::current()` / `Dispatcher::current()` 采用 `terminate` 的方式是针对"必须有"的调用路径；`tryMailbox()` 本身已语义声明了"可能无"。
- 推荐实现：

```cpp
void operator()(uint32_t delta) {
    if (auto* mb = RuntimeFacade::current().tryMailbox()) {
        mb->flush(RuntimeFacade::current().enttRegistry());
    }
    // else: 单线程模式或非激活帧，无需 flush，不报错
    // ...
}
```

若想在开发期暴露意外的 nullptr，可加 `SPDLOG_WARN_ONCE` 而非 assert。

---

### Q5 — RegistryCmd 是否足够，是否需要 EntityCreateCmd

**裁定：RegistryCmd `void(entt::registry&)` 已足够，不需要 EntityCreateCmd。**

- 实体创建：`auto e = reg.create();` 完全可以在 RegistryCmd 的 lambda 内执行，不需要单独类型。
- 实体创建 + 写回场景：主线程提前 `reg.create()` 获得 entity，传给 worker 捕获，worker push RegistryCmd 写回——entity 在 worker 侧只读引用，无竞争。
- **InvokeCmd 语义需明确**：`void()` 无 registry 参数，适合纯副作用（日志记录、回调通知等）。文档中需明确约束：**InvokeCmd 内部禁止访问 entt::registry**，防止绕过保护机制直接读写 ECS。若无法执行此约束，建议移除 InvokeCmd，统一用 RegistryCmd（不需要访问 registry 时忽略参数即可）。

---

### Q6 — 命名与接口设计建议

| 现有命名          | 建议命名                     | 理由                                                                                                     |
| ----------------- | ---------------------------- | -------------------------------------------------------------------------------------------------------- |
| `WorkerMailbox` | `FrameCommandQueue`        | 强调"帧粒度的命令队列"，而非"worker 私有"；mailbox 暗示点对点通信                                        |
| `WorkerCommand` | `FrameCommand`             | 命令由 worker 生成，但属于帧的调度单元                                                                   |
| `RegistryCmd`   | `RegistryCommand`          | 与 C++23 风格统一；避免与 SDL/Windows Cmd 概念歧义                                                       |
| `InvokeCmd`     | `InvokeCommand` 或直接移除 | 同上；若保留需加文档约束                                                                                 |
| `push()`        | `enqueue()`                | 与 ASIO `post()`/`enqueue()` 语境一致；避免与 std::stack::push 混淆                                  |
| `flush()`       | `drain()`                  | 强调"清空所有挂起命令"的语义，flush 更接近 IO 缓冲                                                       |
| `hasPending()`  | 建议移除                     | 主线程只有两个场景：drain 和不 drain；轮询 hasPending 无增量价值，且有 TOCTOU 风险（检查后状态可能变化） |

---

## 高风险点

### 🔴 R1 — Worker 持有裸 WorkerMailbox* 指针，UiRuntime 销毁后悬空

**场景**：主线程提前销毁 `UiRuntime`（例如多运行时切换、热重载），但 ThreadPool 中的 worker 任务仍在运行，其捕获的 `WorkerMailbox*` 指向已释放内存，产生 UAF。

**危害**：未定义行为，可能在 flush 时崩溃，调试极难。

**解决方案（选其一）**：

**方案 A（推荐，最小改动）**：`UiRuntime` 析构时调用 `ThreadPool::instance().drainPending()` 或等价的 join 语义，确保所有 worker 任务在 UiRuntime 销毁前完成。ThreadPool 已持有 `asio::thread_pool`，可扩展 `wait()` 接口。

```cpp
// UiRuntime 析构
~UiRuntime() {
    ThreadPool::instance().wait(); // 等所有 asio::thread_pool 任务完成
}
```

**方案 B**：改用 `std::shared_ptr<WorkerMailbox>`，worker 捕获 shared_ptr；UiRuntime 持有 shared_ptr；销毁 UiRuntime 时设置 cancelled flag，flush 时检查并跳过 cancelled mailbox。代价是额外的引用计数开销。

**方案 C**（多运行时场景）：主线程提交 worker 时同时传递运行时的"generation token"（uint64_t，随 UiRuntime 销毁递增），flush 时验证 token，不匹配则丢弃命令。

> **裁定建议**：优先方案 A——成本最低，与现有架构最契合。

---

### 🔴 R2 — 单线程模式（UI_ENABLE_MULTITHREAD=0）下的行为语义不一致

**场景**：`ThreadPool::submitDetached` 在单线程模式下同步执行任务（调用线程立即运行），worker lambda 内调用 `mb->push(cmd)` 时，主线程正处于 submitDetached 的调用栈上，**push 被立即执行，cmd 进入 writeBuffer**。随后当帧的 QueuedTask 已经运行过 flush（flush 在 QueuedTask 开头），这帧的 cmd **永远不会被消费**，直到下一帧。

与多线程模式相比行为不对称，在单线程开发/测试环境下会产生"命令延迟一帧"的奇怪现象。

**解决方案**：

```cpp
// 单线程模式下直接执行，无需经过 mailbox
if constexpr (!ThreadPool::isMultithreaded()) {
    cmd.apply(RuntimeFacade::current().enttRegistry());
} else {
    mb->enqueue(std::move(cmd));
}
```

或在 `drain()` 内加判断，单线程模式下直接透传，不走双缓冲路径。

---

## 中等风险点

### ⚠️ R3 — RegistryCmd 内部触发 BUFFERED 事件时序问题

`QueuedTask` 执行顺序：`flush` → `UpdateTimer` → `Dispatcher::Update()`（BUFFERED 事件派发）。

若 RegistryCmd 内部调用 `Dispatcher::Enqueue<SomeEvent>()`，该事件在 `Dispatcher::Update()` 中会被当前帧处理；但若调用 `Dispatcher::Trigger<SomeEvent>()` (IMMEDIATE)，会在 UpdateTimer 前提前触发，可能破坏帧上下文状态。

**裁定建议**：在文档/注释中明确约束：**RegistryCmd 的 lambda 内禁止调用任何 Dispatcher 相关 API**，写回只限于 ECS 操作（emplace/patch/destroy/create）。

---

### ⚠️ R4 — ActiveRuntimeState 迁移不完整导致 nullptr 陷阱

新增 `WorkerMailbox* mailbox = nullptr` 后，所有直接构造 `ActiveRuntimeState{}` 的代码（包括测试代码中手工构造的存根）默认 mailbox 为 nullptr。`restoreRuntime` 如果无条件 swap mailbox，会将正常激活的 mailbox 替换为 nullptr，导致下一帧 flush 静默跳过。

**要求**：`activateRuntime`/`restoreRuntime` 实现时必须保持 mailbox 的 swap 对称性，且确保 `UiRuntimeScope` 析构时正确还原旧指针（而非将 nullptr 写入当前槽位）。

---

### ⚠️ R5 — flush 异常安全

`move_only_function::operator()` 抛异常时，当前 drain 循环会中断，剩余命令永远丢失（m_readBuffer 已 clear 后的命令是一次性的）。

**建议**：参照 ThreadPool 中 worker 的 try-catch 模式，在 drain 循环内捕获异常并记录，确保所有命令都被尝试执行：

```cpp
for (auto& cmd : m_readBuffer) {
    try {
        std::visit(..., cmd);
    } catch (const std::exception& ex) {
        Logger::error("[WorkerMailbox] Command threw: {}", ex.what());
    }
}
```

---

## 影响范围与改动类型

| 文件                           | 类型         | 说明                                                        |
| ------------------------------ | ------------ | ----------------------------------------------------------- |
| `src/core/WorkerMailbox.hpp` | 新增         | 类型声明 + push()/enqueue() 内联实现                        |
| `src/core/WorkerMailbox.cpp` | 新增（建议） | drain()/flush() 实现，避免编译膨胀                          |
| `src/core/UiRuntime.hpp`     | 修改（小）   | 加 `WorkerMailbox m_mailbox` 成员 + accessor；析构处理 R1 |
| `src/core/RuntimeFacade.hpp` | 修改（小）   | `ActiveRuntimeState` 加 mailbox 字段；加 accessor         |
| `src/core/RuntimeFacade.cpp` | 修改（小）   | activateRuntime/restoreRuntime 同步 swap mailbox            |
| `src/core/TaskChain.hpp`     | 修改（小）   | QueuedTask::operator() 开头加 drain 调用                    |
| `src/common/ThreadPool.hpp`  | 修改（可选） | 加 `wait()` 接口解决 R1，若选方案 A                       |
| `tests/unittest/`            | 修改（小）   | 若有直接构造 ActiveRuntimeState 的测试，补 mailbox 字段     |

**改动类型**：接口扩展（非破坏性变更）——新增类型，修改仅在已有 friend 内部，调用方无感。

---

## 下一步与任务分解

| 优先级 | 任务                                                                           | 依赖          |
| ------ | ------------------------------------------------------------------------------ | ------------- |
| P0     | 解决 R1（生命周期）：选择方案 A/B/C 并落实                                     | 本任务前置    |
| P0     | 确认 R2（单线程模式）处理策略：mailbox bypass 还是延迟一帧可接受               | 本任务前置    |
| P1     | 实现 WorkerMailbox.hpp/.cpp + 单元测试（多线程 push/drain 验证）               | P0 完成后     |
| P1     | 修改 UiRuntime / RuntimeFacade / TaskChain                                     | P1 上述完成后 |
| P2     | InvokeCmd 去留决策：若保留则加文档约束，若移除则同步更新 WorkerCommand variant | 实现时        |
| P2     | 命名统一（Q6 建议，可在 PR review 时同步执行）                                 | 实现时        |

---

## 待确认问题

1. **R1 生命周期方案选择**：是否接受 `UiRuntime` 析构时调用 `ThreadPool::wait()`？若存在多个 UiRuntime 并发，wait 是否会造成不可接受的阻塞？答：threadpool是否可以改成多实例然后再暴露wait
2. **R2 单线程模式**：延迟一帧在单线程测试场景下是否可接受，还是必须保持行为一致？答：必须保持行为一致
3. **InvokeCmd 保留**：是否有明确的"不涉及 ECS 的纯 void() 回调"场景？若无，建议移除以简化 variant。
4. **`drain()` vs `flush()` 命名**：团队是否有偏好？可在 PR 时对齐。答：flush
5. **hasPending() 是否需要**：若仅供调试用，是否以 `#ifdef NDEBUG` 条件编译保留？答：需要
