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

#include "ColumnPrinter.hh"
#include "Exceptions.hh"

#include <string>
#include <memory>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: file-dump <filename>\n";
    return 1;
  }
  std::cout << std::nounitbuf;

  orc::ReaderOptions opts;
  std::list<int> cols;
  cols.push_back(0);
  opts.include(cols);

  std::unique_ptr<orc::Reader> reader;
  try{
    reader = orc::createReader(orc::readLocalFile(std::string(argv[1])), opts);
  } catch (orc::ParseError e) {
    std::cout << "Error reading file " << argv[1] << "! "
              << e.what() << std::endl;
    return -1;
  }

  std::unique_ptr<orc::ColumnVectorBatch> batch = reader->createRowBatch(1000);
  orc::StructColumnPrinter printer(*batch);

  while (reader->next(*batch)) {
    printer.reset(*batch);
    for(unsigned long i=0; i < batch->numElements; ++i) {
      printer.printRow(i);
    }
  }
  return 0;
}
