// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/compiler/cpp/cpp_message_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

void SetMessageVariables(const FieldDescriptor* descriptor,
                         map<string, string>* variables,
                         const Options& options) {
  SetCommonFieldVariables(descriptor, variables, options);
  (*variables)["type"] = FieldMessageTypeName(descriptor);
  (*variables)["stream_writer"] = (*variables)["declared_type"] +
      (HasFastArraySerialization(descriptor->message_type()->file()) ?
       "MaybeToArray" :
       "");
  // NOTE: Escaped here to unblock proto1->proto2 migration.
  // TODO(liujisi): Extend this to apply for other conflicting methods.
  (*variables)["release_name"] =
      SafeFunctionName(descriptor->containing_type(),
                       descriptor, "release_");
  (*variables)["full_name"] = descriptor->full_name();
}

}  // namespace

// ===================================================================

MessageFieldGenerator::
MessageFieldGenerator(const FieldDescriptor* descriptor,
                      const Options& options)
  : descriptor_(descriptor) {
  SetMessageVariables(descriptor, &variables_, options);
}

MessageFieldGenerator::~MessageFieldGenerator() {}

void MessageFieldGenerator::
GeneratePrivateMembers(io::Printer* printer) const {
  printer->Print(variables_, "$type$ $name$_ = $type$();\n");
}

void MessageFieldGenerator::
GenerateAccessorDeclarations(io::Printer* printer) const {
  printer->Print(variables_,
    "inline const $type$& $name$() const$deprecation$;\n");
}

void MessageFieldGenerator::
GenerateInlineAccessorDefinitions(io::Printer* printer) const {
  printer->Print(variables_,
	"inline const $type$& $classname$::$name$() const {\n"
	"  // @@protoc_insertion_point(field_get:$full_name$)\n"
	"  return $name$_;\n"
	"}\n");
}

void MessageFieldGenerator::
GenerateClearingCode(io::Printer* printer) const {
  printer->Print(variables_,
    "if ($name$_ != NULL) $name$_->$type$::Clear();\n");
}

void MessageFieldGenerator::
GenerateMergingCode(io::Printer* printer) const {
  printer->Print(variables_,
    "mutable_$name$()->$type$::MergeFrom(from.$name$());\n");
}

void MessageFieldGenerator::
GenerateSwappingCode(io::Printer* printer) const {
  printer->Print(variables_, "std::swap($name$_, other->$name$_);\n");
}

void MessageFieldGenerator::
GenerateConstructorCode(io::Printer* printer) const {
  printer->Print(variables_, "$name$_ = NULL;\n");
}

void MessageFieldGenerator::
GenerateMergeFromCodedStream(io::Printer* printer) const {
  if (descriptor_->type() == FieldDescriptor::TYPE_MESSAGE) {
    printer->Print(variables_,
      "DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(\n"
      "     input, mutable_$name$()));\n");
  } else {
    printer->Print(variables_,
      "DO_(::google::protobuf::internal::WireFormatLite::ReadGroupNoVirtual(\n"
      "      $number$, input, mutable_$name$()));\n");
  }
}

void MessageFieldGenerator::
GenerateSerializeWithCachedSizes(io::Printer* printer) const {
  printer->Print(variables_,
    "::google::protobuf::internal::WireFormatLite::Write$stream_writer$(\n"
    "  $number$, this->$name$(), output);\n");
}

void MessageFieldGenerator::
GenerateSerializeWithCachedSizesToArray(io::Printer* printer) const {
  printer->Print(variables_,
    "target = ::google::protobuf::internal::WireFormatLite::\n"
    "  Write$declared_type$NoVirtualToArray(\n"
    "    $number$, this->$name$(), target);\n");
}

void MessageFieldGenerator::
GenerateByteSize(io::Printer* printer) const {
  printer->Print(variables_,
    "total_size += $tag_size$ +\n"
    "  ::google::protobuf::internal::WireFormatLite::$declared_type$SizeNoVirtual(\n"
    "    this->$name$());\n");
}

// ===================================================================

MessageOneofFieldGenerator::
MessageOneofFieldGenerator(const FieldDescriptor* descriptor,
                           const Options& options)
  : MessageFieldGenerator(descriptor, options) {
  SetCommonOneofFieldVariables(descriptor, &variables_);
}

MessageOneofFieldGenerator::~MessageOneofFieldGenerator() {}

void MessageOneofFieldGenerator::
GenerateInlineAccessorDefinitions(io::Printer* printer) const {
  printer->Print(variables_,
    "inline const $type$& $classname$::$name$() const {\n"
    "  return has_$name$() ? *$oneof_prefix$$name$_\n"
    "                      : $type$::default_instance();\n"
    "}\n");
}

