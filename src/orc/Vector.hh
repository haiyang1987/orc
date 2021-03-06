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

#ifndef ORC_VECTOR_HH
#define ORC_VECTOR_HH

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "C09Adapter.hh"

namespace orc {

  enum TypeKind {
    BOOLEAN = 0,
    BYTE = 1,
    SHORT = 2,
    INT = 3,
    LONG = 4,
    FLOAT = 5,
    DOUBLE = 6,
    STRING = 7,
    BINARY = 8,
    TIMESTAMP = 9,
    LIST = 10,
    MAP = 11,
    STRUCT = 12,
    UNION = 13,
    DECIMAL = 14,
    DATE = 15,
    VARCHAR = 16,
    CHAR = 17
  };

  class Type {
  public:
    virtual ~Type();
    virtual int assignIds(int root) = 0;
    virtual int getColumnId() const = 0;
    virtual TypeKind getKind() const = 0;
    virtual unsigned int getSubtypeCount() const = 0;
    virtual const Type& getSubtype(unsigned int typeId) const = 0;
    virtual const std::string& getFieldName(unsigned int fieldId) const = 0;
    virtual unsigned int getMaximumLength() const = 0;
    virtual unsigned int getPrecision() const = 0;
    virtual unsigned int getScale() const = 0;
  };

  const int DEFAULT_DECIMAL_SCALE = 18;
  const int DEFAULT_DECIMAL_PRECISION = 38;

  std::unique_ptr<Type> createPrimitiveType(TypeKind kind);
  std::unique_ptr<Type> createCharType(TypeKind kind,
                                       unsigned int maxLength);
  std::unique_ptr<Type>
                createDecimalType(unsigned int precision=
                                    DEFAULT_DECIMAL_PRECISION,
                                  unsigned int scale=DEFAULT_DECIMAL_SCALE);
  std::unique_ptr<Type>
    createStructType(std::vector<Type*> types,
                      std::vector<std::string> fieldNames);

#if __cplusplus >= 201103L
  std::unique_ptr<Type> createStructType(
      std::initializer_list<std::unique_ptr<Type> > types,
      std::initializer_list<std::string> fieldNames);
#endif // __cplusplus


  std::unique_ptr<Type> createListType(std::unique_ptr<Type> elements);
  std::unique_ptr<Type> createMapType(std::unique_ptr<Type> key,
                                      std::unique_ptr<Type> value);
  std::unique_ptr<Type>
    createUnionType(std::vector<Type*> types);

  /**
   * The base class for each of the column vectors. This class handles
   * the generic attributes such as number of elements, capacity, and
   * notNull vector.
   */
  struct ColumnVectorBatch {
    ColumnVectorBatch(uint64_t capacity);
    virtual ~ColumnVectorBatch();

    // the number of slots available
    uint64_t capacity;
    // the number of current occupied slots
    uint64_t numElements;
    // an array of capacity length marking non-null values
    std::vector<char> notNull;
    // whether there are any null values
    bool hasNulls;

    /**
     * Generate a description of this vector as a string.
     */
    virtual std::string toString() const = 0;

    /**
     * Change the number of slots to at least the given capacity.
     * This function is not recursive into subtypes.
     */
    virtual void resize(uint64_t capacity);

  private:
    ColumnVectorBatch(const ColumnVectorBatch&);
    ColumnVectorBatch& operator=(const ColumnVectorBatch&);
  };

  struct LongVectorBatch: public ColumnVectorBatch {
    LongVectorBatch(uint64_t capacity);
    virtual ~LongVectorBatch();
    std::vector<int64_t> data;
    std::string toString() const;
    void resize(uint64_t capacity);
  };

  struct DoubleVectorBatch: public ColumnVectorBatch {
    DoubleVectorBatch(uint64_t capacity);
    virtual ~DoubleVectorBatch();
    std::string toString() const;
    void resize(uint64_t capacity);

    std::vector<double> data;
  };

  struct StringVectorBatch: public ColumnVectorBatch {
    StringVectorBatch(uint64_t capacity);
    virtual ~StringVectorBatch();
    std::string toString() const;
    void resize(uint64_t capacity);

    // pointers to the start of each string
    std::vector<char*> data;
    // the length of each string
    std::vector<int64_t> length;
  };

  struct StructVectorBatch: public ColumnVectorBatch {
    StructVectorBatch(uint64_t capacity);
    virtual ~StructVectorBatch();
    std::string toString() const;
    void resize(uint64_t capacity);

    std::vector<ColumnVectorBatch*> fields;
  };

  struct ListVectorBatch: public ColumnVectorBatch {
    ListVectorBatch(uint64_t capacity);
    virtual ~ListVectorBatch();
    std::string toString() const;
    void resize(uint64_t capacity);

    /**
     * The offset of the first element of each list.
     * The length of list i is startOffset[i+1] - startOffset[i].
     */
    std::vector<int64_t> offsets;

    // the concatenated elements
    std::unique_ptr<ColumnVectorBatch> elements;
  };

  struct MapVectorBatch: public ColumnVectorBatch {
    MapVectorBatch(uint64_t capacity);
    virtual ~MapVectorBatch();
    std::string toString() const;
    void resize(uint64_t capacity);

    /**
     * The offset of the first element of each list.
     * The length of list i is startOffset[i+1] - startOffset[i].
     */
    std::vector<int64_t> offsets;

    // the concatenated keys
    std::unique_ptr<ColumnVectorBatch> keys;
    // the concatenated elements
    std::unique_ptr<ColumnVectorBatch> elements;
  };

  struct Decimal {
    long upper;
    long lower;
  };
}

#endif
