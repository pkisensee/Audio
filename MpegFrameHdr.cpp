///////////////////////////////////////////////////////////////////////////////
//
//  MpegFrameHdr.cpp
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

#include "MpegFrameHdr.h"
#include "Util.h"
#include "..\frozen\unordered_map.h"

///////////////////////////////////////////////////////////////////////////////
//
// Useful reference material
//
// https://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header
// https://www.mp3-tech.org/programmer/frame_header.html
// http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.557.4662&rep=rep1&type=pdf
// https://www.underbit.com/products/mad
// https://github.com/FlorisCreyf/mp3-decoder

using namespace PKIsensee;

constexpr uint32_t kKilobitsPerSecond = 1000; // MPEG standard; not 1024
constexpr uint32_t kMpegHeaderSize = sizeof( uint32_t );
[[maybe_unused]] constexpr uint32_t kMpegHeaderBits = kMpegHeaderSize * CHAR_BIT; // 32

struct MpegFieldInfo
{
  uint32_t bitOffset;
  uint32_t bitCount;
  uint32_t bitMask;
};

///////////////////////////////////////////////////////////////////////////////
//
// Locations and size of fields within the MPEG header block

constexpr size_t kMaxMpegFields = static_cast<size_t>( MpegField::Max );
constexpr frozen::unordered_map< MpegField, MpegFieldInfo, kMaxMpegFields >
kMpegFieldInfo =
{
  // Field                           Offset Bits Bitmask
  //--------------------------------------------------------------
  { MpegField::FrameSync,             { 32, 11, 0b11111111111 } },
  { MpegField::VersionIndex,          { 21,  2, 0b11   } },
  { MpegField::LayerIndex,            { 19,  2, 0b11   } },
  { MpegField::ProtectionBit,         { 17,  1, 0b1    } },
  { MpegField::BitrateIndex,          { 16,  4, 0b1111 } },
  { MpegField::SamplingRateFreqIndex, { 12,  2, 0b11   } },
  { MpegField::PaddingBit,            { 10,  1, 0b1    } },
  { MpegField::ChannelMode,           {  8,  2, 0b11   } },
  { MpegField::ModeExtension,         {  6,  2, 0b11   } },
  { MpegField::Copyright,             {  4,  1, 0b1    } },
  { MpegField::Original,              {  3,  1, 0b1    } },
  { MpegField::Emphasis,              {  2,  2, 0b11   } },
};

///////////////////////////////////////////////////////////////////////////////
//
// These map directly to the values from the MPEG header

constexpr std::array<MpegVersion, 4> kMpegVersion =
{
  MpegVersion::V2_5,
  MpegVersion::None, // reserved
  MpegVersion::V2,
  MpegVersion::V1
};

constexpr std::array<MpegLayer, 4> kMpegLayer =
{
  MpegLayer::None, // reserved
  MpegLayer::LayerIII,
  MpegLayer::LayerII,
  MpegLayer::LayerI
};

constexpr std::array<MpegChannelMode, 4> kMpegChannelMode =
{
  MpegChannelMode::Stereo,
  MpegChannelMode::JointStereo,
  MpegChannelMode::DualChannel,
  MpegChannelMode::SingleChannel
};

constexpr uint32_t kGoodFrameSync = 0b11111111111; // 11 bits set

// Indicates reserved/bad/invalid indices
constexpr uint32_t kVersionIndexReserved = 0b01;
constexpr uint32_t kLayerIndexReserved = 0b0;
constexpr uint32_t kSamplingRateFreqIndexReserved = 0b11;
constexpr std::array<uint32_t, 2> kBitrateIndexReserved = { 0b0, 0b1111 };

///////////////////////////////////////////////////////////////////////////////
//
// Sampling rates in hertz
// Indices: VersionIndex, SamplingRateIndex
// Table from https://www.mp3-tech.org/programmer/frame_header.html
// Indices: VersionIndex, SamplingRateFreqIndex

constexpr std::array<std::array<uint32_t, 3>, 4> kSamplingRates =
{ {
  // SamplingRateFreqIndex (note: index 3 is reserved)
  //    0      1      2
  //----------------------
  { 11025, 12000,  8000 }, // V2.5
  {     0,     0,     0 }, // reserved
  { 22050, 24000, 16000 }, // V2
  { 44100, 48000, 32000 }  // V1
} };

// Samples per frame
// Indices: VersionIndex, LayerIndex
constexpr std::array<std::array<uint32_t, 4>, 4> kSamplesPerFrame =
{ {
// Layer: None  III    II    I
//-------------------------------
        { 0,    576, 1152, 384 }, // V2_5
        { 0                    }, // reserved
        { 0,    576, 1152, 384 }, // V2
        { 0,   1152, 1152, 384 }, // V1
} };

// Slot size per layer
// Index: LayerIndex
constexpr std::array<uint32_t, 4> kSlotSizes =
{
  0, // None
  1, // III
  1, // II
  4  // I
};

