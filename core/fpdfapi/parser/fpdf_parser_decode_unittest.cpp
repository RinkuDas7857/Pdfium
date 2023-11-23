// Copyright 2015 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/fpdf_parser_decode.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_indirect_object_holder.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/fx_memory_wrappers.h"
#include "core/fxcrt/string_view_template.h"
#include "core/fxcrt/widestring.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"
#include "third_party/base/containers/span.h"

namespace {

// Converts a string literal into a `uint8_t` span.
template <size_t N>
pdfium::span<const uint8_t> ToSpan(const char (&array)[N]) {
  return pdfium::as_bytes(ByteStringView(array, N - 1).span());
}

// Converts a string literal into a `ByteString`.
template <size_t N>
ByteString ToByteString(const char (&array)[N]) {
  return ByteString(array, N - 1);
}

}  // namespace

TEST(ParserDecodeTest, ValidateDecoderPipeline) {
  {
    // Empty decoder list is always valid.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    EXPECT_TRUE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // 1 decoder is almost always valid.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("FlateEncode");
    EXPECT_TRUE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // 1 decoder is almost always valid, even with an unknown decoder.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("FooBar");
    EXPECT_TRUE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Valid 2 decoder pipeline.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("AHx");
    decoders->AppendNew<CPDF_Name>("LZWDecode");
    EXPECT_TRUE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Valid 2 decoder pipeline.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("ASCII85Decode");
    decoders->AppendNew<CPDF_Name>("ASCII85Decode");
    EXPECT_TRUE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Valid 5 decoder pipeline.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("ASCII85Decode");
    decoders->AppendNew<CPDF_Name>("A85");
    decoders->AppendNew<CPDF_Name>("RunLengthDecode");
    decoders->AppendNew<CPDF_Name>("FlateDecode");
    decoders->AppendNew<CPDF_Name>("RL");
    EXPECT_TRUE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Valid 5 decoder pipeline, with an image decoder at the end.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("RunLengthDecode");
    decoders->AppendNew<CPDF_Name>("ASCII85Decode");
    decoders->AppendNew<CPDF_Name>("FlateDecode");
    decoders->AppendNew<CPDF_Name>("LZW");
    decoders->AppendNew<CPDF_Name>("DCTDecode");
    EXPECT_TRUE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Invalid 1 decoder pipeline due to wrong type.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_String>("FlateEncode", false);
    EXPECT_FALSE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Invalid 2 decoder pipeline, with 2 image decoders.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("DCTDecode");
    decoders->AppendNew<CPDF_Name>("CCITTFaxDecode");
    EXPECT_FALSE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Invalid 2 decoder pipeline, with 1 image decoder at the start.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("DCTDecode");
    decoders->AppendNew<CPDF_Name>("FlateDecode");
    EXPECT_FALSE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Invalid 2 decoder pipeline due to wrong type.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_String>("AHx", false);
    decoders->AppendNew<CPDF_Name>("LZWDecode");
    EXPECT_FALSE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Invalid 5 decoder pipeline.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("FlateDecode");
    decoders->AppendNew<CPDF_Name>("FlateDecode");
    decoders->AppendNew<CPDF_Name>("DCTDecode");
    decoders->AppendNew<CPDF_Name>("FlateDecode");
    decoders->AppendNew<CPDF_Name>("FlateDecode");
    EXPECT_FALSE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Invalid 5 decoder pipeline due to wrong type.
    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("ASCII85Decode");
    decoders->AppendNew<CPDF_Name>("A85");
    decoders->AppendNew<CPDF_Name>("RunLengthDecode");
    decoders->AppendNew<CPDF_Name>("FlateDecode");
    decoders->AppendNew<CPDF_String>("RL", false);
    EXPECT_FALSE(ValidateDecoderPipeline(decoders.Get()));
  }
}

