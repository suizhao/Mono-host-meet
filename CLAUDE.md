# MultiLive 架构说明

## 目录骨架

```text
MultiLive/
├─ CMakeLists.txt
├─ CLAUDE.md
└─ src/
   ├─ main.cpp
   ├─ tools/
   │  └─ device_probe_main.cpp
   ├─ app/
   │  ├─ app_context.h/.cpp
   │  └─ env_config.h/.cpp
   ├─ domain/
   │  └─ services/
   │     ├─ backend_service.h
   │     ├─ room_service.h
   │     └─ device_service.h
   ├─ infra/
   │  ├─ http/backend_service_qt.h/.cpp
   │  ├─ rtc/livekit_room_service.h/.cpp
   │  └─ device/device_service_qt.h/.cpp
   └─ presentation/
      ├─ controllers/
      │  ├─ home_controller.h/.cpp
      │  ├─ prejoin_controller.h/.cpp
      │  ├─ room_controller.h/.cpp
      │  └─ video_tile_model.h/.cpp
      └─ qml/
         ├─ App.qml
         ├─ frame_image_provider.h/.cpp
         └─ pages/HomePage.qml
```

## 文件职责

- `src/main.cpp`：Qt 应用入口，创建 `AppContext` 并注入到 QML。
- `src/tools/device_probe_main.cpp`：独立设备探测进程入口，负责调用 `Qt Multimedia` 枚举真实设备并输出 JSON。
- `src/app/env_config.*`：读取运行环境配置（后端地址、LiveKit 地址）。
- `src/app/app_context.*`：依赖注入容器，统一装配 service 与 controller。
- `src/domain/services/*.h`：领域抽象接口，定义后端、房间、设备能力边界。
- `src/infra/http/backend_service_qt.*`：后端 API 适配层，负责 `connection-details` 与 `record start/stop` 的真实 HTTP 调用与错误映射。
- `src/infra/rtc/livekit_room_service.*`：LiveKit C++ SDK 房间适配层，负责 `initialize`、`connect`、`disconnect`、本地音视频轨道开关、Windows DXGI/GDI 屏幕采集推流、远端多路视频轨道订阅与多宫格帧缓存。
- `src/infra/device/device_service_qt.*`：设备能力适配层，通过独立探测可执行文件拉取真实设备（麦克风/摄像头/扬声器）并持久记录用户选择；探测失败时自动回退占位设备。
- `src/presentation/controllers/*.h/.cpp`：UI 控制器，承接交互并调用领域能力。
- `src/presentation/controllers/video_tile_model.*`：会中视频宫格模型，维护固定 tile 行并做增量更新，减少 delegate 重建。
- `src/presentation/qml/frame_image_provider.*`：QML 图片提供器，按 tileId 提供最新帧，替代大体积 data URL 直传。
- `src/presentation/qml/App.qml`：应用根窗口与页面栈入口。
- `src/presentation/qml/pages/HomePage.qml`：首页（Demo / Custom 骨架入口）。

## 依赖方向与边界

- 单向依赖：`presentation -> domain interfaces -> infra implementations`。
- `app_context` 作为唯一装配点：避免 QML 直接依赖 SDK 或网络细节。
- QML 不直接操作 LiveKit SDK：所有媒体行为必须经 `RoomController/IRoomService`。

## 关键决策