///////////////////////////////////////////////////////////////////////////////
//
// Bitrates in kbps
// Table from https://www.mp3-tech.org/programmer/frame_header.html
// Many extra zeros in this 960 byte table, but it simplifies the caller.
// Indices: BitrateIndex, VersionIndex, LayerIndex

constexpr std::array<std::array<std::array<uint32_t, 4>, 4>, 15> kBitrates =
{ {
// Ver:   2.5             None    V2                 V1
//-------------------------------------------------------------------------
// Layer: N  L3  L2  L1           N  L3  L2  L1      N  L3  L2  L1
//-------------------------------------------------------------------------
    { { { 0 },             {0}, { 0 },             { 0 }             } }, // Bitrate index 0
    { { { 0,  8,  8, 32 }, {0}, { 0,  8,  8, 32 }, { 0, 32, 32, 32 } } }, // 1
    { { { 0, 16, 16, 48 }, {0}, { 0, 16, 16, 48 }, { 0, 40, 48, 64 } } }, // 2
    { { { 0, 24, 24, 56 }, {0}, { 0, 24, 24, 56 }, { 0, 48, 56, 96 } } }, // 3
    { { { 0, 32, 32, 64 }, {0}, { 0, 32, 32, 64 }, { 0, 56, 64,128 } } }, // 4
    { { { 0, 40, 40, 80 }, {0}, { 0, 40, 40, 80 }, { 0, 64, 80,160 } } }, // 5
    { { { 0, 48, 48, 96 }, {0}, { 0, 48, 48, 96 }, { 0, 80, 96,192 } } }, // 6
    { { { 0, 56, 56,112 }, {0}, { 0, 56, 56,112 }, { 0, 96,112,224 } } }, // 7
    { { { 0, 64, 64,128 }, {0}, { 0, 64, 64,128 }, { 0,112,128,256 } } }, // 8
    { { { 0, 80, 80,144 }, {0}, { 0, 80, 80,144 }, { 0,128,160,288 } } }, // 9
    { { { 0, 96, 96,160 }, {0}, { 0, 96, 96,160 }, { 0,160,192,320 } } }, // 10
    { { { 0,112,112,176 }, {0}, { 0,112,112,176 }, { 0,192,224,352 } } }, // 11
    { { { 0,128,128,192 }, {0}, { 0,128,128,192 }, { 0,224,256,384 } } }, // 12
    { { { 0,144,144,224 }, {0}, { 0,144,144,224 }, { 0,256,320,416 } } }, // 13
    { { { 0,160,160,256 }, {0}, { 0,160,160,256 }, { 0,320,384,448 } } }  // 14

    // Index 15 is not allowed
} };

///////////////////////////////////////////////////////////////////////////////

MpegFrameHdr::MpegFrameHdr( const uint8_t* pMpegData )
  : 
  mpegHeader_( Util::ToBigEndian(*(reinterpret_cast<const uint32_t*>(pMpegData))) )
{
  assert( pMpegData != nullptr );
}

///////////////////////////////////////////////////////////////////////////////
//
// Determines if this is a correctly formatted header. Only safe to call 
// remaining functions when this is true.

bool MpegFrameHdr::IsValid() const
{
  // Look for first 11 bits set and no reserved indices used
  return ( ExtractBits( MpegField::FrameSync ) == kGoodFrameSync ) &&
         ( ExtractBits( MpegField::VersionIndex )          != kVersionIndexReserved ) &&
         ( ExtractBits( MpegField::LayerIndex )            != kLayerIndexReserved ) &&
         ( ExtractBits( MpegField::BitrateIndex )          != kBitrateIndexReserved[ 0 ] ) &&
         ( ExtractBits( MpegField::BitrateIndex )          != kBitrateIndexReserved[ 1 ] ) &&
         ( ExtractBits( MpegField::SamplingRateFreqIndex ) != kSamplingRateFreqIndexReserved );
}

///////////////////////////////////////////////////////////////////////////////
//
// Extract fields from header

MpegVersion MpegFrameHdr::GetVersion() const
{
  auto versionIndex = ExtractBits( MpegField::VersionIndex );
  return kMpegVersion[ versionIndex ];
}

MpegLayer MpegFrameHdr::GetLayer() const
{
  auto layerIndex = ExtractBits( MpegField::LayerIndex );
  return kMpegLayer[ layerIndex ];
}

MpegChannelMode MpegFrameHdr::GetChannelMode() const
{
  auto channelIndex = ExtractBits( MpegField::ChannelMode );
  return kMpegChannelMode[ channelIndex ];
}

uint32_t MpegFrameHdr::GetBitrateKbps() const // 1000 bits per second
{
  auto bitrateIndex = ExtractBits( MpegField::BitrateIndex );
  auto versionIndex = ExtractBits( MpegField::VersionIndex );
  auto layerIndex   = ExtractBits( MpegField::LayerIndex );

  return kBitrates[ bitrateIndex ][ versionIndex ][ layerIndex ];
}

