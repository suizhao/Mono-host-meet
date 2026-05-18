#include "infra/rtc/livekit_room_service.h"

#include <livekit/livekit.h>
#include <livekit/audio_stream.h>
#include <livekit/local_audio_track.h>
#include <livekit/local_participant.h>
#include <livekit/local_video_track.h>
#include <livekit/room.h>
#include <livekit/track.h>
#include <livekit/video_frame.h>
#include <livekit/video_source.h>

#include <QCoreApplication>
#include <livekit/video_stream.h>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QBuffer>
#include <QTimer>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIODevice>
#include <QImage>
#include <QMediaDevices>
#include <QMutexLocker>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cstring>
#include <exception>
#include <chrono>
#include <thread>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#endif

namespace {
constexpr int kScreenShareDefaultWidth = 1280;
constexpr int kScreenShareDefaultHeight = 720;
constexpr int kScreenShareFps = 30;
constexpr qint64 kPreviewUpdateIntervalMs = 80;

QString trackSourceLabel(livekit::TrackSource source) {
  return source == livekit::TrackSource::SOURCE_SCREENSHARE ? "屏幕共享" : "摄像头";
}

QString streamKey(const QString& identity, livekit::TrackSource source) {
  const QString suffix =
      source == livekit::TrackSource::SOURCE_SCREENSHARE ? "share" : "camera";
  return identity + "|" + suffix;
}

#ifdef Q_OS_WIN
using Microsoft::WRL::ComPtr;

struct DxgiCaptureContext {
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<IDXGIOutputDuplication> duplication;
  ComPtr<ID3D11Texture2D> stagingTexture;
  int width = 0;
  int height = 0;
};

struct GdiCaptureContext {
  HDC screenDc = nullptr;
  HDC memoryDc = nullptr;
  HBITMAP bitmap = nullptr;
  void* pixels = nullptr;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct DesktopCaptureContext {
  enum class Backend { None, Dxgi, Gdi };
  Backend backend = Backend::None;
  DxgiCaptureContext dxgi;
  GdiCaptureContext gdi;
  int width = 0;
  int height = 0;
};

void releaseDesktopCapture(DesktopCaptureContext& ctx) {
  if (ctx.gdi.bitmap) {
    DeleteObject(ctx.gdi.bitmap);
    ctx.gdi.bitmap = nullptr;
  }
  if (ctx.gdi.memoryDc) {
    DeleteDC(ctx.gdi.memoryDc);
    ctx.gdi.memoryDc = nullptr;
  }
  if (ctx.gdi.screenDc) {
    ReleaseDC(nullptr, ctx.gdi.screenDc);
    ctx.gdi.screenDc = nullptr;
  }
  ctx.gdi.pixels = nullptr;
  ctx.dxgi.stagingTexture.Reset();
  ctx.dxgi.duplication.Reset();
  ctx.dxgi.context.Reset();
  ctx.dxgi.device.Reset();
  ctx.backend = DesktopCaptureContext::Backend::None;
  ctx.width = 0;
  ctx.height = 0;
}

bool initDxgiCapture(DesktopCaptureContext& ctx) {
  ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
    return false;
  }

  ComPtr<IDXGIAdapter1> adapter;
  if (FAILED(factory->EnumAdapters1(0, &adapter))) {
    return false;
  }

  ComPtr<IDXGIOutput> output;
  if (FAILED(adapter->EnumOutputs(0, &output))) {
    return false;
  }

  ComPtr<IDXGIOutput1> output1;
  if (FAILED(output.As(&output1))) {
    return false;
  }

  DXGI_OUTPUT_DESC outputDesc{};
  if (FAILED(output->GetDesc(&outputDesc))) {
    return false;
  }

  const int width = outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left;
  const int height = outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;
  if (width <= 0 || height <= 0) {
    return false;
  }

  D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
  constexpr D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
  HRESULT hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                 D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
                                 static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
                                 &ctx.dxgi.device, &featureLevel, &ctx.dxgi.context);
  if (FAILED(hr)) {
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                           D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
                           static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
                           &ctx.dxgi.device, &featureLevel, &ctx.dxgi.context);
    if (FAILED(hr)) {
      return false;
    }
  }

  if (FAILED(output1->DuplicateOutput(ctx.dxgi.device.Get(), &ctx.dxgi.duplication))) {
    return false;
  }

  D3D11_TEXTURE2D_DESC desc{};
  desc.Width = static_cast<UINT>(width);
  desc.Height = static_cast<UINT>(height);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  if (FAILED(ctx.dxgi.device->CreateTexture2D(&desc, nullptr, &ctx.dxgi.stagingTexture))) {
    return false;
  }

  ctx.backend = DesktopCaptureContext::Backend::Dxgi;
  ctx.width = width;
  ctx.height = height;
  ctx.dxgi.width = width;
  ctx.dxgi.height = height;
  return true;
}

bool initGdiCapture(DesktopCaptureContext& ctx) {
  ctx.gdi.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
  ctx.gdi.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  ctx.gdi.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  ctx.gdi.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (ctx.gdi.width <= 0 || ctx.gdi.height <= 0) {
    ctx.gdi.width = kScreenShareDefaultWidth;
    ctx.gdi.height = kScreenShareDefaultHeight;
  }

  ctx.gdi.screenDc = GetDC(nullptr);
  if (!ctx.gdi.screenDc) {
    return false;
  }

  ctx.gdi.memoryDc = CreateCompatibleDC(ctx.gdi.screenDc);
  if (!ctx.gdi.memoryDc) {
    releaseDesktopCapture(ctx);
    return false;
  }

  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = ctx.gdi.width;
  bmi.bmiHeader.biHeight = -ctx.gdi.height; // 顶朝上，便于逐行读取
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  ctx.gdi.bitmap =
      CreateDIBSection(ctx.gdi.screenDc, &bmi, DIB_RGB_COLORS, &ctx.gdi.pixels, nullptr, 0);
  if (!ctx.gdi.bitmap || !ctx.gdi.pixels) {
    releaseDesktopCapture(ctx);
    return false;
  }

  if (!SelectObject(ctx.gdi.memoryDc, ctx.gdi.bitmap)) {
    releaseDesktopCapture(ctx);
    return false;
  }

  ctx.backend = DesktopCaptureContext::Backend::Gdi;
  ctx.width = ctx.gdi.width;
  ctx.height = ctx.gdi.height;
  return true;
}

bool initDesktopCapture(DesktopCaptureContext& ctx) {
  if (initDxgiCapture(ctx)) {
    return true;
  }
  releaseDesktopCapture(ctx);
  if (initGdiCapture(ctx)) {
    return true;
  }
  return false;
}

