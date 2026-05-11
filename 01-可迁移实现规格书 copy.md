# LiveKit Meet 可迁移实现规格书（QML / Flutter）

## 执行摘要
- 当前仓库是 `Next.js + @livekit/components-react + livekit-client + livekit-server-sdk` 的会议应用样板。
- 主链路分两种：`Demo`（自动建房）与 `Custom`（手填 `liveKitUrl + token`）。
- 标准入会流程：`PreJoin` 收集用户选择 -> `GET /api/connection-details` -> `Room.connect()`。
- 会中核心能力：音视频开关、设备切换、设置菜单、录制控制、调试面板、低功耗优化。
- E2EE 通过 URL hash 中 passphrase 初始化 worker 与 key provider。
- 录制 API 为演示实现，代码明确标注不可直接用于生产。
- 迁移重点是保持行为等价，不是 1:1 还原 Web 组件。
- Flutter 可直接走 LiveKit 客户端能力；QML 采用 LiveKit C++ 预构建 SDK 作为 RTC 基座。
- 后端契约（连接详情与录制接口）可直接复用，降低迁移风险。
- 建议按模块逐步迁移：先打通入会，再完善会中和增强功能。

## 1. 目录与模块职责

### `app/`
- `app/layout.tsx`：全局布局、样式、`Toaster` 注入。
- `app/page.tsx`：首页入口（Demo/Custom）。
- `app/rooms/[roomName]/page.tsx`：房间参数解析与传递。
- `app/rooms/[roomName]/PageClientImpl.tsx`：标准入会与会中主逻辑。
- `app/custom/page.tsx` + `app/custom/VideoConferenceClientImpl.tsx`：自定义连接模式。
- `app/api/connection-details/route.ts`：生成参会 token 并返回连接信息。
- `app/api/record/start/route.ts` / `stop/route.ts`：录制启停。

### `lib/`
- `client-utils.ts`：房间号、passphrase 处理、低功耗判断。
- `getLiveKitURL.ts`：region 到 LiveKit Cloud 域名映射。
- `useSetupE2EE.ts`：E2EE worker 初始化。
- `usePerfomanceOptimiser.ts`：低性能自动降级。
- `SettingsMenu.tsx`/`CameraSettings.tsx`/`MicrophoneSettings.tsx`：设备与音视频设置。
- `KeyboardShortcuts.tsx`：快捷键切麦/切摄像头。
- `RecordingIndicator.tsx`：录制态提示。
- `Debug.tsx`：调试信息与日志扩展。

## 2. 用户流程与触发位置
- 打开首页：`app/page.tsx` `Page`
- Demo 入会：`DemoMeetingTab.startMeeting` 跳 `/rooms/{roomName}`
- Custom 入会：`CustomConnectionTab.onSubmit` 跳 `/custom?liveKitUrl=...&token=...`
- 标准 PreJoin：`PageClientImpl.handlePreJoinSubmit`
- 请求连接详情：`fetch(CONN_DETAILS_ENDPOINT)` -> `/api/connection-details`
- 建连：`VideoConferenceComponent` 中 `room.connect(serverUrl, participantToken, connectOptions)`
- 会中控制：`VideoConference` + `SettingsMenu` + `KeyboardShortcuts`
- 离会：`RoomEvent.Disconnected` -> `router.push('/')`

## 3. 功能清单

| 功能名 | 用户可见行为 | 涉及组件/页面 | 依赖 SDK/API | 状态与副作用 |
|---|---|---|---|---|
| Demo 开会 | 一键进入随机房间 | `app/page.tsx` | `next/navigation` | 路由切换，可能附带 E2EE hash |
| Custom 连接 | 输入 URL/Token 入会 | `app/page.tsx`, `app/custom/page.tsx` | `livekit-client` | 直接建连，不经 token API |
| 标准入会 | 先 PreJoin 再入会 | `PageClientImpl` | `@livekit/components-react`, `/api/connection-details` | 先拉连接信息再 connect |
| 音视频开关 | 麦克风/摄像头开关 | `MicrophoneSettings`, `CameraSettings` | `TrackToggle`, `useTrackToggle` | 本地轨道发布状态变化 |
| 设备切换 | 切输入输出设备 | `SettingsMenu` | `MediaDeviceMenu` | 设备重绑定 |
| 录制控制 | 启停录制 | `SettingsMenu` | `/api/record/start`, `/api/record/stop` | 请求后端 egress，录制态变化 |
| E2EE | 加密会议 | `useSetupE2EE`, `PageClientImpl` | `ExternalE2EEKeyProvider` | 设置 key + 启用加密 |
| 低功耗优化 | CPU 紧张自动降质 | `usePerfomanceOptimiser.ts` | `livekit-client` 事件 | 发布/订阅质量降级 |
| 调试能力 | 打开调试覆盖层 | `Debug.tsx` | `tinykeys`, datadog(可选) | `Shift+D` 切换调试 UI |

