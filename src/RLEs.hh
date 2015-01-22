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

#ifndef ORC_RLES_HH
#define ORC_RLES_HH

#include "Compression.hh"
#include "RLE.hh"

#include <memory>

namespace orc {

enum RleVersion {
  RleVersion_1,
  RleVersion_2
};

/**
* Create an RLE decoder.
* @param input the input stream to read from
* @param isSigned true if the number sequence is signed
* @param version version of RLE decoding to do
*/
std::auto_ptr<RleDecoder> createRleDecoder(
    std::auto_ptr<SeekableInputStream> input,
    bool isSigned,
    RleVersion version);

}  // namespace orc

#endif  // ORC_RLES_HH