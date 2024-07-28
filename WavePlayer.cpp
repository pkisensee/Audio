///////////////////////////////////////////////////////////////////////////////
//
//  WavePlayer.cpp
//
//  Copyright � Pete Isensee (PKIsensee@msn.com).
//  All rights reserved worldwide.
//
//  Permission to copy, modify, reproduce or redistribute this source code is
//  granted provided the above copyright notice is retained in the resulting 
//  source code.
// 
//  This software is provided "as is" and without any express or   
//  warranties.
// 
//  Windows version of a PCM audio file playback engine
//
///////////////////////////////////////////////////////////////////////////////

#include "Mp3AudioData.h"
#include "WavePlayer.h"
#include "WinMediaFoundation.h"

using namespace PKIsensee;

///////////////////////////////////////////////////////////////////////////////

bool WavePlayer::LoadMp3( const std::filesystem::path& mp3File )
{
  // Scan MP3 file for channel count, sampling rate and audio length
  Mp3AudioData audioData;
  if( !audioData.Load( mp3File ) )
    return false;

  // Prepare PCM buffer
  mPcmData.SetChannelCountAsInt( audioData.GetChannelCount() );
  mPcmData.SetSamplesPerSecond( audioData.GetSamplingRateHz() );
  mPcmData.PrepareBuffer( audioData.GetDurationMs() );

  WinMediaFoundation mf;
  WinMediaSourceReader sourceReader( mp3File );

  // Ignore all but first audio stream
  sourceReader.UnselectStream( kAllStreams );
  sourceReader.SelectStream( kFirstAudioStream );

  // Decompress to PCM
  sourceReader.SelectOutput( kFirstAudioStream, WinMediaOutputType::PCM );
  WinMediaSample mediaSample;
  size_t pcmBytes = 0;
  while( sourceReader.ReadSample( kFirstAudioStream, mediaSample ) )
  {
    WinMediaBuffer mediaBuffer = mediaSample.GetMediaBuffer();
    WinMediaBufferLock lock( mediaBuffer );
    mPcmData.AppendPcm( lock.GetData(), lock.GetSize() );
    pcmBytes += lock.GetSize(); // for debugging
  }

  // Prepare the wave output device
  return mWaveOut.Open( mPcmData, mWaveEvent );
}