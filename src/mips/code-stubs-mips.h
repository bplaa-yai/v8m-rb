// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_MIPS_CODE_STUBS_ARM_H_
#define V8_MIPS_CODE_STUBS_ARM_H_

#include "codegen-inl.h"
#include "ic-inl.h"
#include "ast.h"

namespace v8 {
namespace internal {


// Compute a transcendental math function natively, or call the
// TranscendentalCache runtime function.
class TranscendentalCacheStub: public CodeStub {
 public:
  explicit TranscendentalCacheStub(TranscendentalCache::Type type)
      : type_(type) {}
  void Generate(MacroAssembler* masm);
 private:
  TranscendentalCache::Type type_;
  Major MajorKey() { return TranscendentalCache; }
  int MinorKey() { return type_; }
  Runtime::FunctionId RuntimeFunction();
};


class ToBooleanStub: public CodeStub {
 public:
  explicit ToBooleanStub(Register tos) : tos_(tos) { }

  void Generate(MacroAssembler* masm);

 private:
  Register tos_;
  Major MajorKey() { return ToBoolean; }
  int MinorKey() { return tos_.code(); }
};


class GenericBinaryOpStub : public CodeStub {
 public:
  GenericBinaryOpStub(Token::Value op,
                      OverwriteMode mode,
                      Register lhs,
                      Register rhs,
                      int constant_rhs = CodeGenerator::kUnknownIntValue)
      : op_(op),
        mode_(mode),
        lhs_(lhs),
        rhs_(rhs),
        constant_rhs_(constant_rhs),
        specialized_on_rhs_(RhsIsOneWeWantToOptimizeFor(op, constant_rhs)),
        runtime_operands_type_(BinaryOpIC::DEFAULT),
        name_(NULL) { }

  GenericBinaryOpStub(int key, BinaryOpIC::TypeInfo type_info)
      : op_(OpBits::decode(key)),
        mode_(ModeBits::decode(key)),
        lhs_(LhsRegister(RegisterBits::decode(key))),
        rhs_(RhsRegister(RegisterBits::decode(key))),
        constant_rhs_(KnownBitsForMinorKey(KnownIntBits::decode(key))),
        specialized_on_rhs_(RhsIsOneWeWantToOptimizeFor(op_, constant_rhs_)),
        runtime_operands_type_(type_info),
        name_(NULL) { }

 private:
  Token::Value op_;
  OverwriteMode mode_;
  Register lhs_;
  Register rhs_;
  int constant_rhs_;
  bool specialized_on_rhs_;
  BinaryOpIC::TypeInfo runtime_operands_type_;
  char* name_;

  static const int kMaxKnownRhs = 0x40000000;
  static const int kKnownRhsKeyBits = 6;

  // Minor key encoding in 16 bits.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 6> {};
  class TypeInfoBits: public BitField<int, 8, 2> {};
  class RegisterBits: public BitField<bool, 10, 1> {};
  class KnownIntBits: public BitField<int, 11, kKnownRhsKeyBits> {};

  Major MajorKey() { return GenericBinaryOp; }
  int MinorKey() {
    ASSERT((lhs_.is(a0) && rhs_.is(a1)) ||
           (lhs_.is(a1) && rhs_.is(a0)));
    // Encode the parameters in a unique 16 bit value.
    return OpBits::encode(op_)
           | ModeBits::encode(mode_)
           | KnownIntBits::encode(MinorKeyForKnownInt())
           | TypeInfoBits::encode(runtime_operands_type_)
           | RegisterBits::encode(lhs_.is(a0));
  }

  void Generate(MacroAssembler* masm);
  void HandleNonSmiBitwiseOp(MacroAssembler* masm,
                             Register lhs,
                             Register rhs);
  void HandleBinaryOpSlowCases(MacroAssembler* masm,
                               Label* not_smi,
                               Register lhs,
                               Register rhs,
                               const Builtins::JavaScript& builtin);
  void GenerateTypeTransition(MacroAssembler* masm);

  static bool RhsIsOneWeWantToOptimizeFor(Token::Value op, int constant_rhs) {
    if (constant_rhs == CodeGenerator::kUnknownIntValue) return false;
    if (op == Token::DIV) return constant_rhs >= 2 && constant_rhs <= 3;
    if (op == Token::MOD) {
      if (constant_rhs <= 1) return false;
      if (constant_rhs <= 10) return true;
      if (constant_rhs <= kMaxKnownRhs && IsPowerOf2(constant_rhs)) return true;
      return false;
    }
    return false;
  }

  int MinorKeyForKnownInt() {
    if (!specialized_on_rhs_) return 0;
    if (constant_rhs_ <= 10) return constant_rhs_ + 1;
    ASSERT(IsPowerOf2(constant_rhs_));
    int key = 12;
    int d = constant_rhs_;
    while ((d & 1) == 0) {
      key++;
      d >>= 1;
    }
    ASSERT(key >= 0 && key < (1 << kKnownRhsKeyBits));
    return key;
  }

  int KnownBitsForMinorKey(int key) {
    if (!key) return 0;
    if (key <= 11) return key - 1;
    int d = 1;
    while (key != 12) {
      key--;
      d <<= 1;
    }
    return d;
  }

  Register LhsRegister(bool lhs_is_a0) {
    return lhs_is_a0 ? a0 : a1;
  }

  Register RhsRegister(bool lhs_is_a0) {
    return lhs_is_a0 ? a1 : a0;
  }

