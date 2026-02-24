# Fallback Renderer 设计方案（SDL_Renderer 后备）

## 目标

为不支持 D3D12/Vulkan 的平台提供轻量级 2D 渲染后备方案。

## 技术选型：SDL_Renderer

### 为什么选择 SDL_Renderer？

1. **零依赖**：SDL3 自带，无需额外库
2. **广泛兼容**：支持 D3D11 / OpenGL 2.1+ / OpenGL ES / 软件渲染
3. **成熟稳定**：已在数千个项目中验证
4. **体积小**：实现代码 < 500 行

### 支持的平台

- Windows 7/8/10/11 (D3D11 / OpenGL)
- Linux (OpenGL 2.1+)
- macOS (OpenGL / Metal via SDL_Renderer)
- Android (OpenGL ES 2.0)
- 树莓派 (OpenGL ES)
- 虚拟机 (软件渲染)

---

## 架构设计

### 1. 渲染器接口抽象

```cpp
// src/ui/interface/IBackendRenderer.hpp
class IBackendRenderer {
public:
    virtual ~IBackendRenderer() = default;
  
    virtual bool initialize(SDL_Window*) = 0;
    virtual void cleanup() = 0;
  
    virtual void beginFrame(const Color& clearColor) = 0;
    virtual void endFrame() = 0;
  
    // 基础图形
    virtual void drawRect(const Rect&, const Color&) = 0;
    virtual void drawRoundedRect(const Rect&, float radius, const Color&) = 0;
    virtual void drawTexture(SDL_Texture*, const Rect& src, const Rect& dst, const Color& tint) = 0;
  
    // 批次渲染（可选优化）
    virtual void drawBatch(const RenderBatch&) = 0;
  
    virtual BackendType getType() const = 0;
};
```

### 2. GPU 渲染器（当前实现）

```cpp
// src/ui/renderers/GPUBackendRenderer.hpp
class GPUBackendRenderer : public IBackendRenderer {
private:
    std::unique_ptr<DeviceManager> m_deviceManager;
    std::unique_ptr<Batcher> m_batcher;
    // ... 现有逻辑不变
  
public:
    BackendType getType() const override { return BackendType::GPU; }
    // 使用现有 SDL_GPU 实现
};
```

### 3. SDL_Renderer 后备实现

```cpp
// src/ui/renderers/FallbackBackendRenderer.hpp
class FallbackBackendRenderer : public IBackendRenderer {
private:
    SDL_Renderer* m_renderer = nullptr;
    std::unordered_map<uint64_t, SDL_Texture*> m_textureCache;
  
public:
    bool initialize(SDL_Window* window) override {
        // 按优先级尝试驱动
        const char* drivers[] = {"direct3d11", "opengl", "opengles2", "software"};
      
        for (const char* driver : drivers) {
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, driver);
            m_renderer = SDL_CreateRenderer(window, driver);
            if (m_renderer) {
                Logger::info("Fallback renderer: {}", driver);
                return true;
            }
        }
        return false;
    }
  
    void drawRoundedRect(const Rect& rect, float radius, const Color& color) override {
        // 简化为普通矩形（圆角需要额外顶点数据）
        // 或使用 SDL_RenderGeometry 绘制近似圆角
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        SDL_FRect sdlRect = {rect.x, rect.y, rect.w, rect.h};
        SDL_RenderFillRect(m_renderer, &sdlRect);
    }
  
    BackendType getType() const override { return BackendType::Fallback; }
};
```

---

## 功能对比

| 功能     | GPU Renderer      | Fallback Renderer   |
| -------- | ----------------- | ------------------- |
| 圆角矩形 | SDF Shader (精确) | 近似多边形 / 直角   |
| 阴影     | Shader 实时       | 预渲染纹理          |
| 文本渲染 | FreeType → 纹理  | FreeType → 纹理 ✅ |
| 抗锯齿   | Shader MSAA       | SDL_RenderGeometry  |
| 性能     | 60+ FPS           | 30-60 FPS           |
| 兼容性   | D3D12/Vulkan      | D3D11/OpenGL/软件   |

---

## 降级策略

### 自动检测流程

