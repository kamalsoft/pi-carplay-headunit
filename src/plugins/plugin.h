#pragma once

// HeadUnitPlugin lifecycle interface (CLAUDE.md §5 Step 3). Plugins are
// called from the core thread only. A plugin must not block it.

#include <cstdint>
#include <string_view>

#include "core/config.h"

namespace hu::plugin {

enum class AudioFocus : uint8_t { None, Media, Navigation };

struct CarPlaySession {
  uint32_t id = 0;
  bool wireless = false;
};

// Declared limits for the core to enforce later. Zero means no declared limit.
struct PluginBudget {
  uint32_t max_rss_kb = 0;
  uint8_t max_cpu_percent = 0;
};

class HeadUnitPlugin {
 public:
  virtual ~HeadUnitPlugin() = default;

  virtual std::string_view name() const = 0;
  virtual PluginBudget budget() const { return {}; }

  // Called once at startup, before any connect. Returning false disables the plugin; the core keeps running.
  virtual bool init(const hu::Config& config) = 0;
  virtual void onCarPlayConnect(const CarPlaySession& session) = 0;
  virtual void onCarPlayDisconnect(const CarPlaySession& session) = 0;
  virtual void onAudioFocusChange(AudioFocus focus) = 0;
  // Power throttling (CLAUDE.md §3.2). Suspend releases non-essential work; resume restores it.
  virtual void onSuspend() = 0;
  virtual void onResume() = 0;
  virtual void onShutdown() = 0;
};

}  // namespace hu::plugin
