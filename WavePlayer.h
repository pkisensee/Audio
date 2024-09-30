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

  bool LoadMp3( const std::filesystem::path& mp3File );

  bool IsPlaying() const
  {
    return waveOut_.IsPlaying();
  }

  bool HasEnded() const
  {
    return waveOut_.HasEnded();
  }

  void Start( uint32_t positionMs = 0u )
  {
    waveEvent_.Reset();
    waveOut_.Prepare( positionMs );
    waveOut_.Start();
  }

  void Pause()
  {
    waveOut_.Pause();
  }

  void Restart()
  {
    waveOut_.Start();
  }

  void Update( uint32_t waitMilliseconds = kDefaultUpdateWaitMilliseconds )
  {
    if( waveEvent_.IsSignalled( waitMilliseconds ) )
      waveOut_.Update();
  }

  uint32_t GetLengthMs() const
  {
    assert( pcmData_.GetSize() <= std::numeric_limits<uint32_t>::max() );
    return pcmData_.BytesToMilliseconds( static_cast<uint32_t>( pcmData_.GetSize() ) );
  }

  uint32_t GetPositionMs() const
  {
    return waveOut_.GetPositionMs();
  }

  WaveOut::Volume GetVolume() const
  {
    return waveOut_.GetVolume();
  }

  void SetVolume( const WaveOut::Volume& volume )
  {
    return waveOut_.SetVolume( volume );
  }

private:

  PcmData     pcmData_;   // raw PCM buffer
  Util::Event waveEvent_; // signaled when waveOut_ is ready for more data
  WaveOut     waveOut_;

};

}; // end namespace PKIsensee

///////////////////////////////////////////////////////////////////////////////
