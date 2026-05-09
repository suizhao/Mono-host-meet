# QML 落地方案（基于 LiveKit C++ 预构建 SDK）

## 1. 前提与目标
- 前提：QML 方案使用 **LiveKit C++ 预构建 SDK**（不是纯自研 RTC 协议栈）。
- 目标：快速复刻当前仓库业务行为（入会、会中、设置、录制、离会），并为后续扩展保留清晰边界。

## 2. 架构原则
- UI 与 RTC 解耦：QML 只消费 `Controller/ViewModel`，不直接操作 SDK。
- 网络契约复用现有后端：`/api/connection-details`、`/api/record/start|stop`。
- 先做最小可用（MVP），再补高级能力（E2EE、性能优化、调试可观测）。

## 3. 目录结构建议
```text
qml_meet/
├─ CMakeLists.txt
├─ src/
│  ├─ main.cpp
│  ├─ app/
│  │  ├─ app_context.h/.cpp
│  │  └─ env_config.h/.cpp
│  ├─ domain/
│  │  ├─ models/
│  │  │  ├─ connection_details.h
│  │  │  ├─ prejoin_choices.h
│  │  │  └─ meeting_state.h
│  │  └─ services/
│  │     ├─ backend_service.h
│  │     ├─ room_service.h
│  │     └─ device_service.h
│  ├─ infra/
│  │  ├─ http/backend_service_qt.h/.cpp
│  │  ├─ rtc/livekit_room_service.h/.cpp
│  │  └─ device/device_service_qt.h/.cpp
│  └─ presentation/
│     ├─ controllers/
│     │  ├─ home_controller.h/.cpp
│     │  ├─ prejoin_controller.h/.cpp
│     │  └─ room_controller.h/.cpp
│     └─ qml/
│        ├─ App.qml
│        ├─ pages/HomePage.qml
│        ├─ pages/PreJoinPage.qml
│        ├─ pages/RoomPage.qml
│        └─ components/
│           ├─ MeetingToolbar.qml
│           ├─ SettingsPanel.qml
│           └─ DeviceSelector.qml
└─ resources/
```

## 4. 模块边界（必须保持）
- `presentation`：页面与交互逻辑。
- `domain/services`：抽象业务能力接口。
- `infra/http`：后端 API 请求与响应解析。
- `infra/rtc`：LiveKit C++ SDK 适配层（连接、轨道控制、事件转发）。
- `infra/device`：设备枚举与设备切换。
- `app`：配置与依赖注入。

## 5. 关键接口（建议）

```cpp
class IBackendService {
public:
  virtual ~IBackendService() = default;
  virtual void getConnectionDetails(
    const QString& roomName,
    const QString& participantName,
    const QString& region = "",
    const QString& metadata = "") = 0;
  virtual void startRecording(const QString& roomName) = 0;
  virtual void stopRecording(const QString& roomName) = 0;
};
```

```cpp
class IRoomService {
public:
  virtual ~IRoomService() = default;
  virtual void connect(
    const QString& serverUrl,
    const QString& token,
    bool audioEnabled,
    bool videoEnabled,
    const QString& audioDeviceId = "",
    const QString& videoDeviceId = "",
    const QString& e2eePassphrase = "") = 0;
  virtual void disconnect() = 0;
  virtual void setMicEnabled(bool enabled) = 0;
  virtual void setCameraEnabled(bool enabled) = 0;
};
```

```cpp
class IDeviceService {
public:
  virtual ~IDeviceService() = default;
  virtual QVariantList listAudioInputs() = 0;
  virtual QVariantList listVideoInputs() = 0;
  virtual QVariantList listAudioOutputs() = 0;
  virtual void selectAudioInput(const QString& deviceId) = 0;
  virtual void selectVideoInput(const QString& deviceId) = 0;
  virtual void selectAudioOutput(const QString& deviceId) = 0;
};
```

## 6. 页面流程（与原仓库对齐）
- `HomePage.qml`
  - Demo：生成 roomId，进入 PreJoin。
  - Custom：输入 `liveKitUrl + token`，直接进入 Room。
- `PreJoinPage.qml`
  - 昵称、音视频开关、设备选择。
  - 调 `IBackendService::getConnectionDetails`。
- `RoomPage.qml`
  - 调 `IRoomService::connect`。
  - 显示本地与远端画面。
  - 工具栏：麦克风、摄像头、设置、离会。
  - 设置：设备切换、录制启停。

## 7. LiveKit C++ SDK 接入关注点
- 连接生命周期：
  - `connect` 成功后才能允许会中控制。
  - 断开事件触发时返回首页并释放资源。
- 媒体控制：
  - 统一通过 `IRoomService` 暴露开关接口，避免 QML 直接调用 SDK。
- 设备能力：
  - 输入/输出设备列表缓存 + 热更新（设备插拔）。
- 线程模型：
  - SDK 回调线程切回 Qt 主线程再更新 UI 状态。
- 错误模型：
  - SDK 错误码映射为业务错误类型，保证提示文案稳定。

## 8. E2EE 与性能优化（分阶段）
- MVP 阶段：
  - E2EE 接口先预留字段（`e2eePassphrase`），能力可按 SDK 支持逐步接入。
  - 性能策略先做基础监控与手动降档。
- 增强阶段：
  - 对齐 Web 方案：连接前注入密钥、失败提示、低功耗自动降级。

## 9. 迭代计划（每项可单 PR）
1. 项目骨架与依赖注入
2. 后端 API 客户端（connection-details + record）
3. LiveKit C++ SDK 基础 connect/disconnect
4. 本地音视频轨道控制
5. 远端画面渲染与参与者列表
6. 设备枚举与切换
7. 设置面板 + 录制按钮
8. 错误处理与离会流程
9. E2EE 接口打通（按 SDK 能力）
10. 性能优化与回归测试

## 10. 验收标准
- 可跑通 Demo 与 Custom 两条入会路径。
- `connection-details` 契约字段完全一致。
- 麦克风/摄像头/设备切换能力可用。
- 录制启停请求可用并反馈明确。
- 断开时可安全回首页并清理资源。
