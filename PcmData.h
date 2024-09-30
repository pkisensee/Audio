///////////////////////////////////////////////////////////////////////////////
//
//  PcmData.h
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
#include <filesystem>
#include <memory>
#include <vector>

namespace PKIsensee
{

constexpr uint32_t kPcmBitsPerSample = 16;

enum class PcmChannelCount
{
  Mono = 1,
  Stereo = 2,
};

class PcmData
{
public:

  PcmData() = default;
  PcmData( PcmChannelCount channelCount, uint32_t samplesPerSecond );

  // Disable copy/move
  PcmData( const PcmData& ) = default;
  PcmData& operator=( const PcmData& ) = default;
  PcmData( PcmData&& ) = default;
  PcmData& operator=( PcmData&& ) = default;

  PcmChannelCount GetChannelCount() const
  {
    return channelCount_;
  }

  uint32_t GetChannelCountAsInt() const
  {
    return uint32_t( channelCount_ );
  }

  void SetChannelCount( PcmChannelCount channelCount )
  {
    channelCount_ = channelCount;
  }

  void SetChannelCountAsInt( uint32_t channelCount )
  {
    channelCount_ = ( channelCount == 1 ) ? PcmChannelCount::Mono : PcmChannelCount::Stereo;
  }

  uint32_t GetBitsPerSample() const
  {
    return bitsPerSample_;
  }

  uint32_t GetSamplesPerSecond() const
  {
    return samplesPerSecond_;
  }

  void SetSamplesPerSecond( uint32_t samplesPerSecond )
  {
    samplesPerSecond_ = samplesPerSecond;
  }

  size_t GetSize() const
  {
    return pcmBuffer_->size();
  }

  const uint8_t* GetPtr() const
  {
    return pcmBuffer_->data();
  }

  uint32_t GetBlockAlignment() const
  {
    return GetChannelCountAsInt() * bitsPerSample_ / CHAR_BIT;
  }

  uint32_t GetBytesPerSec() const
  {
    return GetBlockAlignment() * GetSamplesPerSecond();
  }

  uint32_t BytesToMilliseconds( uint32_t bytePosition ) const;
  uint32_t MillisecondsToBytes( uint32_t positionMs ) const;

  void PrepareBuffer( uint32_t audioMilliseconds );
  void AppendPcm( const uint8_t* pData, uint32_t bytes );
  bool WriteToWavFile( const std::filesystem::path& ) const;

private:

  uint32_t GetEstDataSize( uint32_t audioMilliseconds ) const;

private:

  PcmChannelCount channelCount_ = PcmChannelCount::Mono;
  uint32_t bitsPerSample_ = kPcmBitsPerSample;
  uint32_t samplesPerSecond_ = 0u;

  // shared_ptr allows for fast copies of PCM data
  using PcmBuffer = std::vector<uint8_t>;
  std::shared_ptr<PcmBuffer> pcmBuffer_;

}; // class PcmData

} // namespace PKIsensee

///////////////////////////////////////////////////////////////////////////////