- 先落地可运行骨架，再按迭代计划逐步填充能力，避免一次性耦合过深。
- 用接口隔离 `domain` 与 `infra`，为后续替换实现（Mock/真机/多平台）留出口。
- 录制、连接详情、设备管理均先保留稳定方法签名，再逐步补齐真实逻辑。
- 后端接口统一通过 `IBackendService` 信号回传结果，UI 只消费业务状态，不感知网络细节。
- 房间连接通过 `IRoomService` 的同步返回值与 `lastError()` 反馈状态，避免 Qt 元对象跨层耦合。
- Windows 下使用预构建 LiveKit SDK 时，Debug 构建仅用于 UI/流程调试；真实房间连接需在 Release 构建验证，避免 MSVC Debug CRT ABI 冲突。
- 音视频开关由 `RoomController -> IRoomService` 串行触发，只有 SDK 调用成功才更新 UI 状态，避免界面与真实轨道状态漂移。
- 远端状态采用控制器定时拉取（polling）模型：Service 负责采集并缓存，QML 消费结构化 `tile` 列表进行多宫格渲染。
- 渲染链路改为 `ImageProvider`：Controller 把帧写入 provider，并下发轻量 URL（`image://multilive/<tile>?v=<n>`），降低闪烁与内存抖动。
- 远端视频回调不直接捕获 Service 对象，改为共享状态弱引用 + 帧率节流，降低退出期并发访问导致的崩溃风险。
- 远端视频改为“连续帧节流刷新”策略，并在 UI 显示当前画面来源（参与者 + 摄像头/共享），便于单人房间与多人房间统一观察。
- 设备管理采用 `Qt Multimedia` 枚举能力，但在独立子进程中执行探测，隔离平台插件崩溃风险。
- 主应用进程不再链接 `Qt Multimedia`，设备探测由独立 `multilive_device_probe` 可执行文件承担，进一步降低主进程崩溃面。
- 设置相关能力统一收敛到可折叠设置面板（设备 + 录制），主界面保留核心会中操作按钮。
- 设备枚举默认走真实设备路径；当探测异常、进程异常退出或返回空列表时，退回默认占位项以保证流程可用。
- HomeController 内置会话状态机（入会中/在房间/录制请求中）与统一错误横幅，避免并发操作导致状态错乱。
- 离会流程执行 UI 级会话状态重置（远端画面、参与者、轨道状态、错误信息），保证回到可复用的初始态。
- 共享屏幕改为 Windows 真桌面采集：优先走 DXGI Desktop Duplication（D3D11），失败时回退 GDI 抓取虚拟桌面帧并发布 `SOURCE_SCREENSHARE` 轨道；非 Windows 环境保留测试帧回退。

## 变更记录

- 2026-05-10：完成 MVP 迭代 1（项目骨架与依赖注入）。
- 2026-05-10：完成 MVP 迭代 2（后端 API 客户端打通，含连接详情与录制接口）。
- 2026-05-10：完成 MVP 迭代 3（LiveKit C++ SDK connect/disconnect 打通，并改为同步状态返回模型）。
- 2026-05-10：完成 MVP 迭代 4（本地音视频轨道控制：麦克风/摄像头开关）。
- 2026-05-10：完成 MVP 迭代 5（远端参与者列表与远端视频画面渲染）。
- 2026-05-10：完成 MVP 迭代 6（设备枚举与切换：麦克风/摄像头/扬声器）。
- 2026-05-10：完成 MVP 迭代 7（设置面板 + 录制按钮状态化整合）。
- 2026-05-10：完成 MVP 迭代 8（错误处理与离会流程完善）。
- 2026-05-10：补充 MVP 共享屏幕能力（启停控制 + 轨道发布）。
- 2026-05-10：设备枚举稳定性修复：改为 `--device-probe` 子进程探测，避免主进程因多媒体插件崩溃退出。
- 2026-05-10：进一步稳定性加固：设备探测拆分为独立二进制 `multilive_device_probe`，主程序彻底移除 `Qt Multimedia` 运行时依赖。
- 2026-05-10：远端渲染策略升级：优先显示屏幕共享，其次摄像头；支持将本地参与者纳入候选来源，并在界面显示画面来源文本。
- 2026-05-10：远端画面升级为多宫格并发渲染：每个参与者/轨道独立 tile，摄像头与共享可同时上屏，支持单人房间自预览。
- 2026-05-10：共享屏幕采集升级：`setScreenShareEnabled()` 切换为 Windows 桌面实时采集并推流，替代测试图案流。
- 2026-05-10：共享屏幕采集再次升级：Windows 采集主路径改为 DXGI Desktop Duplication（D3D11），保留 GDI 回退链路以提升兼容性。
- 2026-05-10：会中渲染稳态优化：引入固定 tile 模型 + `FrameImageProvider`，降低多宫格闪烁。
- 2026-05-10：`main.cpp` 对 `HomeController` / `RoomController` / `PrejoinController` 调用 `qmlRegisterUncreatableType`，避免 QML 读取控制器属性时在 `QByteArray`/字符串转换路径上访问违例；源码行尾保持 LF/CRLF 规范，避免 MSVC C4335（误检为 Mac CR）。
