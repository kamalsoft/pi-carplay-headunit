#include "hal/macos/coreaudio_output.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

#include <AudioToolbox/AudioToolbox.h>

namespace hu::hal::macos {

namespace {

// 20 ms per AudioQueue buffer, 4 buffers, and a 500 ms ring (the same buffer model as the simulator).
constexpr uint32_t kBufferMs = 20;
constexpr size_t kBufferCount = 4;
constexpr uint32_t kRingMs = 500;

}  // namespace

struct CoreAudioOutput::Channel {
  AudioQueueRef queue = nullptr;
  AudioQueueBufferRef buffers[kBufferCount] = {};
  hal::PcmFormat format;
  size_t bytes_per_frame = 0;
  float gain = 1.0f;  // kept across close/open

  mutable std::mutex mutex;  // ring state: written by the core thread, read by the audio thread
  std::vector<uint8_t> ring;
  size_t head = 0;
  size_t count = 0;
  std::atomic<uint64_t> underruns{0};

  // Audio thread. Copies up to one buffer from the ring, pads with silence, and counts a shortfall as an underrun.
  void fill(AudioQueueBufferRef buf) {
    const size_t want = buf->mAudioDataBytesCapacity;
    std::lock_guard<std::mutex> lock(mutex);
    const size_t n = std::min(want, count);
    const size_t first = std::min(n, ring.size() - head);
    std::memcpy(buf->mAudioData, ring.data() + head, first);
    std::memcpy(static_cast<uint8_t*>(buf->mAudioData) + first, ring.data(), n - first);
    std::memset(static_cast<uint8_t*>(buf->mAudioData) + n, 0, want - n);
    head = (head + n) % ring.size();
    count -= n;
    if (n < want) underruns.fetch_add(1, std::memory_order_relaxed);
    buf->mAudioDataByteSize = static_cast<UInt32>(want);
  }
};

namespace {

void on_buffer(void* user, AudioQueueRef queue, AudioQueueBufferRef buf) {
  auto* channel = static_cast<CoreAudioOutput::Channel*>(user);
  channel->fill(buf);
  AudioQueueEnqueueBuffer(queue, buf, 0, nullptr);
}

}  // namespace

CoreAudioOutput::CoreAudioOutput() {
  for (auto& ch : channels_) ch = std::make_unique<Channel>();
}

CoreAudioOutput::~CoreAudioOutput() {
  for (uint8_t i = 0; i < channels_.size(); ++i) close(static_cast<hal::AudioChannel>(i));
}

CoreAudioOutput::Channel* CoreAudioOutput::slot(hal::AudioChannel channel) const {
  const size_t index = static_cast<size_t>(channel);
  return index < channels_.size() ? channels_[index].get() : nullptr;
}

bool CoreAudioOutput::open(hal::AudioChannel channel, hal::PcmFormat format, const std::string& device) {
  Channel* ch = slot(channel);
  if (!ch) return false;
  // Only 16-bit PCM is accepted; the HAL contract and the simulator both use S16.
  if (format.sample_rate == 0 || format.channels == 0 || format.bits_per_sample != 16) return false;
  close(channel);

  ch->format = format;
  ch->bytes_per_frame = size_t{format.channels} * (format.bits_per_sample / 8);
  AudioStreamBasicDescription desc{};
  desc.mSampleRate = format.sample_rate;
  desc.mFormatID = kAudioFormatLinearPCM;
  desc.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  desc.mBytesPerPacket = static_cast<UInt32>(ch->bytes_per_frame);
  desc.mFramesPerPacket = 1;
  desc.mBytesPerFrame = static_cast<UInt32>(ch->bytes_per_frame);
  desc.mChannelsPerFrame = format.channels;
  desc.mBitsPerChannel = format.bits_per_sample;

  if (AudioQueueNewOutput(&desc, on_buffer, ch, nullptr, nullptr, 0, &ch->queue) != noErr) return false;
  if (!device.empty()) {
    CFStringRef uid = CFStringCreateWithCString(nullptr, device.c_str(), kCFStringEncodingUTF8);
    const OSStatus rc = uid ? AudioQueueSetProperty(ch->queue, kAudioQueueProperty_CurrentDevice, &uid, sizeof(uid)) : -1;
    if (uid) CFRelease(uid);
    if (rc != noErr) {
      close(channel);
      return false;
    }
  }

  const size_t buffer_bytes = ch->bytes_per_frame * format.sample_rate * kBufferMs / 1000;
  const size_t ring_bytes = ch->bytes_per_frame * format.sample_rate * kRingMs / 1000;
  {
    std::lock_guard<std::mutex> lock(ch->mutex);
    ch->ring.assign(ring_bytes, 0);
    ch->head = 0;
    ch->count = 0;
  }
  for (size_t i = 0; i < kBufferCount; ++i) {
    if (AudioQueueAllocateBuffer(ch->queue, static_cast<UInt32>(buffer_bytes), &ch->buffers[i]) != noErr) {
      close(channel);
      return false;
    }
    std::memset(ch->buffers[i]->mAudioData, 0, buffer_bytes);  // priming is silence, not an underrun
    ch->buffers[i]->mAudioDataByteSize = static_cast<UInt32>(buffer_bytes);
    AudioQueueEnqueueBuffer(ch->queue, ch->buffers[i], 0, nullptr);
  }
  AudioQueueSetParameter(ch->queue, kAudioQueueParam_Volume, ch->gain);  // keeps gain across reopen
  if (AudioQueueStart(ch->queue, nullptr) != noErr) {
    close(channel);
    return false;
  }
  return true;
}

void CoreAudioOutput::close(hal::AudioChannel channel) {
  Channel* ch = slot(channel);
  if (!ch || !ch->queue) return;
  AudioQueueStop(ch->queue, true);      // synchronous: no callback runs after this
  AudioQueueDispose(ch->queue, true);   // frees the buffers too
  ch->queue = nullptr;
  for (auto& b : ch->buffers) b = nullptr;
}

size_t CoreAudioOutput::write(hal::AudioChannel channel, const uint8_t* pcm, size_t size) {
  Channel* ch = slot(channel);
  if (!ch || !ch->queue || !pcm) return 0;
  std::lock_guard<std::mutex> lock(ch->mutex);
  // Whole frames only, so the Media and Navigation streams never desync.
  const size_t free_bytes = ch->ring.size() - ch->count;
  size_t n = std::min(size, free_bytes);
  n -= n % ch->bytes_per_frame;
  const size_t tail = (ch->head + ch->count) % ch->ring.size();
  const size_t first = std::min(n, ch->ring.size() - tail);
  std::memcpy(ch->ring.data() + tail, pcm, first);
  std::memcpy(ch->ring.data(), pcm + first, n - first);
  ch->count += n;
  return n;
}

void CoreAudioOutput::set_gain(hal::AudioChannel channel, float gain) {
  Channel* ch = slot(channel);
  if (!ch) return;
  ch->gain = std::clamp(gain, 0.0f, 1.0f);
  if (ch->queue) AudioQueueSetParameter(ch->queue, kAudioQueueParam_Volume, ch->gain);
}

uint32_t CoreAudioOutput::buffered_ms(hal::AudioChannel channel) const {
  Channel* ch = slot(channel);
  if (!ch || !ch->queue || ch->format.sample_rate == 0) return 0;
  std::lock_guard<std::mutex> lock(ch->mutex);
  const size_t frames = ch->count / ch->bytes_per_frame;
  return static_cast<uint32_t>(frames * 1000 / ch->format.sample_rate);
}

uint64_t CoreAudioOutput::underruns(hal::AudioChannel channel) const {
  const Channel* ch = slot(channel);
  return ch ? ch->underruns.load(std::memory_order_relaxed) : 0;
}

}  // namespace hu::hal::macos
