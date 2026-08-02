#include <chrono>
#include <cstdint>
constexpr double kLowWatermark = 1.0;
constexpr double kHighWatermark = 2.0;
constexpr uint64_t kInitialWindowBytes = 4 * 1024 * 1024;
constexpr uint64_t kTailWindowBytes = 4 * 1024 * 1024;
constexpr uint32_t kTimeoutMs = 30000;
constexpr size_t kBufferSize = 256 * 1024;
constexpr int64_t kRepriorityStep = 4 * 1024 * 1024;
constexpr uint64_t kPriorityWindowBytes = 4 * 1024 * 1024;
constexpr auto kFileWaitTimeout = std::chrono::seconds(30);
constexpr uint64_t kProbeWindow = 4 * 1024 * 1024;
constexpr auto kMaxBufferingTime = std::chrono::seconds(10);
constexpr auto kSeekGracePeriod = std::chrono::seconds(4);
constexpr auto FONT_SIZE = 15;
constexpr auto kPanelHeight = 60;
constexpr int kProgressBarMargin = 20;
constexpr int kProgressBarHeight = 6;
constexpr int kProgressBarY = 10;
constexpr int kPlayButtonSize = 20;
constexpr int kPlayButtonMargin = 20;
constexpr int kVolumeBarWidth = 80;
constexpr int kVolumeBarHeight = 6;
constexpr auto kTimeoutMouse = std::chrono::seconds(3);
constexpr int kHitPadding = 10;
constexpr int kFullscreenIconSize = 16;
constexpr int kFullscreenIconMargin = 20;
constexpr int kThickness = 2;
