///////////////////////////////////////////////////////////////////////////////
//
//  Mp3AudioData.cpp
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

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <limits>

#include "File.h"
#include "Log.h"
#include "Mp3AudioData.h"
#include "MpegFrameHdr.h"
#include "Util.h"

///////////////////////////////////////////////////////////////////////////////
//
// Useful reference material
//
// https://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header

using namespace PKIsensee;
namespace fs = std::filesystem;

constexpr uint8_t kMpegHdrByte = 0xFF; // first 8 bits of MPEG header are always ones

constexpr double kMillisecondsPerSecond = 1000.0;

// Search no further than this in the MPEG (MP3) file to find the first MPEG frame.
// In practice, audio data can begin at quite large offsets.
constexpr uint64_t kMaxMpegFrameHeaderSearch = 500 * 1024;

constexpr size_t kMinMpegFrames = 3; // find this many frames to assume this is an MPEG file

///////////////////////////////////////////////////////////////////////////////
//
// Open file and extract audio content into memory. Hint may be used to skip
// ID3 tag.

bool Mp3AudioData::Load( const std::filesystem::path& mp3FileName, uint64_t fileAudioOffsetHint )
{
  File mp3File( mp3FileName );
  if( !mp3File.Open( FileFlags::Read | FileFlags::SharedRead | FileFlags::SequentialScan ) )
    return false;

  auto fileLength = mp3File.GetLength();
  assert( fileAudioOffsetHint < fileLength );
  if( fileAudioOffsetHint >= fileLength )
    fileAudioOffsetHint = 0;
  auto audioBufferSize64 = fileLength - fileAudioOffsetHint;
  assert( audioBufferSize64 <= std::numeric_limits<uint32_t>::max() );
  auto audioBufferSize = static_cast<uint32_t>( audioBufferSize64 );

  // Skip leading ID3v2 tag when hint non-zero
  [[maybe_unused]] bool isSuccessfulSkip = mp3File.SetPos( fileAudioOffsetHint );
  assert( isSuccessfulSkip );

  // Read file into memory
  mAudioBuffer.resize( audioBufferSize );
  if( !mp3File.Read( mAudioBuffer.data(), audioBufferSize ) )
  {
    PKLOG_WARN( "Failed to read MP3 file %S; ERR: %d\n", mp3FileName.c_str(), Util::GetLastError() );
    return false;
  }
  mp3File.Close();

  // Look for the first kMinMpegFrames MPEG Version 1 Layer III frames that are 
  // consistent within the first kMaxMpegFrameHeaderSearch bytes
  auto countFrames = 0u;
  const uint8_t* pFirstMpegFrame = nullptr;
  auto pRawBuffer = reinterpret_cast<const uint8_t*>( mAudioBuffer.data() );
  auto pEnd = pRawBuffer + std::min( audioBufferSize64, kMaxMpegFrameHeaderSearch );
  for( auto offset = 0u; pRawBuffer < pEnd; pRawBuffer += offset )
  {
    // Search for an MPEG header
    offset = 1;
    if( *pRawBuffer != kMpegHdrByte )
      continue;

    // Check if this is a valid V1 Layer III frame
    MpegFrameHdr frameHdr( pRawBuffer );
    if( frameHdr.IsValid() &&
        frameHdr.GetVersion() == MpegVersion::V1 &&
        frameHdr.GetLayer() == MpegLayer::LayerIII )
    {
      if( pFirstMpegFrame == nullptr )
      {
        pFirstMpegFrame = pRawBuffer;
        mFirstMpegFrameHdr = frameHdr;
      }
      if( ++countFrames == kMinMpegFrames )
      {
        // Found a few valid frames; exit loop
        break;
      }
      offset = frameHdr.GetFrameBytes();
    }
  }

  // If not enough valid frames, probably not an MPEG file
  if( countFrames < kMinMpegFrames )
    return false;

  // A good bet this is an MPEG file
  // Return to the first frame and parse the entire stream
  assert( pFirstMpegFrame >= mAudioBuffer.data() );
  uint32_t firstFrameOffset = static_cast<uint32_t>( pFirstMpegFrame - mAudioBuffer.data() );
  assert( firstFrameOffset <= audioBufferSize );
  ParseFrames( pFirstMpegFrame, audioBufferSize - firstFrameOffset );
  return true;
}

bool Mp3AudioData::HasMpegAudio() const
{
  return mFirstMpegFrameHdr.IsValid();
}

MpegVersion Mp3AudioData::GetVersion() const
{
  return mFirstMpegFrameHdr.GetVersion();
}

MpegLayer Mp3AudioData::GetLayer() const
{
  return mFirstMpegFrameHdr.GetLayer();
}

uint32_t Mp3AudioData::GetDurationMs() const
{
  return static_cast<uint32_t>( std::round( mDurationSec * kMillisecondsPerSecond ) );
}

size_t Mp3AudioData::GetFrameCount() const
{
  return mFrameCount;
}

uint32_t Mp3AudioData::GetSamplingRateHz() const
{
  return mFirstMpegFrameHdr.GetSamplingRateHz();
}

uint32_t Mp3AudioData::GetChannelCount() const
{
  switch( mFirstMpegFrameHdr.GetChannelMode() )
  {
  case ( MpegChannelMode::SingleChannel ): return 1;
  case ( MpegChannelMode::Stereo ):        [[fallthrough]];
  case ( MpegChannelMode::JointStereo ):   [[fallthrough]];
  case ( MpegChannelMode::DualChannel ):   return 2;
  }
  assert( false );
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// Extract data from MPEG frames
//
// Current implementation performs debug mode validation, counts the frames,
// and determines total duration of all frames in milliseconds.
// 
// If frames needed to be maintained, store pRawBuffer pointers in a vector;
// not currently required.

void Mp3AudioData::ParseFrames( const uint8_t* pRawBuffer, uint32_t bufferSize ) // private
{
  mDurationSec = 0.0; // TODO durationSec_
  auto pEnd = pRawBuffer + bufferSize;
  for( auto offset = 0u; pRawBuffer < pEnd; pRawBuffer += offset )
  {
    // Search for an MPEG header
    offset = 1;
    if( *pRawBuffer != kMpegHdrByte )
      continue;
    MpegFrameHdr frameHdr( pRawBuffer );
    if( !frameHdr.IsValid() )
      continue;

    offset = frameHdr.GetFrameBytes();
    mDurationSec += frameHdr.GetFrameDurationInSeconds();
    ++mFrameCount;
  }

  // Ensure we didn't miss any frames (debugging only)
  for( ; pRawBuffer < pEnd; ++pRawBuffer )
  {
    if( *pRawBuffer == 0xFF )
      assert( !MpegFrameHdr( pRawBuffer ).IsValid() );
  }
}

///////////////////////////////////////////////////////////////////////////////