TEST(ParserDecodeTest, ValidateDecoderPipelineWithIndirectObjects) {
  {
    // Valid 2 decoder pipeline with indirect objects.
    CPDF_IndirectObjectHolder objects_holder;
    auto decoder = pdfium::MakeRetain<CPDF_Name>(nullptr, "FlateDecode");
    uint32_t decoder_number =
        objects_holder.AddIndirectObject(std::move(decoder));

    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Reference>(&objects_holder, decoder_number);
    decoders->AppendNew<CPDF_Name>("LZW");
    EXPECT_TRUE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Valid 5 decoder pipeline with indirect objects, with an image decoder at
    // the end.
    CPDF_IndirectObjectHolder objects_holder;
    auto decoder = pdfium::MakeRetain<CPDF_Name>(nullptr, "LZW");
    uint32_t decoder_number =
        objects_holder.AddIndirectObject(std::move(decoder));

    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Name>("RunLengthDecode");
    decoders->AppendNew<CPDF_Name>("ASCII85Decode");
    decoders->AppendNew<CPDF_Name>("FlateDecode");
    decoders->AppendNew<CPDF_Reference>(&objects_holder, decoder_number);
    decoders->AppendNew<CPDF_Name>("DCTDecode");
    EXPECT_TRUE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Invalid 2 decoder pipeline due to wrong type indirect object.
    CPDF_IndirectObjectHolder objects_holder;
    auto decoder =
        pdfium::MakeRetain<CPDF_String>(nullptr, "FlateDecode", false);
    uint32_t decoder_number =
        objects_holder.AddIndirectObject(std::move(decoder));

    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Reference>(&objects_holder, decoder_number);
    decoders->AppendNew<CPDF_Name>("LZW");
    EXPECT_FALSE(ValidateDecoderPipeline(decoders.Get()));
  }
  {
    // Invalid 2 decoder pipeline due to invalid indirect object.
    CPDF_IndirectObjectHolder objects_holder;
    auto decoder = pdfium::MakeRetain<CPDF_Name>(nullptr, "DCTDecode");
    uint32_t decoder_number =
        objects_holder.AddIndirectObject(std::move(decoder));

    auto decoders = pdfium::MakeRetain<CPDF_Array>();
    decoders->AppendNew<CPDF_Reference>(&objects_holder, decoder_number);
    decoders->AppendNew<CPDF_Name>("LZW");
    EXPECT_FALSE(ValidateDecoderPipeline(decoders.Get()));
  }
}

// TODO(thestig): Test decoder params.
TEST(ParserDecodeTest, GetDecoderArray) {
  {
    // Treat no filter as an empty filter array.
    auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
    absl::optional<DecoderArray> decoder_array = GetDecoderArray(dict);
    ASSERT_TRUE(decoder_array.has_value());
    EXPECT_TRUE(decoder_array.value().empty());
  }
  {
    // Wrong filter type.
    auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
    dict->SetNewFor<CPDF_String>("Filter", "RL", false);
    absl::optional<DecoderArray> decoder_array = GetDecoderArray(dict);
    EXPECT_FALSE(decoder_array.has_value());
  }
  {
    // Filter name.
    auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
    dict->SetNewFor<CPDF_Name>("Filter", "RL");
    absl::optional<DecoderArray> decoder_array = GetDecoderArray(dict);
    ASSERT_TRUE(decoder_array.has_value());
    ASSERT_EQ(1u, decoder_array.value().size());
    EXPECT_EQ("RL", decoder_array.value()[0].first);
  }
  {
    // Empty filter array.
    auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
    dict->SetNewFor<CPDF_Array>("Filter");
    absl::optional<DecoderArray> decoder_array = GetDecoderArray(dict);
    ASSERT_TRUE(decoder_array.has_value());
    EXPECT_TRUE(decoder_array.value().empty());
  }
  {
    // Valid 1 element filter array.
    auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
    auto filter_array = dict->SetNewFor<CPDF_Array>("Filter");
    filter_array->AppendNew<CPDF_Name>("FooBar");
    absl::optional<DecoderArray> decoder_array = GetDecoderArray(dict);
    ASSERT_TRUE(decoder_array.has_value());
    ASSERT_EQ(1u, decoder_array.value().size());
    EXPECT_EQ("FooBar", decoder_array.value()[0].first);
  }
  {
    // Valid 2 element filter array.
    auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
    auto filter_array = dict->SetNewFor<CPDF_Array>("Filter");
    filter_array->AppendNew<CPDF_Name>("AHx");
    filter_array->AppendNew<CPDF_Name>("LZWDecode");
    absl::optional<DecoderArray> decoder_array = GetDecoderArray(dict);
    ASSERT_TRUE(decoder_array.has_value());
    ASSERT_EQ(2u, decoder_array.value().size());
    EXPECT_EQ("AHx", decoder_array.value()[0].first);
    EXPECT_EQ("LZWDecode", decoder_array.value()[1].first);
  }
  {
    // Invalid 2 element filter array.
    auto dict = pdfium::MakeRetain<CPDF_Dictionary>();
    auto invalid_filter_array = dict->SetNewFor<CPDF_Array>("Filter");
    invalid_filter_array->AppendNew<CPDF_Name>("DCTDecode");
    invalid_filter_array->AppendNew<CPDF_Name>("CCITTFaxDecode");
    absl::optional<DecoderArray> decoder_array = GetDecoderArray(dict);
    EXPECT_FALSE(decoder_array.has_value());
  }
}