bool captureDesktopFrame(const DesktopCaptureContext& ctx, livekit::VideoFrame& frame) {
  if (ctx.backend == DesktopCaptureContext::Backend::Dxgi) {
    if (!ctx.dxgi.duplication || !ctx.dxgi.context || !ctx.dxgi.stagingTexture ||
        frame.width() != ctx.dxgi.width || frame.height() != ctx.dxgi.height) {
      return false;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = ctx.dxgi.duplication->AcquireNextFrame(5, &frameInfo, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
      return false;
    }
    if (FAILED(hr)) {
      return false;
    }

    bool ok = false;
    do {
      ComPtr<ID3D11Texture2D> desktopTexture;
      if (FAILED(resource.As(&desktopTexture)) || !desktopTexture) {
        break;
      }

      ctx.dxgi.context->CopyResource(ctx.dxgi.stagingTexture.Get(), desktopTexture.Get());

      D3D11_MAPPED_SUBRESOURCE mapped{};
      if (FAILED(ctx.dxgi.context->Map(ctx.dxgi.stagingTexture.Get(), 0, D3D11_MAP_READ, 0,
                                       &mapped))) {
        break;
      }

      std::uint8_t* dst = frame.data();
      if (!dst) {
        ctx.dxgi.context->Unmap(ctx.dxgi.stagingTexture.Get(), 0);
        break;
      }

      for (int y = 0; y < ctx.dxgi.height; ++y) {
        const auto* srcRow = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch;
        std::uint8_t* dstRow = dst + y * ctx.dxgi.width * 4;
        for (int x = 0; x < ctx.dxgi.width; ++x) {
          const int offset = x * 4;
          dstRow[offset + 0] = srcRow[offset + 2];
          dstRow[offset + 1] = srcRow[offset + 1];
          dstRow[offset + 2] = srcRow[offset + 0];
          dstRow[offset + 3] = 255;
        }
      }

      ctx.dxgi.context->Unmap(ctx.dxgi.stagingTexture.Get(), 0);
      ok = true;
    } while (false);

    ctx.dxgi.duplication->ReleaseFrame();
    return ok;
  }

  if (ctx.backend == DesktopCaptureContext::Backend::Gdi) {
    if (!ctx.gdi.screenDc || !ctx.gdi.memoryDc || !ctx.gdi.pixels || frame.width() != ctx.width ||
        frame.height() != ctx.height) {
      return false;
    }

    if (!BitBlt(ctx.gdi.memoryDc, 0, 0, ctx.width, ctx.height, ctx.gdi.screenDc, ctx.gdi.x,
                ctx.gdi.y, SRCCOPY | CAPTUREBLT)) {
      return false;
    }

    const auto* src = static_cast<const std::uint8_t*>(ctx.gdi.pixels); // BGRA
    std::uint8_t* dst = frame.data();                                    // RGBA
    if (!dst) {
      return false;
    }

    const int pixelCount = ctx.width * ctx.height;
    for (int i = 0; i < pixelCount; ++i) {
      const int base = i * 4;
      dst[base + 0] = src[base + 2];
      dst[base + 1] = src[base + 1];
      dst[base + 2] = src[base + 0];
      dst[base + 3] = 255;
    }
    return true;
  }

  return false;
}
#endif
} // namespace

LiveKitRoomService::LiveKitRoomService() = default;

LiveKitRoomService::~LiveKitRoomService() {
  disconnect();
  // MVP 阶段避免在析构时触发全局 shutdown，减少退出阶段崩溃风险。
}

bool LiveKitRoomService::connect(const QString& serverUrl,
                                 const QString& token,
                                 bool audioEnabled,
                                 bool videoEnabled,
                                 const QString& audioDeviceId,
                                 const QString& videoDeviceId,
                                 const QString& e2eePassphrase) {
#ifdef _DEBUG
  Q_UNUSED(serverUrl);
  Q_UNUSED(token);
  Q_UNUSED(audioEnabled);
  Q_UNUSED(videoEnabled);
  Q_UNUSED(audioDeviceId);
  Q_UNUSED(videoDeviceId);
  Q_UNUSED(e2eePassphrase);
  m_lastError =
      "当前为 Debug 构建。Windows 预构建 LiveKit C++ SDK 与 Debug CRT ABI 不兼容，"
      "请切换到 Release 构建运行。";
  return false;
#else
  Q_UNUSED(audioEnabled);
  Q_UNUSED(videoEnabled);
  Q_UNUSED(audioDeviceId);
  Q_UNUSED(videoDeviceId);
  Q_UNUSED(e2eePassphrase);

  m_lastError.clear();

  if (serverUrl.isEmpty() || token.isEmpty()) {
    m_lastError = "serverUrl 或 token 为空，无法连接房间";
    return false;
  }

  if (m_connected || m_room) {
    disconnect();
  }

  if (!m_livekitInitialized) {
    // initialize() 返回 false 也可能表示“已初始化”，不是失败。
    (void)livekit::initialize(livekit::LogLevel::Info, livekit::LogSink::kConsole);
    m_livekitInitialized = true;
  }

  m_room = std::make_unique<livekit::Room>();
  m_room->setDelegate(this);

  livekit::RoomOptions options;
  options.auto_subscribe = true;

  bool connectOk = false;
  try {
    connectOk = m_room->Connect(serverUrl.toStdString(), token.toStdString(), options);
  } catch (const std::exception& ex) {
    m_lastError = QString("LiveKit Connect 异常: %1").arg(ex.what());
    m_room.reset();
    return false;
  } catch (...) {
    m_lastError = "LiveKit Connect 未知异常";
    m_room.reset();
    return false;
  }

  if (!connectOk) {
    m_lastError = "LiveKit Connect 返回 false";
    m_room.reset();
    return false;
  }

  m_connected = true;
  setupRemoteAudioPlayback();
  return true;
#endif
}

void LiveKitRoomService::disconnect() {
  setScreenShareEnabled(false);

  // Stop mic thread and track
  stopMicCapture();
  if (m_micTrack && m_room && m_room->localParticipant()) {
    try { m_room->localParticipant()->unpublishTrack(m_micTrack->sid()); } catch (...) {}
  }
  m_micTrack.reset();
  m_micSource.reset();

  // Stop audio output
  if (m_audioOutput) {
    m_audioOutput->stop();
    m_audioOutputIO = nullptr;
  }
  m_audioOutput.reset();
  m_audioCallbackIds.clear();

  // Stop camera thread and track
  m_cameraActive.store(false);
  if (m_cameraThread.joinable()) m_cameraThread.join();
  if (m_cameraTrack && m_room && m_room->localParticipant()) {
    try { m_room->localParticipant()->unpublishTrack(m_cameraTrack->sid()); } catch (...) {}
  }
  m_cameraTrack.reset();
  m_cameraSource.reset();

  if (m_room) {
    {
      QMutexLocker locker(&m_streamBindingsMutex);
      for (auto it = m_streamBindings.begin(); it != m_streamBindings.end(); ++it) {
        const auto& binding = it.value();
        try {
          m_room->clearOnVideoFrameCallback(binding.identity.toStdString(), binding.source);
        } catch (...) {
        }
      }
      m_streamBindings.clear();
    }
  }

  // 在 Room 对象仍存活时主动关闭 SDK，确保 WebSocket 能发送 Close Frame，
  // 让服务端即时感知断开。仅在正常离会路径执行；析构时跳过以避免崩溃。
  if (m_livekitInitialized && !QCoreApplication::closingDown()) {
    try {
      livekit::shutdown();
    } catch (...) {
    }
    m_livekitInitialized = false;
  }

  if (m_room) {
    m_room.reset();
  }
  m_connected = false;

  {
    QMutexLocker locker(&m_remoteState->mutex);
    m_remoteState->participantsText = "远端参与者：0";
    m_remoteState->videoSourceText = "当前画面来源：无";
    m_remoteState->videoDataUrl.clear();
    m_remoteState->tiles.clear();
  }

  m_remoteIdsHadTracks.clear();
}

bool LiveKitRoomService::setMicEnabled(bool enabled,
                                        const QString& audioDeviceId) {
  m_lastError.clear();
  if (!m_connected || !m_room || !m_room->localParticipant()) {
    m_lastError = "当前未连接房间，无法切换麦克风";
    return false;
  }

  if (!audioDeviceId.isEmpty()) {
    m_audioInputDeviceId = audioDeviceId;
  }

  if (enabled && !m_micTrack) {
    try {
      m_micSource = std::make_shared<livekit::AudioSource>(48000, 1, 20);
      m_micTrack = m_room->localParticipant()->publishAudioTrack(
          "mic-mvp", m_micSource, livekit::TrackSource::SOURCE_MICROPHONE);
      m_micTrack->unmute();
    } catch (const std::exception& ex) {
      m_lastError = QString("创建麦克风轨道失败: %1").arg(ex.what());
      return false;
    } catch (...) {
      m_lastError = "创建麦克风轨道失败: 未知异常";
      return false;
    }
    startMicCapture(m_audioInputDeviceId);
    return true;
  }

  if (m_micTrack) {
    try {
      if (enabled) {
        m_micTrack->unmute();
        startMicCapture(m_audioInputDeviceId);
      } else {
        m_micTrack->mute();
        stopMicCapture();
      }
    } catch (const std::exception& ex) {
      m_lastError = QString("切换麦克风失败: %1").arg(ex.what());
      return false;
    } catch (...) {
      m_lastError = "切换麦克风失败: 未知异常";
      return false;
    }
    return true;
  }

  m_lastError = "未找到可控制的本地音频轨道";
  return false;
}