## 4. 网络与后端契约

### `GET /api/connection-details`
- 请求参数：
  - `roomName`（required）
  - `participantName`（required）
  - `metadata`（optional）
  - `region`（optional）
- 响应：
  - `serverUrl`
  - `roomName`
  - `participantName`
  - `participantToken`
- 状态码：
  - `400` 缺少必要参数
  - `500` 配置错误或内部错误

### `GET /api/record/start`
- 请求参数：`roomName`（required）
- 状态码：
  - `200` 成功
  - `403` 参数缺失
  - `409` 已在录制
  - `500` 内部错误

### `GET /api/record/stop`
- 请求参数：`roomName`（required）
- 状态码：
  - `200` 成功
  - `403` 参数缺失
  - `404` 无活动录制
  - `500` 内部错误

## 5. 媒体与设备实现要点
- 摄像头/麦克风/扬声器：依赖 `MediaDeviceMenu`。
- 背景效果：`BackgroundBlur` 与 `VirtualBackground`。
- 降噪：`useKrispNoiseFilter`，低功耗设备默认关闭增强降噪。
- E2EE：从 `location.hash` 读取 passphrase，初始化 worker 后再启用加密。
- 性能：监听 `LocalTrackCpuConstrained`，自动 `prioritizePerformance` 与 `VideoQuality.LOW`。

## 6. UI 组件树（主路径）
- 首页 `/`
  - Header
  - Tabs
    - Demo tab
    - Custom tab
  - Footer
- 房间页 `/rooms/[roomName]`
  - `PreJoin`（未连接前）
  - `RoomContext.Provider`
    - `KeyboardShortcuts`
    - `VideoConference`
    - `DebugMode`
    - `RecordingIndicator`

## 7. 配置与部署
- 必需：`LIVEKIT_API_KEY`, `LIVEKIT_API_SECRET`, `LIVEKIT_URL`
- 可选：`S3_*`, `NEXT_PUBLIC_SHOW_SETTINGS_MENU`, `NEXT_PUBLIC_LK_RECORD_ENDPOINT`, `NEXT_PUBLIC_DATADOG_*`
- 命令：`pnpm dev`, `pnpm build`, `pnpm start`, `pnpm lint`, `pnpm test`

## 8. 迁移映射建议（QML / Flutter）
- Flutter：优先复用 LiveKit 客户端生态，快速还原业务行为。
- QML：使用 LiveKit C++ 预构建 SDK 作为 RTC 层，UI 与状态管理按 Qt/QML 重建。
- **若目标部署为 Docker 自建 LiveKit Server（非 LiveKit Cloud）**：`LIVEKIT_URL` 指向自建 `wss://...`；`region` 相关逻辑（仅对 `livekit.cloud` 主机名生效）在自建场景通常可忽略；详见 `02-Flutter落地方案.md`、`03-QML-LiveKitCppSDK落地方案.md` 中的部署前提。
- 两端共同原则：先行为等价，再样式等价，再做增强能力（调试、性能、可观测性）。

## 9. 明确的演示用 / 待改进点
- `app/api/record/start|stop` 明确写明无鉴权，仅演示用途。
- `app/rooms/[roomName]/page.tsx` 存在 `FIXME`（playground 模式 region 限制未做）。
- 快捷键注释写法与实现细节存在不一致（注释含 Shift，逻辑未校验 Shift）。