TEST(ParserDecodeTest, A85Decode) {
  const pdfium::DecodeTestData kTestData[] = {
      // Empty src string.
      STR_IN_OUT_CASE("", "", 0),
      // Empty content in src string.
      STR_IN_OUT_CASE("~>", "", 0),
      // Regular conversion.
      STR_IN_OUT_CASE("FCfN8~>", "test", 7),
      // End at the ending mark.
      STR_IN_OUT_CASE("FCfN8~>FCfN8", "test", 7),
      // Skip whitespaces.
      STR_IN_OUT_CASE("\t F C\r\n \tf N 8 ~>", "test", 17),
      // No ending mark.
      STR_IN_OUT_CASE("@3B0)DJj_BF*)>@Gp#-s", "a funny story :)", 20),
      // Non-multiple length.
      STR_IN_OUT_CASE("12A", "2k", 3),
      // Stop at unknown characters.
      STR_IN_OUT_CASE("FCfN8FCfN8vw", "testtest", 11),
  };
  for (const auto& test_case : kTestData) {
    std::unique_ptr<uint8_t, FxFreeDeleter> result;
    uint32_t result_size = 0;
    EXPECT_EQ(test_case.processed_size,
              A85Decode({test_case.input, test_case.input_size}, &result,
                        &result_size))
        << "for case " << test_case.input;
    ASSERT_EQ(test_case.expected_size, result_size);
    const uint8_t* result_ptr = result.get();
    for (size_t j = 0; j < result_size; ++j) {
      EXPECT_EQ(test_case.expected[j], result_ptr[j])
          << "for case " << test_case.input << " char " << j;
    }
  }
}