void LiveKitRoomService::setAudioOutputDevice(const QString& deviceId) {
  if (m_audioOutputDeviceId == deviceId) return;
  m_audioOutputDeviceId = deviceId;
  setupRemoteAudioPlayback();
}

bool LiveKitRoomService::setCameraEnabled(bool enabled) {
  m_lastError.clear();
  if (!m_connected || !m_room || !m_room->localParticipant()) {
    m_lastError = "当前未连接房间，无法切换摄像头";
    return false;
  }

  if (enabled && !m_cameraTrack) {
    // 首次开启：创建 VideoSource + 发布摄像头轨 + 测试图案线程
    try {
      m_cameraSource = std::make_shared<livekit::VideoSource>(640, 480);
      m_cameraTrack = m_room->localParticipant()->publishVideoTrack(
          "camera-mvp", m_cameraSource,
          livekit::TrackSource::SOURCE_CAMERA);
    } catch (const std::exception& ex) {
      m_lastError = QString("创建摄像头轨道失败: %1").arg(ex.what());
      return false;
    } catch (...) {
      m_lastError = "创建摄像头轨道失败: 未知异常";
      return false;
    }
    const QString localIdentity =
        QString::fromStdString(m_room->localParticipant()->identity());
    startCameraThread(streamKey(localIdentity, livekit::TrackSource::SOURCE_CAMERA));
    return true;
  }

  if (m_cameraTrack) {
    try {
      if (enabled) {
        m_cameraTrack->unmute();
        const QString localIdentity =
            QString::fromStdString(m_room->localParticipant()->identity());
        startCameraThread(streamKey(localIdentity, livekit::TrackSource::SOURCE_CAMERA));
      } else {
        m_cameraTrack->mute();
        m_cameraActive.store(false);
        if (m_cameraThread.joinable()) {
          m_cameraThread.join();
        }
      }
    } catch (const std::exception& ex) {
      m_lastError = QString("切换摄像头失败: %1").arg(ex.what());
      return false;
    } catch (...) {
      m_lastError = "切换摄像头失败: 未知异常";
      return false;
    }
    return true;
  }

  m_lastError = "未找到可控制的本地视频轨道";
  return false;
}

bool LiveKitRoomService::setScreenShareEnabled(bool enabled) {
  m_lastError.clear();

  if (!enabled) {
    m_screenShareRunning.store(false);
    if (m_screenShareThread.joinable()) {
      m_screenShareThread.join();
    }

    std::shared_ptr<livekit::LocalVideoTrack> screenTrack;
    {
      std::lock_guard<std::mutex> locker(m_screenShareMutex);
      screenTrack = m_screenShareTrack;
    }

    if (screenTrack && m_room && m_room->localParticipant()) {
      try {
        m_room->localParticipant()->unpublishTrack(screenTrack->sid());
      } catch (...) {
      }
    }

    std::lock_guard<std::mutex> locker(m_screenShareMutex);
    m_screenShareTrack.reset();
    m_screenShareSource.reset();

    // 清理屏幕共享 tile（本地 tile 不受孤儿/超时清理保护）
    if (m_room && m_room->localParticipant()) {
      const QString localIdentity =
          QString::fromStdString(m_room->localParticipant()->identity());
      const QString shareKey =
          streamKey(localIdentity, livekit::TrackSource::SOURCE_SCREENSHARE);
      QMutexLocker stateLocker(&m_remoteState->mutex);
      m_remoteState->tiles.remove(shareKey);
    }
    return true;
  }

  if (screenShareEnabled()) {
    return true;
  }

  if (!m_connected || !m_room || !m_room->localParticipant()) {
    m_lastError = "当前未连接房间，无法开启共享屏幕";
    return false;
  }

  const QString localIdentity =
      QString::fromStdString(m_room->localParticipant()->identity());
  const QString localShareTileKey =
      streamKey(localIdentity, livekit::TrackSource::SOURCE_SCREENSHARE);

  int shareWidth = kScreenShareDefaultWidth;
  int shareHeight = kScreenShareDefaultHeight;
#ifdef Q_OS_WIN
  {
    DesktopCaptureContext probe;
    if (initDesktopCapture(probe)) {
      shareWidth = probe.width;
      shareHeight = probe.height;
      releaseDesktopCapture(probe);
    }
  }
#endif

  std::shared_ptr<livekit::VideoSource> source;
  std::shared_ptr<livekit::LocalVideoTrack> track;
  try {
    source = std::make_shared<livekit::VideoSource>(shareWidth, shareHeight);
    track = m_room->localParticipant()->publishVideoTrack(
        "screenshare-mvp", source, livekit::TrackSource::SOURCE_SCREENSHARE);
  } catch (const std::exception& ex) {
    m_lastError = QString("开启共享屏幕失败: %1").arg(ex.what());
    return false;
  } catch (...) {
    m_lastError = "开启共享屏幕失败: 未知异常";
    return false;
  }

  {
    std::lock_guard<std::mutex> locker(m_screenShareMutex);
    m_screenShareSource = source;
    m_screenShareTrack = track;
  }

  m_screenShareRunning.store(true);
  m_screenShareThread = std::thread(
      [this, source, shareWidth, shareHeight, localIdentity, localShareTileKey]() {
    int tick = 0;
    const auto frameInterval = std::chrono::milliseconds(1000 / kScreenShareFps);
    qint64 lastLocalPreviewMs = 0;
#ifdef Q_OS_WIN
    DesktopCaptureContext capture;
    const bool desktopCaptureReady = initDesktopCapture(capture);
#endif
    while (m_screenShareRunning.load()) {
      auto frame =
          livekit::VideoFrame::create(shareWidth, shareHeight, livekit::VideoBufferType::RGBA);
#ifdef Q_OS_WIN
      const bool frameReady = desktopCaptureReady ? captureDesktopFrame(capture, frame) : false;
#else
      bool frameReady = false;
      std::uint8_t* buffer = frame.data();
      if (buffer) {
        frameReady = true;
        for (int y = 0; y < shareHeight; ++y) {
          for (int x = 0; x < shareWidth; ++x) {
            const int offset = (y * shareWidth + x) * 4;
            buffer[offset + 0] = static_cast<std::uint8_t>((x + tick * 3) % 255);
            buffer[offset + 1] = static_cast<std::uint8_t>((y + tick * 2) % 255);
            buffer[offset + 2] = static_cast<std::uint8_t>((x + y + tick) % 255);
            buffer[offset + 3] = 255;
          }
        }
      }
#endif

      if (frameReady) {
        // 必须在 captureFrame 之前拷贝帧用于本地预览；
        // SDK 调用 captureFrame 后取得数据所有权，之后 frame.data() 不可再用。
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const bool doPreview = nowMs - lastLocalPreviewMs >= kPreviewUpdateIntervalMs;
        QImage previewCopy;
        if (doPreview) {
          const QImage image(frame.data(), frame.width(), frame.height(),
                             QImage::Format_RGBA8888);
          if (!image.isNull()) {
            previewCopy =
                image.width() > 1280
                    ? image.copy().scaledToWidth(1280, Qt::SmoothTransformation)
                    : image.copy();
          }
        }

        try {
          const auto now = std::chrono::steady_clock::now().time_since_epoch();
          const auto timestampUs =
              std::chrono::duration_cast<std::chrono::microseconds>(now).count();
          source->captureFrame(frame, static_cast<std::int64_t>(timestampUs),
                               livekit::VideoRotation::VIDEO_ROTATION_0);
        } catch (...) {
        }

        if (!previewCopy.isNull()) {
          QByteArray encodedBytes;
          QBuffer buffer(&encodedBytes);
          if (buffer.open(QIODevice::WriteOnly)) {
            if (previewCopy.save(&buffer, "JPEG", 75)) {
              const QString dataUrl =
                  "data:image/jpeg;base64," + QString::fromLatin1(encodedBytes.toBase64());
              QMutexLocker locker(&m_remoteState->mutex);
              auto& tile = m_remoteState->tiles[localShareTileKey];
              tile.tileId = localShareTileKey;
              tile.identity = localIdentity;
              tile.displayName = "我自己";
              tile.sourceText = "屏幕共享";
              tile.isLocal = true;
              tile.dataUrl = dataUrl;
              tile.lastFrameMs = nowMs;
            }
          }
          lastLocalPreviewMs = nowMs;
        }
      }

      ++tick;
      std::this_thread::sleep_for(frameInterval);
    }
#ifdef Q_OS_WIN
    releaseDesktopCapture(capture);
#endif
  });

  return true;
}

