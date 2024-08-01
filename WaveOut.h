///////////////////////////////////////////////////////////////////////////////
//
//  WaveOut.h
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
#include <memory>

#include "PcmData.h"
#include "Util.h"

namespace PKIsensee
{

constexpr size_t kDefaultWaveBufferCount = 2;

///////////////////////////////////////////////////////////////////////////////
//
// Audio device PCM playback interface

class WaveOut
{
public:

  WaveOut();

  // Disable copy/mov
  WaveOut( const WaveOut& ) = delete;
  WaveOut& operator=( const WaveOut& ) = delete;
  WaveOut( WaveOut&& ) = delete;
  WaveOut& operator=( WaveOut&& ) = delete;

  bool Open( const PcmData& pcmData, Util::Event& callbackEvent );
  void Prepare( uint32_t positionMs = 0, size_t waveBufferCount = kDefaultWaveBufferCount );
  void Start();
  void Pause();
  void Update(); // invoke when callbackEvent is signalled
  bool IsPlaying() const; // started and not paused
  bool HasEnded() const; // data queue is empty
  void Close();

  using VolumeType = uint16_t;
  using Volume = std::pair<VolumeType, VolumeType>; // left, right
  Volume GetVolume() const;
  void SetVolume( const Volume& );
  uint32_t GetPositionMs() const;

private:

  class Impl;
  using ImplDeleter = void ( * )( Impl* );
  std::unique_ptr<Impl, ImplDeleter> impl_;
};

}; // end namespace PKIsensee

///////////////////////////////////////////////////////////////////////////////

