# MultiLive 架构说明

## 目录结构

```text
MultiLive/
├─ CMakeLists.txt
├─ CLAUDE.md
├─ cmake/
│  └─ LiveKitSDK.cmake
└─ src/
   ├─ main.cpp
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
      │  └─ room_controller.h/.cpp
      └─ qml/
         ├─ App.qml
         └─ pages/HomePage.qml
```

## 文件职责

- `src/main.cpp`: Qt 程序入口，创建 `AppContext` 并注入到 QML。
- `src/app/env_config.*`: 读取环境变量配置，提供运行时默认值。
- `src/app/app_context.*`: 依赖注入根，组装 service 与 controller。
- `src/domain/services/*.h`: 业务能力抽象接口，约束上层依赖方向。
- `src/infra/http/backend_service_qt.*`: 后端 API 访问层（当前为 MVP 占位实现）。
- `src/infra/rtc/livekit_room_service.*`: LiveKit RTC 适配层（当前为 MVP 占位实现）。
- `src/infra/device/device_service_qt.*`: 设备能力适配层（当前返回默认设备占位数据）。
- `src/presentation/controllers/*.h/.cpp`: QML 可绑定控制器，承载页面状态与用户动作。
- `src/presentation/qml/App.qml`: 应用根视图。
- `src/presentation/qml/pages/HomePage.qml`: MVP 首页，演示 Demo/Custom 动作入口。

## 依赖边界

- `presentation -> domain`: 通过 controller 依赖接口，不直接依赖 SDK/网络细节。
- `infra -> domain`: `infra` 实现 `domain/services` 定义的抽象能力。
- `app -> infra + presentation`: `app_context` 负责组装依赖图。
- `qml -> controller`: QML 只调用 controller，不直接访问 service 与 SDK。

## 关键架构决策

- 使用「接口 + 适配层」隔离 LiveKit SDK 与网络实现，减少 UI 层耦合。
- 使用 `AppContext` 作为单一依赖注入入口，统一生命周期管理。
- MVP 阶段先提供可编译可运行骨架，后续迭代逐步替换占位实现。

## 开发规范（本项目）

- 变量/类/函数命名使用英文；注释与文档使用中文。
- UI 不直接触达 LiveKit SDK，必须经 `controller -> service` 路径。
- 新增功能先落在 `domain/services` 接口，再补 `infra` 实现。

## 变更日志

- `2026-05-10`: 完成迭代第 1 步（项目骨架与依赖注入）MVP 版本。