uint32_t MpegFrameHdr::GetSamplingRateHz() const
{
  auto versionIndex  = ExtractBits( MpegField::VersionIndex );
  auto sampleRateIndex = ExtractBits( MpegField::SamplingRateFreqIndex );

  return kSamplingRates[ versionIndex ][ sampleRateIndex ];
}

uint32_t MpegFrameHdr::GetSampleCount() const
{
  auto versionIndex = ExtractBits( MpegField::VersionIndex );
  auto layerIndex   = ExtractBits( MpegField::LayerIndex );

  return kSamplesPerFrame[ versionIndex ][ layerIndex ];
}

///////////////////////////////////////////////////////////////////////////////
//
// Returns the number of bytes in this frame, including the header; useful for
// locating the next frame, which may OR MAY NOT be at this location
//
// See https://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header#MPEGAudioFrameHeader
// and https://hydrogenaud.io/index.php?topic=85125.0
// for details and code useful in computing this value

uint32_t MpegFrameHdr::GetFrameBytes() const
{
  auto bitrateIndex    = ExtractBits( MpegField::BitrateIndex );
  auto versionIndex    = ExtractBits( MpegField::VersionIndex );
  auto layerIndex      = ExtractBits( MpegField::LayerIndex );
  auto sampleRateIndex = ExtractBits( MpegField::SamplingRateFreqIndex );

  auto slotSize = kSlotSizes[ layerIndex ];
  auto padding = HasPaddingBit() ? 1u : 0u;
  auto paddingSize = slotSize * padding;

  auto samplesPerByte = kSamplesPerFrame[ versionIndex ][ layerIndex ] / CHAR_BIT / slotSize;
  auto bitRate = kBitrates[ bitrateIndex ][ versionIndex ][ layerIndex ] * kKilobitsPerSecond;
  auto samplingRate = kSamplingRates[ versionIndex ][ sampleRateIndex ];

  assert( samplesPerByte > 0u );
  assert( bitRate > 0u );
  assert( samplingRate > 0u );

  return ( samplesPerByte * bitRate / samplingRate ) + paddingSize;
}

///////////////////////////////////////////////////////////////////////////////
//
// Duration of the frame in seconds. Useful for computing the actual size of 
// variable bitrate encoded (VBR) files.

double MpegFrameHdr::GetFrameDurationInSeconds() const
{
  auto samples = static_cast<double>( GetSampleCount() );
  auto freq = static_cast<double>( GetSamplingRateHz() );
  auto seconds = samples / freq;
  return seconds;
}

///////////////////////////////////////////////////////////////////////////////
//
// True if frame header is immediately followed by 16-bit CRC.
// See https://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header#CRC
// for information about what CRC algorithm to use and what bytes
// in the frame should be used, which varies based on the version, layer,
// and stereo mode.

bool MpegFrameHdr::ProtectedByCRC() const
{
  return ExtractBits( MpegField::ProtectionBit ) == 0b1;
}

bool MpegFrameHdr::HasPaddingBit() const
{
  return !!ExtractBits( MpegField::PaddingBit );
}

bool MpegFrameHdr::IsIntensityStereoOn() const
{
  auto mode = ExtractBits( MpegField::ModeExtension );
  return ( mode & 0b1 );
}

bool MpegFrameHdr::IsMSStereoOn() const
{
  auto mode = ExtractBits( MpegField::ModeExtension );
  return ( mode & 0b10 );
}

bool MpegFrameHdr::IsCopyrighted() const
{
  return !!ExtractBits( MpegField::Copyright );
}

bool MpegFrameHdr::IsOriginal() const
{
  return !!ExtractBits( MpegField::Original );
}

///////////////////////////////////////////////////////////////////////////////
//
// Extracts the given value from mpegHeader

constexpr uint32_t MpegFrameHdr::ExtractBits( MpegField mpegField ) const // private
{
  MpegFieldInfo fi = kMpegFieldInfo.at( mpegField );

  assert( fi.bitOffset <= kMpegHeaderBits );
  static_cast<void>( kMpegHeaderBits ); // avoid spurious Visual Studio compiler warning
  assert( ( fi.bitCount == 1  && fi.bitMask == 0b1 ) ||
          ( fi.bitCount == 2  && fi.bitMask == 0b11 ) ||
          ( fi.bitCount == 4  && fi.bitMask == 0b1111 ) ||
          ( fi.bitCount == 11 && fi.bitMask == 0b11111111111 ) );

  auto shift = fi.bitOffset - fi.bitCount;
  fi.bitMask <<= shift;
  auto value = ( mpegHeader_ & fi.bitMask ) >> shift;
  return value;
}

///////////////////////////////////////////////////////////////////////////////
