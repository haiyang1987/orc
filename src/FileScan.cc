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

//#include "orc/OrcFile.hh"
#include "ColumnPrinter.hh"

#include <string>
#include <memory>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: file-scan <filename>\n";
  }
  orc::ReaderOptions opts;
  // opts.include({1});
  std::auto_ptr<orc::Reader> reader =
    orc::createReader(orc::readLocalFile(std::string(argv[1])), opts);
  std::auto_ptr<orc::ColumnVectorBatch> batch = reader->createRowBatch(1024);

  unsigned long rows = 0;
  unsigned long batches = 0;
  while (reader->next(*batch)) {
    batches += 1;
    if(DEBUG) { std::cout << "Reading batch " << batches << std::endl; }
    rows += batch->numElements;
  }
  std::cout << "Rows: " << rows << "\n";
  std::cout << "Batches: " << batches << "\n";
  return 0;
}