// NOTE: python's zlib.compress() and zlib.decompress() may be useful for
// external validation of the FlateDncode/FlateEecode test cases.
TEST(FPDFParserDecodeEmbedderTest, FlateDecode) {
  static const pdfium::DecodeTestData flate_decode_cases[] = {
      STR_IN_OUT_CASE("", "", 0),
      STR_IN_OUT_CASE("preposterous nonsense", "", 2),
      STR_IN_OUT_CASE("\x78\x9c\x03\x00\x00\x00\x00\x01", "", 8),
      STR_IN_OUT_CASE("\x78\x9c\x53\x00\x00\x00\x21\x00\x21", " ", 9),
      STR_IN_OUT_CASE("\x78\x9c\x33\x34\x32\x06\x00\01\x2d\x00\x97", "123", 11),
      STR_IN_OUT_CASE("\x78\x9c\x63\xf8\x0f\x00\x01\x01\x01\x00", "\x00\xff",
                      10),
      STR_IN_OUT_CASE(
          "\x78\x9c\x33\x54\x30\x00\x42\x5d\x43\x05\x23\x4b\x05\x73\x33\x63"
          "\x85\xe4\x5c\x2e\x90\x80\xa9\xa9\xa9\x82\xb9\xb1\xa9\x42\x51\x2a"
          "\x57\xb8\x42\x1e\x57\x21\x92\xa0\x89\x9e\xb1\xa5\x09\x92\x84\x9e"
          "\x85\x81\x81\x25\xd8\x14\x24\x26\xd0\x18\x43\x05\x10\x0c\x72\x57"
          "\x80\x30\x8a\xd2\xb9\xf4\xdd\x0d\x14\xd2\x8b\xc1\x46\x99\x59\x1a"
          "\x2b\x58\x1a\x9a\x83\x8c\x49\xe3\x0a\x04\x42\x00\x37\x4c\x1b\x42",
          "1 0 0 -1 29 763 cm\n0 0 555 735 re\nW n\nq\n0 0 555 734.394 re\n"
          "W n\nq\n0.8009 0 0 0.8009 0 0 cm\n1 1 1 RG 1 1 1 rg\n/G0 gs\n"
          "0 0 693 917 re\nf\nQ\nQ\n",
          96),
  };

  for (size_t i = 0; i < std::size(flate_decode_cases); ++i) {
    const pdfium::DecodeTestData& data = flate_decode_cases[i];
    std::unique_ptr<uint8_t, FxFreeDeleter> buf;
    uint32_t buf_size;
    EXPECT_EQ(data.processed_size,
              FlateDecode({data.input, data.input_size}, &buf, &buf_size))
        << " for case " << i;
    ASSERT_TRUE(buf);
    EXPECT_EQ(data.expected_size, buf_size) << " for case " << i;
    if (data.expected_size != buf_size)
      continue;
    EXPECT_EQ(0, memcmp(data.expected, buf.get(), data.expected_size))
        << " for case " << i;
  }
}

TEST(ParserDecodeTest, FlateEncode) {
  static const pdfium::StrFuncTestData flate_encode_cases[] = {
      STR_IN_OUT_CASE("", "\x78\x9c\x03\x00\x00\x00\x00\x01"),
      STR_IN_OUT_CASE(" ", "\x78\x9c\x53\x00\x00\x00\x21\x00\x21"),
      STR_IN_OUT_CASE("123", "\x78\x9c\x33\x34\x32\x06\x00\01\x2d\x00\x97"),
      STR_IN_OUT_CASE("\x00\xff", "\x78\x9c\x63\xf8\x0f\x00\x01\x01\x01\x00"),
      STR_IN_OUT_CASE(
          "1 0 0 -1 29 763 cm\n0 0 555 735 re\nW n\nq\n0 0 555 734.394 re\n"
          "W n\nq\n0.8009 0 0 0.8009 0 0 cm\n1 1 1 RG 1 1 1 rg\n/G0 gs\n"
          "0 0 693 917 re\nf\nQ\nQ\n",
          "\x78\x9c\x4d\x8c\x3b\x0e\x80\x20\x10\x05\xfb\x3d\xc5\xbb\x80\xb8"
          "\xc8\xcf\x3d\x01\x35\x36\x1e\x80\x28\x15\x26\xe2\xfd\x13\x83\x36"
          "\xe4\x35\x93\x79\xc9\x68\x30\x18\x93\xc6\x22\x08\xde\x20\x57\xea"
          "\xc2\x39\x87\x60\x1c\xda\x41\x3b\x2e\xba\x07\x69\x95\x11\x3b\x1c"
          "\x6a\x65\x96\xaf\x32\x60\xae\xa4\xd1\xb7\x45\xfc\xd0\x0a\xcd\x91"
          "\x51\x9e\x2f\xe5\xc5\x40\x74\xe8\x99\x93\x12\x25\x7a\x01\x37\x4c"
          "\x1b\x42"),
  };

  for (size_t i = 0; i < std::size(flate_encode_cases); ++i) {
    const pdfium::StrFuncTestData& data = flate_encode_cases[i];
    DataVector<uint8_t> result = FlateEncode({data.input, data.input_size});
    EXPECT_EQ(data.expected_size, result.size()) << " for case " << i;
    if (data.expected_size != result.size())
      continue;
    EXPECT_EQ(0, memcmp(data.expected, result.data(), data.expected_size))
        << " for case " << i;
  }
}

