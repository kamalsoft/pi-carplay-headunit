#include "hal/pi/v4l2_decoder.h"

#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/rational.h>
}

namespace hu::hal::pi {

namespace {

uint64_t now_us() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

}  // namespace

struct V4l2Decoder::Impl {
  AVCodecContext* ctx = nullptr;
  AVPacket* pkt = nullptr;
  AVFrame* frame = nullptr;  // decoder output, NV12 in system memory (no hwaccel transfer on this path)

  ~Impl() {
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
  }
};

V4l2Decoder::V4l2Decoder(macos::CoreQueue& queue) : queue_(queue) {}

V4l2Decoder::~V4l2Decoder() = default;

bool V4l2Decoder::open(uint32_t width, uint32_t height) {
  close();
  if (width == 0 || height == 0 || width > INT_MAX || height > INT_MAX) return false;

  const AVCodec* codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
  if (!codec) {
    std::fprintf(stderr, "v4l2 decoder: h264_v4l2m2m is not in this FFmpeg build\n");
    return false;
  }

  auto impl = std::make_unique<Impl>();
  impl->ctx = avcodec_alloc_context3(codec);
  impl->pkt = av_packet_alloc();
  impl->frame = av_frame_alloc();
  if (!impl->ctx || !impl->pkt || !impl->frame) return false;

  impl->ctx->pkt_timebase = AVRational{1, 1000000};  // pts is microseconds
  impl->ctx->width = static_cast<int>(width);
  impl->ctx->height = static_cast<int>(height);
  const int rc = avcodec_open2(impl->ctx, codec, nullptr);
  if (rc < 0) {
    char msg[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(rc, msg, sizeof msg);
    std::fprintf(stderr, "v4l2 decoder: avcodec_open2(h264_v4l2m2m) failed: %s\n", msg);
    return false;
  }

  impl_ = std::move(impl);
  return true;
}

void V4l2Decoder::close() {
  if (!impl_) return;
  // Send EOF so the hardware returns the frames still in flight, then free the context.
  avcodec_send_packet(impl_->ctx, nullptr);
  receive_frames(now_us());
  impl_.reset();
}

bool V4l2Decoder::submit(const uint8_t* annexb, size_t size, uint64_t pts_us) {
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

void V4l2Decoder::set_frame_sink(FrameSink sink) { sink_ = std::move(sink); }

hal::DecodeStats V4l2Decoder::stats() const { return stats_; }

void V4l2Decoder::receive_frames(uint64_t submit_us) {
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

// Latency is submit-to-output for the packet that produced the frame. With reordering or
// decoder queueing, that is an approximation (same rule as the VideoToolbox backend).
void V4l2Decoder::deliver(uint64_t submit_us) {
  AVFrame* f = impl_->frame;
  // bcm2835-codec returns YU12 (YUV420P) on the capture side; NV12 is accepted too. Anything else is dropped.
  const bool planar = f->format == AV_PIX_FMT_YUV420P;
  if (!planar && f->format != AV_PIX_FMT_NV12) {
    ++stats_.frames_dropped;
    return;
  }

  const uint32_t w = static_cast<uint32_t>(f->width);
  const uint32_t h = static_cast<uint32_t>(f->height);
  const size_t cw = (w + 1) / 2;
  const size_t ch = (h + 1) / 2;
  auto nv12 = std::make_shared<std::vector<uint8_t>>(size_t{w} * h + 2 * cw * ch);
  uint8_t* out = nv12->data();
  for (uint32_t row = 0; row < h; ++row) {
    std::memcpy(out + size_t{row} * w, f->data[0] + size_t{row} * f->linesize[0], w);
  }
  uint8_t* uv = out + size_t{w} * h;
  for (size_t row = 0; row < ch; ++row) {
    if (!planar) {
      std::memcpy(uv + row * 2 * cw, f->data[1] + row * f->linesize[1], 2 * cw);
      continue;
    }
    // Planar U and V planes are interleaved into NV12 UV pairs.
    const uint8_t* u = f->data[1] + row * f->linesize[1];
    const uint8_t* v = f->data[2] + row * f->linesize[2];
    for (size_t x = 0; x < cw; ++x) {
      uv[row * 2 * cw + 2 * x] = u[x];
      uv[row * 2 * cw + 2 * x + 1] = v[x];
    }
  }

  ++stats_.frames_decoded;
  const uint64_t now = now_us();
  stats_.last_latency_us = now > submit_us ? static_cast<uint32_t>(now - submit_us) : 0;
  if (!sink_) return;
  const uint64_t pts = static_cast<uint64_t>(f->pts);
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

}  // namespace hu::hal::pi
