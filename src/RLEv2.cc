/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "RLEv2.hh"
#include "Compression.hh"
#include "Exceptions.hh"

#define MIN_REPEAT 3

namespace orc {

struct FixedBitSizes {
  enum FBS {
    ONE = 0, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, ELEVEN, TWELVE,
    THIRTEEN, FOURTEEN, FIFTEEN, SIXTEEN, SEVENTEEN, EIGHTEEN, NINETEEN,
    TWENTY, TWENTYONE, TWENTYTWO, TWENTYTHREE, TWENTYFOUR, TWENTYSIX,
    TWENTYEIGHT, THIRTY, THIRTYTWO, FORTY, FORTYEIGHT, FIFTYSIX, SIXTYFOUR
  };
};

inline uint32_t decodeBitWidth(uint32_t n) {
  if (n >= FixedBitSizes::ONE &&
      n <= FixedBitSizes::TWENTYFOUR) {
    return n + 1;
  } else if (n == FixedBitSizes::TWENTYSIX) {
    return 26;
  } else if (n == FixedBitSizes::TWENTYEIGHT) {
    return 28;
  } else if (n == FixedBitSizes::THIRTY) {
    return 30;
  } else if (n == FixedBitSizes::THIRTYTWO) {
    return 32;
  } else if (n == FixedBitSizes::FORTY) {
    return 40;
  } else if (n == FixedBitSizes::FORTYEIGHT) {
    return 48;
  } else if (n == FixedBitSizes::FIFTYSIX) {
    return 56;
  } else {
    return 64;
  }
}

inline uint32_t getClosestFixedBits(uint32_t n) {
  if (n == 0) {
    return 1;
  }

  if (n >= 1 && n <= 24) {
    return n;
  } else if (n > 24 && n <= 26) {
    return 26;
  } else if (n > 26 && n <= 28) {
    return 28;
  } else if (n > 28 && n <= 30) {
    return 30;
  } else if (n > 30 && n <= 32) {
    return 32;
  } else if (n > 32 && n <= 40) {
    return 40;
  } else if (n > 40 && n <= 48) {
    return 48;
  } else if (n > 48 && n <= 56) {
    return 56;
  } else {
    return 64;
  }
}

uint64_t RleDecoderV2::readLongs(int64_t *data, uint64_t offset,
                                 uint64_t len, uint64_t fb,
                                 const char *notNull) {
  unsigned long ret = 0;

  // TODO: unroll to improve performance
  for(unsigned long i = offset; i < (offset + len); i++) {
    // skip null positions
    if (notNull && !notNull[i]) {
      continue;
    }
    uint64_t result = 0;
    uint64_t bitsLeftToRead = fb;
    while (bitsLeftToRead > bitsLeft) {
      result <<= bitsLeft;
      result |= curByte & ((1 << bitsLeft) - 1);
      bitsLeftToRead -= bitsLeft;
      curByte = readByte();
      bitsLeft = 8;
    }

    // handle the left over bits
    if (bitsLeftToRead > 0) {
      result <<= bitsLeftToRead;
      bitsLeft -= bitsLeftToRead;
      result |= (curByte >> bitsLeft) & ((1 << bitsLeftToRead) - 1);
    }
    data[i] = static_cast<long>(result);
    ++ret;
  }

  return ret;
}

unsigned char RleDecoderV2::readByte() {
  if (bufferStart == bufferEnd) {
    int bufferLength;
    const void* bufferPointer;
    if (!inputStream->Next(&bufferPointer, &bufferLength)) {
      throw ParseError("bad read in readByte");
    }
    bufferStart = static_cast<const char*>(bufferPointer);
    bufferEnd = bufferStart + bufferLength;
  }

  unsigned char result = static_cast<unsigned char>(*bufferStart++);
  return result;
}

long RleDecoderV2::readLongBE(unsigned bsz) {
  long ret = 0, val;
  unsigned n = bsz;
  while (n > 0) {
    n--;
    val = readByte();
    ret |= (val << (n * 8));
  }
  return ret;
}

inline int64_t RleDecoderV2::readVslong() {
  return unZigZag(readVulong());
}

uint64_t RleDecoderV2::readVulong() {
  uint64_t ret = 0, b;
  uint64_t offset = 0;
  do {
    b = readByte();
    ret |= (0x7f & b) << offset;
    offset += 7;
  } while (b >= 0x80);
  return ret;
}

RleDecoderV2::RleDecoderV2(std::unique_ptr<SeekableInputStream> input,
                           bool _isSigned)
  : inputStream(std::move(input)),
    isSigned(_isSigned),
    firstByte(0),
    runLength(0),
    runRead(0),
    bufferStart(nullptr),
    bufferEnd(bufferStart),
    deltaBase(0),
    byteSize(0),
    firstValue(0),
    prevValue(0),
    bitSize(0),
    bitsLeft(0),
    curByte(0),
    patchBitSize(0),
    base(0),
    curGap(0),
    patchMask(0),
    actualGap(0) {
}

void RleDecoderV2::seek(PositionProvider& location) {
  // move the input stream
  inputStream->seek(location);
  // clear state
  bufferEnd = bufferStart = 0;
  runRead = runLength = 0;
  // skip ahead the given number of records
  skip(location.next());
}

void RleDecoderV2::skip(unsigned long numValues) {
  // simple for now, until perf tests indicate something encoding specific is
  // needed
  const unsigned long N = 64;
  int64_t dummy[N];

  while (numValues) {
    unsigned long nRead = std::min(N, numValues);
    next(dummy, nRead, nullptr);
    numValues -= nRead;
  }
}

void RleDecoderV2::next(int64_t* const data,
                        const unsigned long numValues,
                        const char* const notNull) {
  unsigned long nRead = 0;
  while (nRead < numValues) {
    if (runRead == runLength) {
      firstByte = readByte();
    }

    unsigned long offset = nRead, length = numValues - nRead;

    EncodingType enc = static_cast<EncodingType>
        ((firstByte >> 6) & 0x03);
    switch(static_cast<int>(enc)) {
    case SHORT_REPEAT:
      nRead += nextShortRepeats(data, offset, length, notNull);
      break;
    case DIRECT:
      nRead += nextDirect(data, offset, length, notNull);
      break;
    case PATCHED_BASE:
      nRead += nextPatched(data, offset, length, notNull);
      break;
    case DELTA:
      nRead += nextDelta(data, offset, length, notNull);
      break;
    default:
      throw ParseError("unknown encoding");
    }
  }
}

uint64_t RleDecoderV2::nextShortRepeats(int64_t* const data,
                                        uint64_t offset,
                                        uint64_t numValues,
                                        const char* const notNull) {
  if (runRead == runLength) {
    // extract the number of fixed bytes
    byteSize = (firstByte >> 3) & 0x07;
    byteSize += 1;

    runLength = firstByte & 0x07;
    // run lengths values are stored only after MIN_REPEAT value is met
    runLength += MIN_REPEAT;
    runRead = 0;

    // read the repeated value which is store using fixed bytes
    firstValue = readLongBE(byteSize);

    if (isSigned) {
      firstValue = unZigZag(static_cast<uint64_t>(firstValue));
    }
  }

  unsigned long nRead = std::min(runLength - runRead, numValues);

  if (notNull) {
    for(unsigned long pos = offset; pos < offset + nRead; ++pos) {
      if (notNull[pos]) {
        data[pos] = firstValue;
        ++runRead;
      }
    }
  } else {
    for(unsigned long pos = offset; pos < offset + nRead; ++pos) {
      data[pos] = firstValue;
      ++runRead;
    }
  }

  return nRead;
}

uint64_t RleDecoderV2::nextDirect(int64_t* const data,
                                  uint64_t offset,
                                  uint64_t numValues,
                                  const char* const notNull) {
  if (runRead == runLength) {
    // extract the number of fixed bits
    unsigned char fbo = (firstByte >> 1) & 0x1f;
    bitSize = decodeBitWidth(fbo);
    bitsLeft = 0;
    curByte = 0;

    // extract the run length
    runLength = static_cast<uint64_t>(firstByte & 0x01) << 8;
    runLength |= readByte();
    // runs are one off
    runLength += 1;
    runRead = 0;
  }

  unsigned long nRead = std::min(runLength - runRead, numValues);

  runRead += readLongs(data, offset, nRead, bitSize, notNull);
  if (isSigned) {
    if (notNull) {
      for (uint64_t pos = offset; pos < offset + nRead; ++pos) {
        if (notNull[pos]) {
          data[pos] = unZigZag(static_cast<uint64_t>(data[pos]));
        }
      }
    } else {
      for (uint64_t pos = offset; pos < offset + nRead; ++pos) {
        data[pos] = unZigZag(static_cast<uint64_t>(data[pos]));
      }
    }
  }

  return nRead;
}

uint64_t RleDecoderV2::nextPatched(int64_t* const data,
                                   uint64_t offset,
                                   uint64_t numValues,
                                   const char* const notNull) {
  if (runRead == runLength) {
    // extract the number of fixed bits
    unsigned char fbo = (firstByte >> 1) & 0x1f;
    bitSize = decodeBitWidth(fbo);

    // extract the run length
    runLength = static_cast<uint64_t>(firstByte & 0x01) << 8;
    runLength |= readByte();
    // runs are one off
    runLength += 1;
    runRead = 0;

    // extract the number of bytes occupied by base
    unsigned int thirdByte = readByte();
    byteSize = (thirdByte >> 5) & 0x07;
    // base width is one off
    byteSize += 1;

    // extract patch width
    uint32_t pwo = thirdByte & 0x1f;
    patchBitSize = decodeBitWidth(pwo);

    // read fourth byte and extract patch gap width
    unsigned int fourthByte = readByte();
    uint32_t pgw = (fourthByte >> 5) & 0x07;
    // patch gap width is one off
    pgw += 1;

    // extract the length of the patch list
    size_t pl = fourthByte & 0x1f;

    // read the next base width number of bytes to extract base value
    base = readLongBE(byteSize);
    long mask = (1L << ((byteSize * 8) - 1));
    // if mask of base value is 1 then base is negative value else positive
    if ((base & mask) != 0) {
      base = base & ~mask;
      base = -base;
    }

    // TODO: something more efficient than resize
    unpacked.resize(runLength);
    unpackedIdx = 0;
    readLongs(unpacked.data(), 0, runLength, bitSize);
    // any remaining bits are thrown out
    bitsLeft = 0;

    // TODO: something more efficient than resize
    unpackedPatch.resize(pl);
    patchIdx = 0;
    // TODO: Skip corrupt?
    //    if ((patchBitSize + pgw) > 64 && !skipCorrupt) {
    if ((patchBitSize + pgw) > 64) {
      throw ParseError("Corrupt PATCHED_BASE encoded data!");
    }
    uint32_t cfb = getClosestFixedBits(patchBitSize + pgw);
    readLongs(unpackedPatch.data(), 0, pl, cfb);
    // any remaining bits are thrown out
    bitsLeft = 0;

    // apply the patch directly when decoding the packed data
    patchMask = ((1L << patchBitSize) - 1);

    adjustGapAndPatch();
  }

  unsigned long nRead = std::min(runLength - runRead, numValues);

  for(unsigned long pos = offset; pos < offset + nRead; ++pos) {
    // skip null positions
    if (notNull && !notNull[pos]) {
      continue;
    }
    if (static_cast<long>(unpackedIdx) != actualGap) {
      // no patching required. add base to unpacked value to get final value
      data[pos] = base + unpacked[unpackedIdx];
    } else {
      // extract the patch value
      long patchedVal = unpacked[unpackedIdx] | (curPatch << bitSize);

      // add base to patched value
      data[pos] = base + patchedVal;

      // increment the patch to point to next entry in patch list
      ++patchIdx;

      if (patchIdx < unpackedPatch.size()) {
        adjustGapAndPatch();

        // next gap is relative to the current gap
        actualGap += unpackedIdx;
      }
    }

    ++runRead;
    ++unpackedIdx;
  }

  return nRead;
}

uint64_t RleDecoderV2::nextDelta(int64_t* const data,
                                 uint64_t offset,
                                 uint64_t numValues,
                                 const char* const notNull) {
  if (runRead == runLength) {
    // extract the number of fixed bits
    unsigned char fbo = (firstByte >> 1) & 0x1f;
    if (fbo != 0) {
      bitSize = decodeBitWidth(fbo);
    } else {
      bitSize = 0;
    }

    // extract the run length
    runLength = static_cast<uint64_t>(firstByte & 0x01) << 8;
    runLength |= readByte();
    ++runLength; // account for first value
    runRead = deltaBase = 0;

    // read the first value stored as vint
    if (isSigned) {
      firstValue = static_cast<long>(readVslong());
    } else {
      firstValue = static_cast<long>(readVulong());
    }

    prevValue = firstValue;

    // read the fixed delta value stored as vint (deltas can be negative even
    // if all number are positive)
    deltaBase = static_cast<long>(readVslong());
  }

  unsigned long nRead = std::min(runLength - runRead, numValues);

  unsigned long pos = offset;
  for ( ; pos < offset + nRead; ++pos) {
    // skip null positions
    if (!notNull || notNull[pos]) break;
  }
  if (runRead == 0 && pos < offset + nRead) {
    data[pos++] = firstValue;
    ++runRead;
  }

  if (bitSize == 0) {
    // add fixed deltas to adjacent values
    for ( ; pos < offset + nRead; ++pos) {
      // skip null positions
      if (notNull && !notNull[pos]) {
        continue;
      }
      prevValue = data[pos] = prevValue + deltaBase;
      ++runRead;
    }
  } else {
    for ( ; pos < offset + nRead; ++pos) {
      // skip null positions
      if (!notNull || notNull[pos]) break;
    }
    if (runRead < 2 && pos < offset + nRead) {
      // add delta base and first value
      prevValue = data[pos++] = firstValue + deltaBase;
      ++runRead;
    }

    // write the unpacked values, add it to previous value and store final
    // value to result buffer. if the delta base value is negative then it
    // is a decreasing sequence else an increasing sequence
    unsigned long remaining = (offset + nRead) - pos;
    runRead += readLongs(data, pos, remaining, bitSize, notNull);
    if (deltaBase < 0) {
      for ( ; pos < offset + nRead; ++pos) {
        // skip null positions
        if (notNull && !notNull[pos]) {
          continue;
        }
        prevValue = data[pos] = prevValue - data[pos];
      }
    } else {
      for ( ; pos < offset + nRead; ++pos) {
        // skip null positions
        if (notNull && !notNull[pos]) {
          continue;
        }
        prevValue = data[pos] = prevValue + data[pos];
      }
    }
  }

  return nRead;
}

}  // namespace orc
