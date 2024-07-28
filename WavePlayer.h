///////////////////////////////////////////////////////////////////////////////
//
//  WavePlayer.h
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
#include <cassert>
#include <limits>

#include "PcmData.h"
#include "Util.h"
#include "WaveOut.h"

namespace PKIsensee
{

constexpr uint32_t kDefaultUpdateWaitMilliseconds = 4;

class WavePlayer
{
public:

  WavePlayer() = default;

  // Disable copy/move
  WavePlayer( const WavePlayer& ) = delete;
  WavePlayer& operator=( const WavePlayer& ) = delete;
  WavePlayer( WavePlayer&& ) = delete;
  WavePlayer& operator=( WavePlayer&& ) = delete;

  bool IsPlaying() const
  {
    return mWaveOut.IsPlaying();
  }

  bool HasEnded() const
  {
    return mWaveOut.HasEnded();
  }

  bool LoadMp3( const std::filesystem::path& mp3File );

  void Start( uint32_t positionMs = 0u )
  {
    mWaveEvent.Reset();
    mWaveOut.Prepare( positionMs );
    mWaveOut.Start();
  }

  void Pause()
  {
    mWaveOut.Pause();
  }

  void Restart()
  {
    mWaveOut.Start();
  }

  void Update( uint32_t waitMilliseconds = kDefaultUpdateWaitMilliseconds )
  {
    if( mWaveEvent.IsSignalled( waitMilliseconds ) )
      mWaveOut.Update();
  }

  uint32_t GetLengthMs() const
  {
    assert( mPcmData.GetSize() <= std::numeric_limits<uint32_t>::max() );
    return mPcmData.BytesToMilliseconds( static_cast<uint32_t>( mPcmData.GetSize() ) );
  }

  uint32_t GetPositionMs() const
  {
    return mWaveOut.GetPositionMs();
  }

  WaveOut::Volume GetVolume() const
  {
    return mWaveOut.GetVolume();
  }

  void SetVolume( const WaveOut::Volume& volume )
  {
    return mWaveOut.SetVolume( volume );
  }

private:

  PcmData     mPcmData;   // raw PCM buffer
  Util::Event mWaveEvent; // signaled when mWaveOut is ready for more data
  WaveOut     mWaveOut;

};

}; // end namespace PKIsensee

///////////////////////////////////////////////////////////////////////////////