  bool ShouldGenerateSmiCode() {
    return ((op_ != Token::DIV && op_ != Token::MOD) || specialized_on_rhs_) &&
        runtime_operands_type_ != BinaryOpIC::HEAP_NUMBERS &&
        runtime_operands_type_ != BinaryOpIC::STRINGS;
  }

  bool ShouldGenerateFPCode() {
    return runtime_operands_type_ != BinaryOpIC::STRINGS;
  }

  virtual int GetCodeKind() { return Code::BINARY_OP_IC; }

  virtual InlineCacheState GetICState() {
    return BinaryOpIC::ToState(runtime_operands_type_);
  }

  const char* GetName();

#ifdef DEBUG
  void Print() {
    if (!specialized_on_rhs_) {
      PrintF("GenericBinaryOpStub (%s)\n", Token::String(op_));
    } else {
      PrintF("GenericBinaryOpStub (%s by %d)\n",
             Token::String(op_),
             constant_rhs_);
    }
  }
#endif
};


// Flag that indicates how to generate code for the stub StringAddStub.
enum StringAddFlags {
  NO_STRING_ADD_FLAGS = 0,
  NO_STRING_CHECK_IN_STUB = 1 << 0  // Omit string check in stub.
};


class StringAddStub: public CodeStub {
 public:
  explicit StringAddStub(StringAddFlags flags) {
    string_check_ = ((flags & NO_STRING_CHECK_IN_STUB) == 0);
  }

 private:
  Major MajorKey() { return StringAdd; }
  int MinorKey() { return string_check_ ? 0 : 1; }

  void Generate(MacroAssembler* masm);

  // Should the stub check whether arguments are strings?
  bool string_check_;
};


class SubStringStub: public CodeStub {
 public:
  SubStringStub() {}

 private:
  Major MajorKey() { return SubString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class StringCompareStub: public CodeStub {
 public:
  StringCompareStub() { }

  // Compare two flat ASCII strings and returns result in v0.
  // Does not use the stack.
  static void GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                              Register left,
                                              Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              Register scratch4);

 private:
  Major MajorKey() { return StringCompare; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


// This stub can convert a signed int32 to a heap number (double).  It does
// not work for int32s that are in Smi range!  No GC occurs during this stub
// so you don't have to set up the frame.
class WriteInt32ToHeapNumberStub : public CodeStub {
 public:
  WriteInt32ToHeapNumberStub(Register the_int,
                             Register the_heap_number,
                             Register scratch,
                             Register scratch2)
      : the_int_(the_int),
        the_heap_number_(the_heap_number),
        scratch_(scratch),
        sign_(scratch2) { }

 private:
  Register the_int_;
  Register the_heap_number_;
  Register scratch_;
  Register sign_;

  // Minor key encoding in 16 bits.
  class IntRegisterBits: public BitField<int, 0, 4> {};
  class HeapNumberRegisterBits: public BitField<int, 4, 4> {};
  class ScratchRegisterBits: public BitField<int, 8, 4> {};

  Major MajorKey() { return WriteInt32ToHeapNumber; }
  int MinorKey() {
    // Encode the parameters in a unique 16 bit value.
    return IntRegisterBits::encode(the_int_.code())
           | HeapNumberRegisterBits::encode(the_heap_number_.code())
           | ScratchRegisterBits::encode(scratch_.code());
  }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "WriteInt32ToHeapNumberStub"; }

#ifdef DEBUG
  void Print() { PrintF("WriteInt32ToHeapNumberStub\n"); }
#endif
};


class NumberToStringStub: public CodeStub {
 public:
  NumberToStringStub() { }

  // Generate code to do a lookup in the number string cache. If the number in
  // the register object is found in the cache the generated code falls through
  // with the result in the result register. The object and the result register
  // can be the same. If the number is not found in the cache the code jumps to
  // the label not_found with only the content of register object unchanged.
  static void GenerateLookupNumberStringCache(MacroAssembler* masm,
                                              Register object,
                                              Register result,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              bool object_is_smi,
                                              Label* not_found);

 private:
  Major MajorKey() { return NumberToString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "NumberToStringStub"; }

#ifdef DEBUG
  void Print() {
    PrintF("NumberToStringStub\n");
  }
#endif
};


class RecordWriteStub : public CodeStub {
 public:
  RecordWriteStub(Register object, Register offset, Register scratch)
      : object_(object), offset_(offset), scratch_(scratch) { }

  void Generate(MacroAssembler* masm);

 private:
  Register object_;
  Register offset_;
  Register scratch_;

  // Minor key encoding in 12 bits. 4 bits for each of the three
  // registers (object, offset and scratch) OOOOAAAASSSS.
  class ScratchBits: public BitField<uint32_t, 0, 4> {};
  class OffsetBits: public BitField<uint32_t, 4, 4> {};
  class ObjectBits: public BitField<uint32_t, 8, 4> {};

  Major MajorKey() { return RecordWrite; }

  int MinorKey() {
    // Encode the registers.
    return ObjectBits::encode(object_.code()) |
           OffsetBits::encode(offset_.code()) |
           ScratchBits::encode(scratch_.code());
  }

#ifdef DEBUG
  void Print() {
    PrintF("RecordWriteStub (object reg %d), (offset reg %d),"
           " (scratch reg %d)\n",
           object_.code(), offset_.code(), scratch_.code());
  }
#endif
};


} }  // namespace v8::internal

#endif  // V8_MIPS_CODE_STUBS_ARM_H_