TEST(ParserDecodeTest, HexDecode) {
  const pdfium::DecodeTestData kTestData[] = {
      // Empty src string.
      STR_IN_OUT_CASE("", "", 0),
      // Empty content in src string.
      STR_IN_OUT_CASE(">", "", 1),
      // Only whitespaces in src string.
      STR_IN_OUT_CASE("\t   \r\n>", "", 7),
      // Regular conversion.
      STR_IN_OUT_CASE("12Ac>zzz", "\x12\xac", 5),
      // Skip whitespaces.
      STR_IN_OUT_CASE("12 Ac\t02\r\nBF>zzz>", "\x12\xac\x02\xbf", 13),
      // Non-multiple length.
      STR_IN_OUT_CASE("12A>zzz", "\x12\xa0", 4),
      // Skips unknown characters.
      STR_IN_OUT_CASE("12tk  \tAc>zzz", "\x12\xac", 10),
      // No ending mark.
      STR_IN_OUT_CASE("12AcED3c3456", "\x12\xac\xed\x3c\x34\x56", 12),
  };
  for (const auto& test_case : kTestData) {
    std::unique_ptr<uint8_t, FxFreeDeleter> result;
    uint32_t result_size = 0;
    EXPECT_EQ(test_case.processed_size,
              HexDecode({test_case.input, test_case.input_size}, &result,
                        &result_size))
        << "for case " << test_case.input;
    ASSERT_EQ(test_case.expected_size, result_size);
    const uint8_t* result_ptr = result.get();
    for (size_t j = 0; j < result_size; ++j) {
      EXPECT_EQ(test_case.expected[j], result_ptr[j])
          << "for case " << test_case.input << " char " << j;
    }
  }
}

TEST(ParserDecodeTest, DecodeText) {
  // Empty src string.
  EXPECT_EQ(L"", PDF_DecodeText(ToSpan("")));

  // ASCII text.
  EXPECT_EQ(L"the quick\tfox", PDF_DecodeText(ToSpan("the quick\tfox")));

  // UTF-8 text.
  EXPECT_EQ(L"\x0330\x0331",
            PDF_DecodeText(ToSpan("\xEF\xBB\xBF\xCC\xB0\xCC\xB1")));

  // UTF-16BE text.
  EXPECT_EQ(L"\x0330\x0331",
            PDF_DecodeText(ToSpan("\xFE\xFF\x03\x30\x03\x31")));

  // More UTF-16BE text.
  EXPECT_EQ(
      L"\x7F51\x9875\x0020\x56FE\x7247\x0020"
      L"\x8D44\x8BAF\x66F4\x591A\x0020\x00BB",
      PDF_DecodeText(
          ToSpan("\xFE\xFF\x7F\x51\x98\x75\x00\x20\x56\xFE\x72\x47\x00"
                 "\x20\x8D\x44\x8B\xAF\x66\xF4\x59\x1A\x00\x20\x00\xBB")));

  // Supplementary UTF-8 text.
  EXPECT_EQ(L"🎨", PDF_DecodeText(ToSpan("\xEF\xBB\xBF\xF0\x9F\x8E\xA8")));

  // Supplementary UTF-16BE text.
  EXPECT_EQ(L"🎨", PDF_DecodeText(ToSpan("\xFE\xFF\xD8\x3C\xDF\xA8")));
}

// https://crbug.com/pdfium/182
TEST(ParserDecodeTest, DecodeTextWithUnicodeEscapes) {
  EXPECT_EQ(L"\x0020\x5370\x5237",
            PDF_DecodeText(ToSpan(
                "\xEF\xBB\xBF\x1B\x6A\x61\x1B\x20\xE5\x8D\xB0\xE5\x88\xB7")));
  EXPECT_EQ(L"\x0020\x5370\x5237",
            PDF_DecodeText(ToSpan(
                "\xFE\xFF\x00\x1B\x6A\x61\x00\x1B\x00\x20\x53\x70\x52\x37")));
  EXPECT_EQ(
      L"\x0020\x5370\x5237",
      PDF_DecodeText(ToSpan(
          "\xFE\xFF\x00\x1B\x6A\x61\x00\x1B\x00\x20\x53\x70\x52\x37\x29")));
  EXPECT_EQ(
      L"\x0020\x5370\x5237",
      PDF_DecodeText(ToSpan(
          "\xFE\xFF\x00\x1B\x6A\x61\x4A\x50\x00\x1B\x00\x20\x53\x70\x52\x37")));
  EXPECT_EQ(L"\x0020\x5237",
            PDF_DecodeText(ToSpan(
                "\xFE\xFF\x00\x20\x00\x1B\x6A\x61\x4A\x50\x00\x1B\x52\x37")));
}