void MessageOneofFieldGenerator::
GenerateClearingCode(io::Printer* printer) const {
  // if it is the active field, it cannot be NULL.
  printer->Print(variables_,
    "delete $oneof_prefix$$name$_;\n");
}

void MessageOneofFieldGenerator::
GenerateSwappingCode(io::Printer* printer) const {
  // Don't print any swapping code. Swapping the union will swap this field.
}

void MessageOneofFieldGenerator::
GenerateConstructorCode(io::Printer* printer) const {
  // Don't print any constructor code. The field is in a union. We allocate
  // space only when this field is used.
}

// ===================================================================

RepeatedMessageFieldGenerator::
RepeatedMessageFieldGenerator(const FieldDescriptor* descriptor,
                              const Options& options)
  : descriptor_(descriptor) {
  SetMessageVariables(descriptor, &variables_, options);
}

RepeatedMessageFieldGenerator::~RepeatedMessageFieldGenerator() {}

void RepeatedMessageFieldGenerator::
GeneratePrivateMembers(io::Printer* printer) const {
  printer->Print(variables_,
    "std::vector< $type$ > $name$_;\n");
}

void RepeatedMessageFieldGenerator::
GenerateAccessorDeclarations(io::Printer* printer) const {
  printer->Print(variables_,
    "inline const $type$& $name$(int index) const$deprecation$;\n"
);
  printer->Print(variables_,
	"inline const std::vector< $type$ >& $name$() const$deprecation$;\n");
}

void RepeatedMessageFieldGenerator::
GenerateInlineAccessorDefinitions(io::Printer* printer) const {
  printer->Print(variables_,
	"inline const $type$& $classname$::$name$(int index) const {\n"
	"  // @@protoc_insertion_point(field_get:$full_name$)\n"
	"  return $name$_[index];\n"
	"}\n");
  printer->Print(variables_,
	"inline const std::vector< $type$ >& $classname$::$name$() const {\n"
	"  // @@protoc_insertion_point(field_list:$full_name$)\n"
	"  return $name$_;\n"
	"}\n");
}

void RepeatedMessageFieldGenerator::
GenerateClearingCode(io::Printer* printer) const {
  printer->Print(variables_, "$name$_.Clear();\n");
}

void RepeatedMessageFieldGenerator::
GenerateMergingCode(io::Printer* printer) const {
  printer->Print(variables_, "$name$_.MergeFrom(from.$name$_);\n");
}

void RepeatedMessageFieldGenerator::
GenerateSwappingCode(io::Printer* printer) const {
  printer->Print(variables_, "$name$_.Swap(&other->$name$_);\n");
}

void RepeatedMessageFieldGenerator::
GenerateConstructorCode(io::Printer* printer) const {
  // Not needed for repeated fields.
}

void RepeatedMessageFieldGenerator::
GenerateMergeFromCodedStream(io::Printer* printer) const {
  if (descriptor_->type() == FieldDescriptor::TYPE_MESSAGE) {
    printer->Print(variables_,
      "DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(\n"
      "      input, add_$name$()));\n");
  } else {
    printer->Print(variables_,
      "DO_(::google::protobuf::internal::WireFormatLite::ReadGroupNoVirtual(\n"
      "      $number$, input, add_$name$()));\n");
  }
}

void RepeatedMessageFieldGenerator::
GenerateSerializeWithCachedSizes(io::Printer* printer) const {
  printer->Print(variables_,
    "for (int i = 0; i < this->$name$_size(); i++) {\n"
    "  ::google::protobuf::internal::WireFormatLite::Write$stream_writer$(\n"
    "    $number$, this->$name$(i), output);\n"
    "}\n");
}

void RepeatedMessageFieldGenerator::
GenerateSerializeWithCachedSizesToArray(io::Printer* printer) const {
  printer->Print(variables_,
    "for (int i = 0; i < this->$name$_size(); i++) {\n"
    "  target = ::google::protobuf::internal::WireFormatLite::\n"
    "    Write$declared_type$NoVirtualToArray(\n"
    "      $number$, this->$name$(i), target);\n"
    "}\n");
}

void RepeatedMessageFieldGenerator::
GenerateByteSize(io::Printer* printer) const {
  printer->Print(variables_,
    "total_size += $tag_size$ * this->$name$_size();\n"
    "for (int i = 0; i < this->$name$_size(); i++) {\n"
    "  total_size +=\n"
    "    ::google::protobuf::internal::WireFormatLite::$declared_type$SizeNoVirtual(\n"
    "      this->$name$(i));\n"
    "}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
