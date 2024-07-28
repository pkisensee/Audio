///////////////////////////////////////////////////////////////////////////////
//
//  MpegFrameHdr.h
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
#include <vector>

namespace PKIsensee
{

enum class MpegField
{
  FrameSync,
  VersionIndex,
  LayerIndex,
  ProtectionBit,
  BitrateIndex,
  SamplingRateFreqIndex,
  PaddingBit,
  ChannelMode,
  ModeExtension,
  Copyright,
  Original,
  Emphasis,

  Max
};

enum class MpegVersion
{
  None,
  V1,
  V2,
  V2_5
};

enum class MpegLayer
{
  None,
  LayerI,
  LayerII,
  LayerIII
};

enum class MpegChannelMode
{
  Stereo,
  JointStereo,
  DualChannel,
  SingleChannel // mono
};

class MpegFrameHdr
{
public:

  MpegFrameHdr() = default;
  explicit MpegFrameHdr( const uint8_t* );

  bool IsValid() const;

  MpegVersion GetVersion() const;
  MpegLayer GetLayer() const;
  MpegChannelMode GetChannelMode() const;

  uint32_t GetBitrateKbps() const;
  uint32_t GetSamplingRateHz() const;
  uint32_t GetSampleCount() const;
  uint32_t GetFrameBytes() const;
  double GetFrameDurationInSeconds() const;

  bool ProtectedByCRC() const;
  bool HasPaddingBit() const;
  bool IsIntensityStereoOn() const;
  bool IsMSStereoOn() const;
  bool IsCopyrighted() const;
  bool IsOriginal() const;

private:

  constexpr uint32_t ExtractBits( MpegField ) const;

private:

  uint32_t mpegHeader = 0u;

}; // class MpegFrameHdr

} // namespace PKIsensee

///////////////////////////////////////////////////////////////////////////////