bool LiveKitRoomService::screenShareEnabled() const {
  std::lock_guard<std::mutex> locker(m_screenShareMutex);
  return m_screenShareRunning.load() && (m_screenShareTrack != nullptr);
}

void LiveKitRoomService::startMicCapture(const QString& deviceId) {
  if (m_micActive.load()) return;
  m_micActive.store(true);

  if (!deviceId.isEmpty()) {
    const auto devices = QMediaDevices::audioInputs();
    QAudioDevice device;
    for (const auto& d : devices) {
      if (QString::fromLatin1(d.id()) == deviceId) {
        device = d;
        break;
      }
    }

    if (!device.isNull()) {
      // Try Int16, then Float, then preferred
      QAudioFormat fmt;
      QAudioFormat int16fmt;
      int16fmt.setSampleRate(48000);
      int16fmt.setChannelCount(1);
      int16fmt.setSampleFormat(QAudioFormat::Int16);
      QAudioFormat floatFmt;
      floatFmt.setSampleRate(48000);
      floatFmt.setChannelCount(1);
      floatFmt.setSampleFormat(QAudioFormat::Float);

      if (device.isFormatSupported(int16fmt)) {
        fmt = int16fmt;
        qInfo() << "Mic: using Int16 format";
      } else if (device.isFormatSupported(floatFmt)) {
        fmt = floatFmt;
        qInfo() << "Mic: using Float format";
      } else {
        fmt = device.preferredFormat();
        qInfo() << "Mic: using preferred format";
      }
      qInfo() << "Mic capture using" << device.description()
              << "@" << fmt.sampleRate() << "Hz ch=" << fmt.channelCount();

      // Capture in dedicated thread using blocking waitForReadyRead()
      auto src = m_micSource;
      m_micThread = std::thread([this, device, fmt, src]() {
        QAudioSource audioSource(device, fmt);
        QIODevice* io = audioSource.start();
        if (!io) {
          qWarning() << "Mic capture: start() returned null in thread";
          return;
        }

        constexpr int kFrameMs = 20;
        constexpr int kSampleRate = 48000;
        constexpr int kSamplesPerFrame = kSampleRate * kFrameMs / 1000; // 960
        constexpr int kBytesPerFrame = kSamplesPerFrame * sizeof(int16_t); // 1920
        QByteArray buffer;

        while (m_micActive.load()) {
          buffer.append(io->readAll());
          static int frameCount = 0;
          while (buffer.size() >= kBytesPerFrame) {
            auto frame = livekit::AudioFrame::create(kSampleRate, 1, kSamplesPerFrame);
            auto& buf = frame.data();
            std::memcpy(buf.data(), buffer.constData(), kBytesPerFrame);
            buffer.remove(0, kBytesPerFrame);
            if (++frameCount <= 3) {
              int maxVal = 0;
              for (auto s : buf) { int v = s < 0 ? -s : s; if (v > maxVal) maxVal = v; }
              qInfo() << "Mic frame #" << frameCount << "maxAmplitude:" << maxVal;
            }
            try {
              src->captureFrame(frame, kFrameMs);
            } catch (...) {
            }
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        audioSource.stop();
      });
      return;
    }
  }

  // Fallback: silent placeholder
  m_micThread = std::thread([this]() {
    while (m_micActive.load()) {
      auto frame = livekit::AudioFrame::create(48000, 1, 960);
      try {
        m_micSource->captureFrame(frame, 20);
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  });
}

void LiveKitRoomService::stopMicCapture() {
  m_micActive.store(false);
  if (m_micThread.joinable()) {
    m_micThread.join();
  }
}


void LiveKitRoomService::setupRemoteAudioPlayback() {
  if (m_audioOutputIO) {
    m_audioOutput->stop();
    m_audioOutputIO = nullptr;
  }
  m_audioOutput.reset();

  if (m_audioOutputDeviceId.isEmpty()) {
    // Use default output device
    const auto devices = QMediaDevices::audioOutputs();
    if (devices.isEmpty()) return;
    m_audioOutputDeviceId = QString::fromLatin1(devices.first().id());
  }

  const auto devices = QMediaDevices::audioOutputs();
  QAudioDevice device;
  for (const auto& d : devices) {
    if (QString::fromLatin1(d.id()) == m_audioOutputDeviceId) {
      device = d;
      break;
    }
  }
  if (device.isNull()) return;

  QAudioFormat fmt;
  fmt.setSampleRate(48000);
  fmt.setChannelCount(1);
  fmt.setSampleFormat(QAudioFormat::Int16);

  m_audioOutput = std::make_unique<QAudioSink>(device, fmt);
  m_audioOutputIO = m_audioOutput->start();
  if (m_audioOutputIO) {
    qInfo() << "LiveKitRoomService: audio output started on" << device.description();
  }
}

void LiveKitRoomService::startCameraThread(const QString& cameraTileKey) {
  if (m_cameraActive.load()) return;
  m_cameraActive.store(true);
  std::weak_ptr<RemoteState> weakState = m_remoteState;
  const QString tileKey = cameraTileKey;
  m_cameraThread = std::thread([this, weakState, tileKey]() {
    int tick = 0;
    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    const auto frameInterval = std::chrono::milliseconds(100);

    while (m_cameraActive.load()) {
      auto frame = livekit::VideoFrame::create(kWidth, kHeight,
                                               livekit::VideoBufferType::RGBA);
      std::uint8_t* buf = frame.data();
      if (buf) {
        for (int y = 0; y < kHeight; ++y) {
          for (int x = 0; x < kWidth; ++x) {
            const int offset = (y * kWidth + x) * 4;
            buf[offset + 0] = static_cast<std::uint8_t>((x + tick * 2) % 256);
            buf[offset + 1] = static_cast<std::uint8_t>((y + tick) % 256);
            buf[offset + 2] = 200;
            buf[offset + 3] = 255;
          }
        }
      }
      try {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto tsUs = std::chrono::duration_cast<std::chrono::microseconds>(
                              now).count();
        m_cameraSource->captureFrame(frame, static_cast<std::int64_t>(tsUs),
                                     livekit::VideoRotation::VIDEO_ROTATION_0);
      } catch (...) {
      }

      // 写 JPEG 预览到 tile，类似屏幕共享的做法
      if (buf) {
        const QImage previewFrame(buf, kWidth, kHeight, QImage::Format_RGBA8888);
        if (!previewFrame.isNull()) {
          QByteArray encodedBytes;
          QBuffer buffer(&encodedBytes);
          if (buffer.open(QIODevice::WriteOnly)) {
            QImage copy = previewFrame.copy();
            if (copy.save(&buffer, "JPEG", 75)) {
              const QString dataUrl =
                  "data:image/jpeg;base64," + QString::fromLatin1(encodedBytes.toBase64());
              const auto state = weakState.lock();
              if (state) {
                QMutexLocker locker(&state->mutex);
                if (state->tiles.contains(tileKey)) {
                  state->tiles[tileKey].dataUrl = dataUrl;
                }
              }
            }
          }
        }
      }

      ++tick;
      std::this_thread::sleep_for(frameInterval);
    }
  });
}

void LiveKitRoomService::refreshRemoteState() {
  if (!m_connected || !m_room) {
    m_streamBindings.clear();
    QMutexLocker locker(&m_remoteState->mutex);
    m_remoteState->participantsText = "远端参与者：0";
    m_remoteState->videoSourceText = "当前画面来源：无";
    m_remoteState->videoDataUrl.clear();
    m_remoteState->tiles.clear();
    return;
  }

  const auto remoteParticipants = m_room->remoteParticipants();
  QStringList participantNames;
  for (const auto& participant : remoteParticipants) {
    if (!participant) {
      continue;
    }

    const QString identity = QString::fromStdString(participant->identity());
    const QString name = QString::fromStdString(participant->name());
    participantNames << (name.isEmpty() ? identity : QString("%1(%2)").arg(name, identity));
  }

  QString participantsText = "远端参与者：0";
  if (!participantNames.isEmpty()) {
    const int showCount = qMin(4, participantNames.size());
    QStringList previewNames;
    for (int i = 0; i < showCount; ++i) {
      previewNames << participantNames.at(i);
    }

    const int remain = participantNames.size() - showCount;
    const QString suffix = remain > 0 ? QString(" ... +%1").arg(remain) : "";
    participantsText = QString("远端参与者：%1 [%2]%3")
                           .arg(participantNames.size())
                           .arg(previewNames.join(", "))
                           .arg(suffix);
  }

  {
    QMutexLocker locker(&m_remoteState->mutex);
    m_remoteState->participantsText = participantsText;
  }

  QHash<QString, StreamBinding> desiredBindings;
  auto appendBindings = [&](const QString& identity, const QString& displayName,
                            bool isLocal, const auto& publicationMap) {
    for (const auto& [sid, publication] : publicationMap) {
      Q_UNUSED(sid);
      if (!publication) {
        continue;
      }
      if (publication->kind() != livekit::TrackKind::KIND_VIDEO || publication->muted()) {
        continue;
      }
      const auto source = publication->source();
      if (source != livekit::TrackSource::SOURCE_CAMERA &&
          source != livekit::TrackSource::SOURCE_SCREENSHARE) {
        continue;
      }

      StreamBinding binding;
      binding.identity = identity;
      binding.displayName = displayName;
      binding.source = source;
      binding.isLocal = isLocal;
      binding.key = streamKey(identity, source);
      desiredBindings.insert(binding.key, binding);
    }
  };

  for (const auto& participant : remoteParticipants) {
    if (!participant) {
      continue;
    }
    const QString identity = QString::fromStdString(participant->identity());
    const QString name = QString::fromStdString(participant->name());
    appendBindings(identity, name.isEmpty() ? identity : name, false,
                   participant->trackPublications());
  }

  if (m_room->localParticipant()) {
    const QString localIdentity = QString::fromStdString(m_room->localParticipant()->identity());
    appendBindings(localIdentity, "我自己", true, m_room->localParticipant()->trackPublications());
  }

  // 当前有效视频轨 key 集合，用于清理不属于当前发布列表的残留 tile
  QSet<QString> desiredKeys;
  for (auto it = desiredBindings.cbegin(); it != desiredBindings.cend(); ++it) {
    desiredKeys.insert(it.key());
  }

  // 为没有视频轨的参与者创建占位 tile（远端 + 本地），确保始终显示在场成员
  {
    QMutexLocker locker(&m_remoteState->mutex);

    // 视频轨参与者（仅视频，用于决定是否移除占位 tile）
    QSet<QString> identitiesWithVideoTrack;
    for (auto it = desiredBindings.cbegin(); it != desiredBindings.cend(); ++it) {
      identitiesWithVideoTrack.insert(it.value().identity);
    }

    // 麦克风活跃参与者（未静音音频轨）
    auto hasActiveAudio = [](const auto& publicationMap) -> bool {
      for (const auto& [sid, pub] : publicationMap) {
        Q_UNUSED(sid);
        if (pub && pub->kind() == livekit::TrackKind::KIND_AUDIO && !pub->muted()) {
          return true;
        }
      }
      return false;
    };
    QSet<QString> identitiesWithActiveMic;
    for (const auto& participant : remoteParticipants) {
      if (!participant) continue;
      const QString identity = QString::fromStdString(participant->identity());
      if (hasActiveAudio(participant->trackPublications())) {
        identitiesWithActiveMic.insert(identity);
      }
    }
    if (m_room->localParticipant()) {
      const QString localId = QString::fromStdString(m_room->localParticipant()->identity());
      if (hasActiveAudio(m_room->localParticipant()->trackPublications())) {
        identitiesWithActiveMic.insert(localId);
      }
    }

    // 更新"曾有过轨道"的记录（视频或音频均可）；用于判断参与者是否可能已断开
    QSet<QString> identitiesWithAnyTrack = identitiesWithVideoTrack;
    identitiesWithAnyTrack.unite(identitiesWithActiveMic);
    for (const auto& identity : identitiesWithAnyTrack) {
      m_remoteIdsHadTracks.insert(identity);
    }

    QSet<QString> currentRemoteIdentities;
    for (const auto& participant : remoteParticipants) {
      if (participant) {
        currentRemoteIdentities.insert(
            QString::fromStdString(participant->identity()));
      }
    }

    // 清理已离开参与者的记录
    for (auto it = m_remoteIdsHadTracks.begin();
         it != m_remoteIdsHadTracks.end();) {
      if (!currentRemoteIdentities.contains(*it)) {
        it = m_remoteIdsHadTracks.erase(it);
      } else {
        ++it;
      }
    }

    // 清理已离开参与者的所有 tile（视频轨 + 占位），确保离会后不留残留画面
    for (auto it = m_remoteState->tiles.begin();
         it != m_remoteState->tiles.end();) {
      if (!it.value().isLocal &&
          !currentRemoteIdentities.contains(it.value().identity)) {
        it = m_remoteState->tiles.erase(it);
      } else {
        ++it;
      }
    }

    // 从 desiredKeys 提取有视频轨的参与者的 identity 集合。
    // 直接用 tile key 的前缀匹配，避免 QString::fromStdString 往返可能引入的差异。
    QSet<QString> identitiesWithVideo;
    for (const QString& key : desiredKeys) {
      int pipe = key.lastIndexOf('|');
      if (pipe > 0) {
        identitiesWithVideo.insert(key.left(pipe));
      }
    }

    // 单次遍历：清理所有不应存在的 tile
    for (auto it = m_remoteState->tiles.begin();
         it != m_remoteState->tiles.end();) {
      const QString& key = it.key();
      const VideoTile& tile = it.value();

      // 当前有效视频轨 → 保留
      if (desiredKeys.contains(key)) {
        ++it;
        continue;
      }

      // 占位 tile：若该参与者有视频轨则移除，否则保留
      if (tile.sourceText == "无视频流") {
        if (identitiesWithVideo.contains(tile.identity)) {
          it = m_remoteState->tiles.erase(it);
        } else {
          ++it;
        }
        continue;
      }

      // 本地 tile 且源仍为本地 → 保留（本地屏幕共享等已在 setScreenShareEnabled 清理）
      if (tile.isLocal) {
        ++it;
        continue;
      }

      // 已离开参与者 → 已在上面清理，此处防御
      if (!currentRemoteIdentities.contains(tile.identity)) {
        ++it;
        continue;
      }

      // 其余残留 video tile → 移除
      it = m_remoteState->tiles.erase(it);
    }

    // 为无视频轨的远端参与者创建占位 tile
    for (const auto& participant : remoteParticipants) {
      if (!participant) {
        continue;
      }
      const QString identity = QString::fromStdString(participant->identity());
      if (identitiesWithVideo.contains(identity)) continue;
      const QString presenceKey = identity + "|presence";
      if (m_remoteState->tiles.contains(presenceKey)) continue;
      const QString name = QString::fromStdString(participant->name());
      const QString disp = name.isEmpty() ? identity : name;
      auto& tile = m_remoteState->tiles[presenceKey];
      tile.tileId = presenceKey;
      tile.identity = identity;
      tile.displayName = disp;
      tile.sourceText = "无视频流";
      tile.isLocal = false;
    }

    // 为本地参与者添加占位 tile（如果无视频轨）
    if (m_room->localParticipant()) {
      const QString localIdentity =
          QString::fromStdString(m_room->localParticipant()->identity());
      const QString presenceKey = localIdentity + "|presence";
      if (identitiesWithVideo.contains(localIdentity)) {
        m_remoteState->tiles.remove(presenceKey);
      } else if (!m_remoteState->tiles.contains(presenceKey)) {
        auto& tile = m_remoteState->tiles[presenceKey];
        tile.tileId = presenceKey;
        tile.identity = localIdentity;
        tile.displayName = "我自己";
        tile.sourceText = "无视频流";
        tile.isLocal = true;
      }
    }

    // 为所有 tile 设置麦克风活跃状态
    for (auto it = m_remoteState->tiles.begin(); it != m_remoteState->tiles.end(); ++it) {
      it.value().micActive = identitiesWithActiveMic.contains(it.value().identity);
    }
  }

  {
    QMutexLocker locker(&m_streamBindingsMutex);

    QSet<QString> existingKeys;
    for (auto it = m_streamBindings.cbegin(); it != m_streamBindings.cend(); ++it) {
      existingKeys.insert(it.key());
    }

    for (const QString& staleKey : existingKeys - desiredKeys) {
      const auto stale = m_streamBindings.value(staleKey);
      try {
        m_room->clearOnVideoFrameCallback(stale.identity.toStdString(), stale.source);
      } catch (...) {
      }
      m_streamBindings.remove(staleKey);
      {
        QMutexLocker tileLocker(&m_remoteState->mutex);
        m_remoteState->tiles.remove(staleKey);
      }
    }
  }

  // 清理"孤儿 tile"：由 onTrackSubscribed 自建但帧回调已被 delegate 清掉的
  // 残留 tile。这些 tile 不在 m_streamBindings 也不在 desiredKeys 中。
  {
    QSet<QString> orphanKeys;
    {
      QMutexLocker bindLocker(&m_streamBindingsMutex);
      QMutexLocker stateLocker(&m_remoteState->mutex);
      for (auto it = m_remoteState->tiles.cbegin();
           it != m_remoteState->tiles.cend(); ++it) {
        if (it.value().sourceText == "无视频流") continue;
        if (it.value().isLocal) continue;
        if (!m_streamBindings.contains(it.key()) &&
            !desiredKeys.contains(it.key())) {
          orphanKeys.insert(it.key());
        }
      }
    }
    if (!orphanKeys.isEmpty()) {
      QMutexLocker stateLocker(&m_remoteState->mutex);
      for (const QString& key : orphanKeys) {
        m_remoteState->tiles.remove(key);
      }
    }
  }

  // 直接清理：任何非占位、非本地 tile，如果不在 desiredKeys 中，
  // 说明对应轨道已取消发布，立即移除。补偿 SDK onTrackUnpublished 可能不触发的问题。
  {
    QSet<QString> keysToRemove;
    {
      QMutexLocker stateLocker(&m_remoteState->mutex);
      for (auto it = m_remoteState->tiles.cbegin();
           it != m_remoteState->tiles.cend(); ++it) {
        if (it.value().sourceText == "无视频流") continue;
        if (it.value().isLocal) continue;
        if (!desiredKeys.contains(it.key())) {
          keysToRemove.insert(it.key());
        }
      }
    }
    if (!keysToRemove.isEmpty()) {
      // 同时清理可能残留的帧回调
      for (const QString& key : keysToRemove) {
        QMutexLocker bindLocker(&m_streamBindingsMutex);
        if (m_streamBindings.contains(key)) {
          const auto binding = m_streamBindings.value(key);
          try {
            m_room->clearOnVideoFrameCallback(binding.identity.toStdString(),
                                              binding.source);
          } catch (...) {}
          m_streamBindings.remove(key);
        }
      }
      QMutexLocker stateLocker(&m_remoteState->mutex);
      for (const QString& key : keysToRemove) {
        m_remoteState->tiles.remove(key);
      }
    }
  }

  // 帧时效检测：对曾活跃参与者，3 秒无帧 → 清除回调+绑定+tile；
  // 对未曾活跃的，仅清 dataUrl。补偿 SDK 断开通知延迟。
  {
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    struct Removal {
      QString key;
      QString identity;
    };
    QList<Removal> removals;

    {
      QMutexLocker stateLocker(&m_remoteState->mutex);
      for (auto it = m_remoteState->tiles.begin();
           it != m_remoteState->tiles.end(); ++it) {
        if (it.value().isLocal) continue;
        if (it.value().lastFrameMs == 0) continue;
        if (nowMs - it.value().lastFrameMs <= 3000) continue;

        if (m_remoteIdsHadTracks.contains(it.value().identity)) {
          removals.push_back({it.key(), it.value().identity});
        } else {
          it.value().dataUrl.clear();
        }
      }
    }

    // 以下操作不再持有 m_remoteState->mutex，遵循锁顺序
    for (const auto& r : removals) {
      try {
        m_room->clearOnVideoFrameCallback(
            r.identity.toStdString(), livekit::TrackSource::SOURCE_CAMERA);
      } catch (...) {}
      try {
        m_room->clearOnVideoFrameCallback(
            r.identity.toStdString(),
            livekit::TrackSource::SOURCE_SCREENSHARE);
      } catch (...) {}
    }
    if (!removals.isEmpty()) {
      {
        QMutexLocker bindLocker(&m_streamBindingsMutex);
        for (const auto& r : removals) {
          m_streamBindings.remove(r.key);
        }
      }
      QMutexLocker stateLocker(&m_remoteState->mutex);
      for (const auto& r : removals) {
        m_remoteState->tiles.remove(r.key);
      }
    }
  }

  livekit::VideoStream::Options options;
  options.capacity = 4;
  options.format = livekit::VideoBufferType::RGBA;

  std::weak_ptr<RemoteState> weakState = m_remoteState;
  for (auto it = desiredBindings.begin(); it != desiredBindings.end(); ++it) {
    const auto binding = it.value();

    {
      QMutexLocker locker(&m_streamBindingsMutex);
      if (m_streamBindings.contains(binding.key)) {
        continue;
      }
    }

    {
      QMutexLocker locker(&m_remoteState->mutex);
      auto& tile = m_remoteState->tiles[binding.key];
      tile.tileId = binding.key;
      tile.identity = binding.identity;
      tile.displayName = binding.displayName;
      tile.sourceText = trackSourceLabel(binding.source);
      tile.isLocal = binding.isLocal;
    }

    m_room->setOnVideoFrameCallback(
        binding.identity.toStdString(), binding.source,
        [weakState, key = binding.key](const livekit::VideoFrame& frame, std::int64_t) {
          const auto state = weakState.lock();
          if (!state || frame.width() <= 0 || frame.height() <= 0 || frame.dataSize() == 0) {
            return;
          }

          const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
          {
            QMutexLocker locker(&state->mutex);
            if (!state->tiles.contains(key)) {
              return;
            }
            auto& tile = state->tiles[key];
            if (nowMs - tile.lastFrameMs < kPreviewUpdateIntervalMs) {
              return;
            }
            tile.lastFrameMs = nowMs;
          }

          const QImage image(frame.data(), frame.width(), frame.height(),
                             QImage::Format_RGBA8888);
          if (image.isNull()) {
            return;
          }

          QByteArray pngBytes;
          QBuffer buffer(&pngBytes);
          if (!buffer.open(QIODevice::WriteOnly)) {
            return;
          }
          const QImage scaled = image.width() > 1280
                                    ? image.copy().scaledToWidth(1280, Qt::SmoothTransformation)
                                    : image.copy();
          if (!scaled.save(&buffer, "JPEG", 75)) {
            return;
          }

          const QString dataUrl =
              "data:image/jpeg;base64," + QString::fromLatin1(pngBytes.toBase64());
          QMutexLocker locker(&state->mutex);
          if (!state->tiles.contains(key)) {
            return;
          }
          state->tiles[key].dataUrl = dataUrl;
        },
        options);

    {
      QMutexLocker locker(&m_streamBindingsMutex);
      m_streamBindings.insert(binding.key, binding);
    }
  }

  {
    QMutexLocker locker(&m_remoteState->mutex);
    if (m_remoteState->tiles.isEmpty()) {
      m_remoteState->videoSourceText = "当前画面来源：无（等待摄像头/共享）";
      m_remoteState->videoDataUrl.clear();
    } else {
      m_remoteState->videoSourceText = QString("当前画面来源：多宫格（%1 路）")
                                           .arg(m_remoteState->tiles.size());
      m_remoteState->videoDataUrl.clear();
      for (auto it = m_remoteState->tiles.cbegin(); it != m_remoteState->tiles.cend(); ++it) {
        if (!it.value().dataUrl.isEmpty()) {
          m_remoteState->videoDataUrl = it.value().dataUrl;
          break;
        }
      }
    }
  }

  // Register audio frame callbacks for remote participants to route PCM to QAudioSink
  if (m_audioOutputIO) {
    for (const auto& participant : remoteParticipants) {
      if (!participant) continue;
      const QString identity = QString::fromStdString(participant->identity());
      if (m_audioCallbackIds.contains(identity)) continue;
      m_audioCallbackIds.insert(identity);

      livekit::AudioStream::Options audioOpts;
      m_room->setOnAudioFrameCallback(
          identity.toStdString(),
          livekit::TrackSource::SOURCE_MICROPHONE,
          [this, identity](const livekit::AudioFrame& frame) {
            if (!m_audioOutputIO) return;
            const auto& data = frame.data();
            if (data.empty()) return;
            static int frameCount = 0;
            if (++frameCount <= 3) {
              qInfo() << "AudioFrame received from" << identity
                      << "samples:" << data.size()
                      << "sample_rate:" << frame.sample_rate()
                      << "channels:" << frame.num_channels();
            }
            qint64 written = m_audioOutputIO->write(
                reinterpret_cast<const char*>(data.data()),
                static_cast<qint64>(data.size() * sizeof(int16_t)));
            if (frameCount == 50) {
              // Check if audio data is actually non-silent
              int16_t maxVal = 0;
              for (auto s : data) { if ((s < 0 ? -s : s) > maxVal) maxVal = (s < 0 ? -s : s); }
              qInfo() << "Audio output state:" << int(m_audioOutput->state())
                      << "error:" << int(m_audioOutput->error())
                      << "bytesFree:" << m_audioOutput->bytesFree()
                      << "bufferSize:" << m_audioOutput->bufferSize()
                      << "maxAmplitude:" << maxVal;
            }
          },
          audioOpts);
    }
  }
}

QString LiveKitRoomService::remoteParticipantsText() const {
  QMutexLocker locker(&m_remoteState->mutex);
  return m_remoteState->participantsText;
}

QString LiveKitRoomService::remoteVideoDataUrl() const {
  QMutexLocker locker(&m_remoteState->mutex);
  return m_remoteState->videoDataUrl;
}

QVariantList LiveKitRoomService::remoteVideoTiles() const {
  QMutexLocker locker(&m_remoteState->mutex);
  QVariantList list;
  QList<VideoTile> tiles = m_remoteState->tiles.values();
  std::sort(tiles.begin(), tiles.end(), [](const VideoTile& a, const VideoTile& b) {
    if (a.isLocal != b.isLocal) {
      return !a.isLocal;
    }
    if (a.sourceText != b.sourceText) {
      return a.sourceText == "屏幕共享";
    }
    return a.tileId < b.tileId;
  });

  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  list.reserve(tiles.size());
  for (const auto& tile : tiles) {
    // 屏幕共享 tile：超过 1 秒无帧或从未收到帧 → 降级为占位 tile
    bool shareIsStale = false;
    if (tile.sourceText == "屏幕共享") {
      if (tile.dataUrl.isEmpty()) {
        shareIsStale = true;
      } else if (tile.lastFrameMs > 0 && nowMs - tile.lastFrameMs > 1000) {
        shareIsStale = true;
      }
    }
    const QString effectiveSource = shareIsStale ? QString("无视频流") : tile.sourceText;

    QVariantMap map;
    map.insert("tileId", tile.tileId);
    map.insert("identity", tile.identity);
    map.insert("displayName", tile.displayName);
    map.insert("sourceText", effectiveSource);
    map.insert("isLocal", tile.isLocal);
    map.insert("micActive", tile.micActive);
    map.insert("dataUrl", tile.dataUrl);
    list.push_back(map);
  }
  return list;
}

QVariantList LiveKitRoomService::participantTracks() const {
  QVariantList list;
  if (!m_room) return list;

  // 本地参与者
  if (auto* lp = m_room->localParticipant()) {
    const QString identity = QString::fromStdString(lp->identity());
    const QString name = QString::fromStdString(lp->name());
    for (const auto& [sid, pub] : lp->trackPublications()) {
      Q_UNUSED(sid);
      if (!pub) continue;
      QVariantMap map;
      map.insert("participantIdentity", identity);
      map.insert("participantName", name.isEmpty() ? identity : name);
      map.insert("trackSid", QString::fromStdString(pub->sid()));
      map.insert("kind",
                 pub->kind() == livekit::TrackKind::KIND_AUDIO ? "audio" : "video");
      map.insert("source", static_cast<int>(pub->source()));
      map.insert("muted", pub->muted());
      list.push_back(map);
    }
  }

  // 远端参与者
  for (const auto& p : m_room->remoteParticipants()) {
    if (!p) continue;
    const QString identity = QString::fromStdString(p->identity());
    const QString name = QString::fromStdString(p->name());
    for (const auto& [sid, pub] : p->trackPublications()) {
      Q_UNUSED(sid);
      if (!pub) continue;
      QVariantMap map;
      map.insert("participantIdentity", identity);
      map.insert("participantName", name.isEmpty() ? identity : name);
      map.insert("trackSid", QString::fromStdString(pub->sid()));
      map.insert("kind",
                 pub->kind() == livekit::TrackKind::KIND_AUDIO ? "audio" : "video");
      map.insert("source", static_cast<int>(pub->source()));
      map.insert("muted", pub->muted());
      list.push_back(map);
    }
  }

  return list;
}

QString LiveKitRoomService::remoteVideoSourceText() const {
  QMutexLocker locker(&m_remoteState->mutex);
  return m_remoteState->videoSourceText;
}

void LiveKitRoomService::publishData(const QByteArray& data,
                                     const QString& topic) {
  if (!m_room || !m_room->localParticipant()) return;
  const auto vec = std::vector<std::uint8_t>(data.begin(), data.end());
  m_room->localParticipant()->publishData(
      vec, false, {}, topic.toStdString());
}

// ---------------------------------------------------------------------------
// RoomDelegate — host command reception via data channel
// ---------------------------------------------------------------------------

void LiveKitRoomService::onUserPacketReceived(
    livekit::Room&, const livekit::UserDataPacketEvent& event) {
  if (event.data.empty()) return;
  const QString topic = QString::fromStdString(event.topic);
  const QByteArray payload(
      reinterpret_cast<const char*>(event.data.data()),
      static_cast<int>(event.data.size()));
  QMetaObject::invokeMethod(
      this, "doHandleUserPacketReceived", Qt::QueuedConnection,
      Q_ARG(QString, topic), Q_ARG(QByteArray, payload));
}

void LiveKitRoomService::doHandleUserPacketReceived(const QString& topic,
                                                    const QByteArray& payload) {
  Q_UNUSED(topic);
  QJsonParseError err;
  const auto doc = QJsonDocument::fromJson(payload, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
  const QString cmd = doc.object().value("cmd").toString();
  if (cmd.isEmpty()) return;
  emit hostCommandReceived(cmd);
}

QString LiveKitRoomService::lastError() const { return m_lastError; }

// ---------------------------------------------------------------------------
// livekit::RoomDelegate — event-driven, fire from SDK thread
// ---------------------------------------------------------------------------

void LiveKitRoomService::onParticipantConnected(
    livekit::Room&, const livekit::ParticipantConnectedEvent& event) {
  if (!event.participant) {
    return;
  }
  QMetaObject::invokeMethod(this, "doHandleParticipantConnected",
                            Qt::QueuedConnection);
}

void LiveKitRoomService::onParticipantDisconnected(
    livekit::Room&, const livekit::ParticipantDisconnectedEvent& event) {
  if (!event.participant) {
    return;
  }
  const QString identity =
      QString::fromStdString(event.participant->identity());
  QMetaObject::invokeMethod(this, "doHandleParticipantDisconnected",
                            Qt::QueuedConnection, Q_ARG(QString, identity));
}

void LiveKitRoomService::onTrackUnpublished(
    livekit::Room&, const livekit::TrackUnpublishedEvent& event) {
  if (!event.participant || !event.publication) {
    return;
  }
  const QString identity =
      QString::fromStdString(event.participant->identity());
  const auto source = event.publication->source();
  QMetaObject::invokeMethod(this, "doHandleTrackUnpublished",
                            Qt::QueuedConnection, Q_ARG(QString, identity),
                            Q_ARG(livekit::TrackSource, source));
}

void LiveKitRoomService::onTrackSubscribed(
    livekit::Room&, const livekit::TrackSubscribedEvent& event) {
  if (!event.participant || !event.publication || !event.track) {
    return;
  }
  if (event.track->kind() != livekit::TrackKind::KIND_VIDEO) {
    return;
  }
  const auto source = event.publication->source();
  if (source != livekit::TrackSource::SOURCE_CAMERA &&
      source != livekit::TrackSource::SOURCE_SCREENSHARE) {
    return;
  }
  if (event.publication->muted()) {
    return;
  }

  const QString identity =
      QString::fromStdString(event.participant->identity());
  QString name = QString::fromStdString(event.participant->name());
  if (name.isEmpty()) name = identity;
  const QString key = streamKey(identity, source);
  const QString sourceLabel = trackSourceLabel(source);

  // 原子地检查是否已有绑定，避免与 refreshRemoteState() 竞争导致重复注册。
  {
    QMutexLocker locker(&m_streamBindingsMutex);
    if (m_streamBindings.contains(key)) {
      return;  // 已由 refreshRemoteState 注册过
    }

    StreamBinding binding;
    binding.key = key;
    binding.identity = identity;
    binding.displayName = name;
    binding.source = source;
    binding.isLocal = false;
    m_streamBindings.insert(key, binding);
  }

  // 在 SDK 线程直接注册帧回调，避免 QueuedConnection 排队期间帧丢失。
  livekit::VideoStream::Options opts;
  opts.capacity = 4;
  opts.format = livekit::VideoBufferType::RGBA;

  std::weak_ptr<RemoteState> weakState = m_remoteState;
  m_room->setOnVideoFrameCallback(
      identity.toStdString(), source,
      [weakState, key, identity, name, sourceLabel]
      (const livekit::VideoFrame& frame, std::int64_t) {
        const auto state = weakState.lock();
        if (!state || frame.width() <= 0 || frame.height() <= 0 ||
            frame.dataSize() == 0) {
          return;
        }

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        {
          QMutexLocker locker(&state->mutex);
          auto& tile = state->tiles[key];
          if (tile.tileId.isEmpty()) {
            tile.tileId = key;
            tile.identity = identity;
            tile.displayName = name;
            tile.sourceText = sourceLabel;
            tile.isLocal = false;
          }
          if (nowMs - tile.lastFrameMs < kPreviewUpdateIntervalMs) {
            return;
          }
          tile.lastFrameMs = nowMs;
        }

        const QImage image(frame.data(), frame.width(), frame.height(),
                           QImage::Format_RGBA8888);
        if (image.isNull()) return;

        QByteArray pngBytes;
        QBuffer buffer(&pngBytes);
        if (!buffer.open(QIODevice::WriteOnly)) return;
        const QImage scaled =
            image.width() > 1280
                ? image.copy().scaledToWidth(1280, Qt::SmoothTransformation)
                : image.copy();
        if (!scaled.save(&buffer, "JPEG", 75)) return;

        const QString dataUrl =
            "data:image/jpeg;base64," +
            QString::fromLatin1(pngBytes.toBase64());
        QMutexLocker locker(&state->mutex);
        state->tiles[key].dataUrl = dataUrl;
      },
      opts);

  // UI 刷新推迟到主线程
  QMetaObject::invokeMethod(this, "doHandleTrackSubscribed",
                            Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------
// Private slots — always run on Qt main thread
// ---------------------------------------------------------------------------

void LiveKitRoomService::doHandleParticipantConnected() {
  if (!m_connected || !m_room) {
    return;
  }
  // 新参与者加入时立即刷新远端状态，确保无视频轨的用户也能实时出现占位 tile，
  // 无需等待下一次 80ms 轮询。
  refreshRemoteState();
  emit remoteStateDirty();
}

void LiveKitRoomService::doHandleParticipantDisconnected(
    const QString& identity) {
  if (!m_connected || !m_room) {
    return;
  }
  // 立即清除 SDK 侧帧回调（可能由 onTrackSubscribed 直接注册，尚未记入
  // m_streamBindings），但保留 m_streamBindings 让 refreshRemoteState() 做最终清理。
  try {
    m_room->clearOnVideoFrameCallback(identity.toStdString(),
                                      livekit::TrackSource::SOURCE_CAMERA);
  } catch (...) {
  }
  try {
    m_room->clearOnVideoFrameCallback(identity.toStdString(),
                                      livekit::TrackSource::SOURCE_SCREENSHARE);
  } catch (...) {
  }

  {
    QMutexLocker locker(&m_remoteState->mutex);
    for (auto it = m_remoteState->tiles.begin();
         it != m_remoteState->tiles.end();) {
      if (it.value().identity == identity) {
        it = m_remoteState->tiles.erase(it);
      } else {
        ++it;
      }
    }
  }

  emit remoteStateDirty();
}

void LiveKitRoomService::doHandleTrackUnpublished(
    const QString& identity, livekit::TrackSource source) {
  if (!m_connected || !m_room) {
    return;
  }
  const QString key = streamKey(identity, source);

  // 立即清除 SDK 侧帧回调（onTrackSubscribed 中直接注册的），
  // 但保留 m_streamBindings 让 refreshRemoteState() 做最终清理。
  try {
    m_room->clearOnVideoFrameCallback(identity.toStdString(), source);
  } catch (...) {
  }

  {
    QMutexLocker locker(&m_remoteState->mutex);
    m_remoteState->tiles.remove(key);
  }

  emit remoteStateDirty();
}

void LiveKitRoomService::doHandleTrackSubscribed() {
  if (!m_connected || !m_room) {
    return;
  }
  // 强制即时刷新远端状态，确保新订阅的轨道（尤其是先入会者已发布的
  // 屏幕共享）能在订阅完成的当下就开始接收帧回调。
  refreshRemoteState();
  emit remoteStateDirty();
}
