///////////////////////////////////////////////////////////////////////////////
//
//  PcmData.cpp
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

#include <cassert>

#include "File.h"
#include "PcmData.h"
#include "Util.h"

using namespace PKIsensee;
using namespace Util;

constexpr uint32_t kDecompressionCushionMs = 2000; // account for slightly incorrect song times
constexpr uint32_t kMillisecondsPerSecond = 1000;
constexpr double   kMillisecondsPerSecondAsDbl = 1000.0;
constexpr uint16_t kWaveFormatPCM = 1;

// size of packed WAVEFORMATEX struct (2 16-bit values, 2 32-bit, 3 16-bit)
constexpr uint32_t kWaveFormatExSize = ( 2u * 2u ) + ( 2u * 4u ) + ( 3u * 2u );

///////////////////////////////////////////////////////////////////////////////
//
// WAVE header
//
// Little endian format
// https://docs.microsoft.com/en-us/windows/win32/medfound/tutorial--decoding-audio

#pragma pack( push, 1 )
struct WaveHeader
{
  // RIFF header
  uint32_t chunkID = FourCC( "RIFF" );
  uint32_t fileSize = 0; // file size not including "RIFF" field and this field (8 bytes)
  uint32_t format = FourCC( "WAVE" );

  // Format header
  uint32_t formatID = FourCC( "fmt " );
  uint32_t formatSize = Util::ToLittleEndian( kWaveFormatExSize ); // size of next block below

  // WAVEFORMATEX data
  uint16_t wFormatTag = Util::ToLittleEndian( kWaveFormatPCM );
  uint16_t nChannels = 0;       // e.g. 2 for stereo
  uint32_t nSamplesPerSec = 0;  // e.g. 44100
  uint32_t nAvgBytesPerSec = 0; // e.g. nSamplesPerSec * nBlockAlign
  uint16_t nBlockAlign = 0;     // e.g. nChannels * wBitsPerSample / 8
  uint16_t wBitsPerSample = 0;  // e.g. 16
  uint16_t cbSize = 0;          // not used

  // Data header
  uint32_t dataID = FourCC( "data" );
  uint32_t dataSize = 0;
};
#pragma pack( pop )

PcmData::PcmData( PcmChannelCount channelCount, uint32_t samplesPerSecond )
  :
  mChannelCount( channelCount ),
  mSamplesPerSecond( samplesPerSecond ),
  mPcm( new PcmBuffer )
{
}

uint32_t PcmData::BytesToMilliseconds( uint32_t bytePosition ) const
{
  // ms = bytes / bytes per second * 1000
  auto bytesPerSec = static_cast<double>( GetBytesPerSec() );
  auto seconds = bytePosition / bytesPerSec;
  auto ms = static_cast<uint32_t>( std::round( seconds * kMillisecondsPerSecond ) );

  // Never refer to invalid section
  auto maxSeconds = static_cast<double>(GetSize()) / bytesPerSec;
  auto maxMs = static_cast<uint32_t>( std::round( maxSeconds * kMillisecondsPerSecond ) );
  return std::min( ms, maxMs );
}

uint32_t PcmData::MillisecondsToBytes( uint32_t positionMs ) const
{
  // bytes = ms * bytes per second / 1000
  auto bytesPerSec = static_cast<double>( GetBytesPerSec() );
  auto bytesPerMs = bytesPerSec / kMillisecondsPerSecondAsDbl;
  auto byteOffset = static_cast<uint32_t>( std::round( positionMs * bytesPerMs ) );

  // Block align
  auto align = GetBlockAlignment();
  byteOffset /= align;
  byteOffset *= align;

  // Never refer to invalid data
  auto maxBytes = static_cast<uint32_t>( GetSize() );
  return std::min( byteOffset, maxBytes );
}

void PcmData::PrepareBuffer( uint32_t audioMilliseconds )
{
  auto byteEstimate = GetEstDataSize( audioMilliseconds );
  if( !mPcm )
    mPcm.reset( new PcmBuffer );
  mPcm->reserve( byteEstimate );
  mPcm->resize( 0 );
}

void PcmData::AppendPcm( const uint8_t* pcmData, uint32_t pcmBytes )
{
  assert( mPcm );
  assert( mPcm->size() + pcmBytes <= mPcm->capacity() ); // try to avoid reallocation
  mPcm->insert( mPcm->end(), pcmData, pcmData + pcmBytes );
}

bool PcmData::WriteToWavFile( const std::filesystem::path& wavPath ) const
{
  uint32_t dataSize = uint32_t( mPcm->size() );

  WaveHeader wh;
  uint32_t fileSize  = static_cast<uint32_t>( sizeof( wh ) + dataSize - sizeof( wh.chunkID ) - sizeof( wh.fileSize ) );
  wh.fileSize        = Util::ToLittleEndian( fileSize );
  wh.nChannels       = Util::ToLittleEndian( static_cast<uint16_t>( mChannelCount ) );
  wh.wBitsPerSample  = Util::ToLittleEndian( static_cast<uint16_t>( mBitsPerSample ) );
  wh.nSamplesPerSec  = Util::ToLittleEndian( mSamplesPerSecond );
  uint16_t blockAlign= static_cast<uint16_t>( wh.nChannels * wh.wBitsPerSample / CHAR_BIT );
  wh.nBlockAlign     = Util::ToLittleEndian( blockAlign );
  wh.nAvgBytesPerSec = Util::ToLittleEndian( wh.nSamplesPerSec * wh.nBlockAlign );
  wh.dataSize        = Util::ToLittleEndian( dataSize );

  File wavFile( wavPath );
  if( !wavFile.Create( FileFlags::Write ) )
    return false;
  if( !wavFile.Write( &wh, sizeof( wh ) ) )
    return false;
  if( !wavFile.Write( mPcm->data(), dataSize ) )
    return false;
  return true;
}

uint32_t PcmData::GetEstDataSize( uint32_t audioMilliseconds ) const
{
  audioMilliseconds += kDecompressionCushionMs; // small add'l buffer

  // Take care with the math to avoid overflow; ms could be as large as 50M (12 hrs)
  // Divide before multiplying, even though we lose some precision
  // It's just an estimate after all

  auto sampleCount = audioMilliseconds / kMillisecondsPerSecond * mSamplesPerSecond;
  auto bytesPerSample = mBitsPerSample / CHAR_BIT; // 1-4
  auto channelByteCount = sampleCount * bytesPerSample;
  auto totalBytes = channelByteCount * static_cast<uint32_t>( mChannelCount ); // channels = 1-2

  assert( sampleCount > audioMilliseconds ); // check for overflow
  assert( totalBytes > channelByteCount ); // check for overflow

  return totalBytes;
}

///////////////////////////////////////////////////////////////////////////////