// https://crbug.com/1001159
TEST(ParserDecodeTest, DecodeTextWithInvalidUnicodeEscapes) {
  EXPECT_EQ(L"", PDF_DecodeText(ToSpan("\xEF\xBB\xBF\x1B\x1B")));
  EXPECT_EQ(L"", PDF_DecodeText(ToSpan("\xFE\xFF\x00\x1B\x00\x1B")));
  EXPECT_EQ(L"", PDF_DecodeText(ToSpan("\xFE\xFF\x00\x1B\x00\x1B\x20")));
  EXPECT_EQ(L"\x0020", PDF_DecodeText(ToSpan("\xEF\xBB\xBF\x1B\x1B\x20")));
  EXPECT_EQ(L"\x0020",
            PDF_DecodeText(ToSpan("\xFE\xFF\x00\x1B\x00\x1B\x00\x20")));
}

TEST(ParserDecodeTest, DecodeTextWithUnpairedSurrogates) {
  EXPECT_EQ(L"\xD800", PDF_DecodeText(ToSpan("\xFE\xFF\xD8\x00"))) << "High";
  EXPECT_EQ(L"\xDC00", PDF_DecodeText(ToSpan("\xFE\xFF\xDC\x00"))) << "Low";
  EXPECT_EQ(L"\xD800🎨",
            PDF_DecodeText(ToSpan("\xFE\xFF\xD8\x00\xD8\x3C\xDF\xA8")))
      << "High-high";
  EXPECT_EQ(L"🎨\xDC00",
            PDF_DecodeText(ToSpan("\xFE\xFF\xD8\x3C\xDF\xA8\xDC\x00")))
      << "Low-low";
}

TEST(ParserDecodeTest, EncodeText) {
  // Empty src string.
  EXPECT_EQ("", PDF_EncodeText(L""));

  // ASCII text.
  EXPECT_EQ("the quick\tfox", PDF_EncodeText(L"the quick\tfox"));

  // Unicode text.
  EXPECT_EQ("\xFE\xFF\x03\x30\x03\x31", PDF_EncodeText(L"\x0330\x0331"));

  // More Unicode text.
  EXPECT_EQ(
      ToByteString("\xFE\xFF\x7F\x51\x98\x75\x00\x20\x56\xFE\x72\x47\x00"
                   "\x20\x8D\x44\x8B\xAF\x66\xF4\x59\x1A\x00\x20\x00\xBB"),
      PDF_EncodeText(L"\x7F51\x9875\x0020\x56FE\x7247\x0020"
                     L"\x8D44\x8BAF\x66F4\x591A\x0020\x00BB"));

  // Supplementary Unicode text.
  EXPECT_EQ("\xFE\xFF\xD8\x3C\xDF\xA8", PDF_EncodeText(L"🎨"));
}

TEST(ParserDecodeTest, RoundTripText) {
  for (int pdf_code_point = 0; pdf_code_point < 256; ++pdf_code_point) {
    ByteString original(static_cast<char>(pdf_code_point));
    ByteString reencoded =
        PDF_EncodeText(PDF_DecodeText(original.raw_span()).AsStringView());

    switch (pdf_code_point) {
      case 0x7F:
      case 0x9F:
      case 0xAD:
        EXPECT_EQ(ByteString('\0'), reencoded) << "PDFDocEncoding undefined";
        break;

      default:
        EXPECT_EQ(original, reencoded) << "PDFDocEncoding: " << pdf_code_point;
        break;
    }
  }
}
