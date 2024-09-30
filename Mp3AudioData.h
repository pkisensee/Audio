///////////////////////////////////////////////////////////////////////////////
//
//  Mp3AudioData.h
//
//  Copyright © Pete Isensee (PKIsensee@msn.com).
//  All rights reserved worldwide.
//
//  Permission to copy, modify, reproduce or redistribute this source code is
//  granted provided the above copyright notice is retained in the resulting 
//  source code.
// 
//  This software is provided "as is" and without any express or implied
//  warranties.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

#include "MpegFrameHdr.h"
#include "PcmData.h"

namespace PKIsensee
{

class Mp3AudioData
{
public:

  Mp3AudioData() = default;

  // Disable copy/move
  Mp3AudioData( const Mp3AudioData& ) = delete;
  Mp3AudioData& operator=( const Mp3AudioData& ) = delete;
  Mp3AudioData( Mp3AudioData&& ) = delete;
  Mp3AudioData& operator=( Mp3AudioData&& ) = delete;

  bool Load( const std::filesystem::path&, uint64_t fileAudioOffsetHint = 0u );
  bool HasMpegAudio() const;
  MpegVersion GetVersion() const;
  MpegLayer GetLayer() const;
  uint32_t GetDurationMs() const;
  size_t GetFrameCount() const;
  uint32_t GetSamplingRateHz() const;
  uint32_t GetChannelCount() const;
  PcmData ExtractPcmData() const;

private:

  bool IsValidFileHeader() const;
  void ParseFrames( const uint8_t*, uint32_t );

private:

  std::vector<uint8_t> audioBuffer_;
  MpegFrameHdr firstMpegFrameHdr_;
  double durationSec_ = 0.0; // seconds
  size_t frameCount_ = 0u;

}; // class Mp3AudioData

} // namespace PKIsensee

///////////////////////////////////////////////////////////////////////////////
