#include "hal/macos/videotoolbox_decoder.h"

#include <chrono>
#include <climits>
#include <cstring>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/rational.h>
}

namespace hu::hal::macos {

namespace {

uint64_t now_us() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

// Accept only VideoToolbox. Returning NONE makes avcodec fail the open, so no software fallback.
enum AVPixelFormat pick_videotoolbox(AVCodecContext*, const enum AVPixelFormat* formats) {
  for (const enum AVPixelFormat* f = formats; *f != AV_PIX_FMT_NONE; ++f) {
    if (*f == AV_PIX_FMT_VIDEOTOOLBOX) return *f;
  }
  return AV_PIX_FMT_NONE;
}

}  // namespace

struct VideoToolboxDecoder::Impl {
  AVCodecContext* ctx = nullptr;
  AVBufferRef* hw_device = nullptr;
  AVPacket* pkt = nullptr;
  AVFrame* frame = nullptr;  // decoder output, VideoToolbox surface
  AVFrame* sw = nullptr;     // NV12 copy in system memory

  ~Impl() {
    av_frame_free(&sw);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    av_buffer_unref(&hw_device);
  }
};

VideoToolboxDecoder::VideoToolboxDecoder(CoreQueue& queue) : queue_(queue) {}

VideoToolboxDecoder::~VideoToolboxDecoder() = default;

bool VideoToolboxDecoder::open(uint32_t width, uint32_t height) {
  close();
  if (width == 0 || height == 0 || width > INT_MAX || height > INT_MAX) return false;

  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) return false;

  auto impl = std::make_unique<Impl>();
  impl->ctx = avcodec_alloc_context3(codec);
  impl->pkt = av_packet_alloc();
  impl->frame = av_frame_alloc();
  impl->sw = av_frame_alloc();
  if (!impl->ctx || !impl->pkt || !impl->frame || !impl->sw) return false;

  if (av_hwdevice_ctx_create(&impl->hw_device, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0) < 0) return false;
  impl->ctx->hw_device_ctx = av_buffer_ref(impl->hw_device);
  impl->ctx->get_format = pick_videotoolbox;
  impl->ctx->pkt_timebase = AVRational{1, 1000000};  // pts is microseconds
  impl->ctx->width = static_cast<int>(width);
  impl->ctx->height = static_cast<int>(height);
  if (!impl->ctx->hw_device_ctx || avcodec_open2(impl->ctx, codec, nullptr) < 0) return false;

  impl_ = std::move(impl);
  return true;
}

void VideoToolboxDecoder::close() { impl_.reset(); }

bool VideoToolboxDecoder::submit(const uint8_t* annexb, size_t size, uint64_t pts_us) {
  if (!impl_ || !annexb || size == 0 || size > INT_MAX || pts_us > INT64_MAX) {
    ++stats_.frames_dropped;
    return false;
  }
  const uint64_t submit_us = now_us();
  AVPacket* pkt = impl_->pkt;
  if (av_new_packet(pkt, static_cast<int>(size)) < 0) {
    ++stats_.frames_dropped;
    return false;
  }
  std::memcpy(pkt->data, annexb, size);
  pkt->pts = static_cast<int64_t>(pts_us);

  int rc = avcodec_send_packet(impl_->ctx, pkt);
  if (rc == AVERROR(EAGAIN)) {  // output full: drain, then resend once
    receive_frames(submit_us);
    rc = avcodec_send_packet(impl_->ctx, pkt);
  }
  av_packet_unref(pkt);
  if (rc < 0) {
    ++stats_.frames_dropped;
    return false;
  }
  receive_frames(submit_us);
  return true;
}

void VideoToolboxDecoder::set_frame_sink(FrameSink sink) { sink_ = std::move(sink); }

hal::DecodeStats VideoToolboxDecoder::stats() const { return stats_; }

void VideoToolboxDecoder::receive_frames(uint64_t submit_us) {
  for (;;) {
    const int rc = avcodec_receive_frame(impl_->ctx, impl_->frame);
    if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) return;
    if (rc < 0) {
      ++stats_.frames_dropped;
      return;
    }
    deliver(submit_us);
    av_frame_unref(impl_->frame);
  }
}

// Latency is submit-to-output for the packet that produced the frame. With reordering, that is an approximation.
void VideoToolboxDecoder::deliver(uint64_t submit_us) {
  AVFrame* sw = impl_->sw;
  av_frame_unref(sw);
  sw->format = AV_PIX_FMT_NV12;
  if (av_hwframe_transfer_data(sw, impl_->frame, 0) < 0 || sw->format != AV_PIX_FMT_NV12) {
    ++stats_.frames_dropped;
    return;
  }

  const uint32_t w = static_cast<uint32_t>(sw->width);
  const uint32_t h = static_cast<uint32_t>(sw->height);
  const size_t cw = (w + 1) / 2;
  const size_t ch = (h + 1) / 2;
  auto nv12 = std::make_shared<std::vector<uint8_t>>(size_t{w} * h + 2 * cw * ch);
  uint8_t* out = nv12->data();
  for (uint32_t row = 0; row < h; ++row) {
    std::memcpy(out + size_t{row} * w, sw->data[0] + size_t{row} * sw->linesize[0], w);
  }
  uint8_t* uv = out + size_t{w} * h;
  for (size_t row = 0; row < ch; ++row) {
    std::memcpy(uv + row * 2 * cw, sw->data[1] + row * sw->linesize[1], 2 * cw);
  }

  ++stats_.frames_decoded;
  const uint64_t now = now_us();
  stats_.last_latency_us = now > submit_us ? static_cast<uint32_t>(now - submit_us) : 0;
  if (!sink_) return;
  const uint64_t pts = static_cast<uint64_t>(impl_->frame->pts);
  queue_.post([sink = sink_, nv12, w, h, pts] {
    VideoFrame frame;
    frame.width = w;
    frame.height = h;
    frame.format = PixelFormat::Nv12;
    frame.pts_us = pts;
    frame.data = nv12->data();
    frame.size = nv12->size();
    sink(frame);
  });
}

}  // namespace hu::hal::macos
