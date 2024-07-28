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
    return mChannelCount;
  }

  uint32_t GetChannelCountAsInt() const
  {
    return static_cast<uint32_t>( mChannelCount );
  }

  void SetChannelCount( PcmChannelCount channelCount )
  {
    mChannelCount = channelCount;
  }

  void SetChannelCountAsInt( uint32_t channelCount )
  {
    mChannelCount = ( channelCount == 1 ) ? PcmChannelCount::Mono : PcmChannelCount::Stereo;
  }

  uint32_t GetBitsPerSample() const
  {
    return mBitsPerSample;
  }

  uint32_t GetSamplesPerSecond() const
  {
    return mSamplesPerSecond;
  }

  void SetSamplesPerSecond( uint32_t samplesPerSecond )
  {
    mSamplesPerSecond = samplesPerSecond;
  }

  size_t GetSize() const
  {
    return mPcm->size();
  }

  const uint8_t* GetPtr() const
  {
    return mPcm->data();
  }

  uint32_t GetBlockAlignment() const
  {
    return GetChannelCountAsInt() * mBitsPerSample / CHAR_BIT;
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

  PcmChannelCount mChannelCount = PcmChannelCount::Mono;
  uint32_t mBitsPerSample = kPcmBitsPerSample;
  uint32_t mSamplesPerSecond = 0;

  // shared_ptr allows for fast copies of PcmData
  using PcmBuffer = std::vector<uint8_t>;
  std::shared_ptr<PcmBuffer> mPcm;

}; // class PcmData

} // namespace PKIsensee

///////////////////////////////////////////////////////////////////////////////