```cpp
// src/ui/systems/RenderSystem.cpp
bool RenderSystem::initializeBackend(SDL_Window* window) {
    // 1. 尝试 GPU 后端
    auto gpuRenderer = std::make_unique<GPUBackendRenderer>();
    if (gpuRenderer->initialize(window)) {
        m_backend = std::move(gpuRenderer);
        return true;
    }
  
    // 2. 降级到 SDL_Renderer
    Logger::warn("GPU backend unavailable, falling back to SDL_Renderer");
    auto fallbackRenderer = std::make_unique<FallbackBackendRenderer>();
    if (fallbackRenderer->initialize(window)) {
        m_backend = std::move(fallbackRenderer);
        showFallbackNotice(); // 可选：通知用户使用兼容模式
        return true;
    }
  
    // 3. 彻底失败
    Logger::error("All rendering backends failed!");
    return false;
}
```

---

## 实现优先级

### 阶段 1：核心功能（必需）

- [ ] 接口抽象 `IBackendRenderer`
- [ ] `FallbackBackendRenderer` 基础实现
- [ ] 矩形/文本/纹理渲染
- [ ] DeviceManager 集成自动降级

### 阶段 2：视觉优化（可选）

- [ ] 圆角矩形近似（分段贝塞尔）
- [ ] 简单阴影（预渲染高斯模糊纹理）
- [ ] 渐变色支持

### 阶段 3：性能优化（可选）

- [ ] 批次合并（减少 draw call）
- [ ] 纹理图集复用
- [ ] Dirty rectangle 优化

---

## 性能预期

### 测试场景：卡牌游戏 UI

- **100 个 Widget**（按钮、标签、卡牌）
- **1920x1080 分辨率**
- **Windows 10 + 集成显卡**

| 后端              | FPS  | CPU 占用 |
| ----------------- | ---- | -------- |
| GPU (D3D12)       | 120+ | 5%       |
| Fallback (D3D11)  | 60   | 15%      |
| Fallback (OpenGL) | 55   | 18%      |
| Fallback (软件)   | 30   | 40%      |

**结论：** 除软件渲染外，性能完全可接受。

---

## 向后兼容保证

### 对现有代码的影响

1. **零破坏性**：现有 GPU 渲染器逻辑不变
2. **透明切换**：RenderSystem 自动选择后端
3. **API 统一**：上层 UI 系统无感知

### 测试计划

- [ ] GPU 后端回归测试
- [ ] Fallback 后端功能测试
- [ ] 虚拟机环境测试（无 D3D12）
- [ ] Windows 7 实机测试

---

## 文件结构

```
src/ui/
├── interface/
│   └── IBackendRenderer.hpp        [新增]
├── renderers/
│   ├── GPUBackendRenderer.hpp      [重构现有]
│   └── FallbackBackendRenderer.hpp [新增]
└── systems/
    └── RenderSystem.cpp            [修改：集成后端选择]
```

---

## 开发时间估算

- **接口抽象**：2 小时
- **FallbackRenderer 实现**：8 小时
- **集成测试**：4 小时
- **文档 + 示例**：2 小时

**总计：约 2 个工作日**

---

## 替代方案对比

| 方案         | 体积   | 兼容性     | 开发成本 | 性能 |
| ------------ | ------ | ---------- | -------- | ---- |
| SDL_Renderer | +50KB  | ⭐⭐⭐⭐⭐ | 低       | 中   |
| SwiftShader  | +60MB  | ⭐⭐⭐⭐   | 极低     | 低   |
| Cairo        | +5MB   | ⭐⭐⭐     | 中       | 低   |
| 自研软件渲染 | +200KB | ⭐⭐⭐⭐⭐ | 高       | 低   |

**推荐：SDL_Renderer** 是最佳平衡点。

---

## FAQ

**Q: 为什么不用 OpenGL 直接？**
A: SDL_Renderer 已封装 OpenGL/D3D11，且自带软件后备，省去大量平台适配代码。

**Q: 圆角效果会丢失吗？**
A: 可以用多边形近似，或在 Fallback 模式禁用圆角（性能优先）。

**Q: 会影响现有 GPU 性能吗？**
A: 不会。GPU 路径完全独立，只在检测失败时才启用 Fallback。

**Q: 如何在配置中强制使用 Fallback？**
A: 添加环境变量 `PESTMANKILL_FORCE_FALLBACK=1` 即可。
