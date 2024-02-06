// Copyright 2021 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/span_util.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

TEST(Spanset, Fits) {
  std::vector<char> dst(4, 'B');
  fxcrt::spanset(pdfium::make_span(dst).first(2), 'A');
  EXPECT_EQ(dst[0], 'A');
  EXPECT_EQ(dst[1], 'A');
  EXPECT_EQ(dst[2], 'B');
  EXPECT_EQ(dst[3], 'B');
}

TEST(Spanset, Empty) {
  std::vector<char> dst(4, 'B');
  fxcrt::spanset(pdfium::make_span(dst).subspan(4), 'A');
  EXPECT_EQ(dst[0], 'B');
  EXPECT_EQ(dst[1], 'B');
  EXPECT_EQ(dst[2], 'B');
  EXPECT_EQ(dst[3], 'B');
}

TEST(Spancpy, FitsEntirely) {
  std::vector<char> src(4, 'A');
  std::vector<char> dst(4, 'B');
  auto remain = fxcrt::spancpy(pdfium::make_span(dst), pdfium::make_span(src));
  EXPECT_EQ(dst[0], 'A');
  EXPECT_EQ(dst[1], 'A');
  EXPECT_EQ(dst[2], 'A');
  EXPECT_EQ(dst[3], 'A');
  EXPECT_TRUE(remain.empty());
}

TEST(Spancpy, FitsWithin) {
  std::vector<char> src(2, 'A');
  std::vector<char> dst(4, 'B');
  // Also show that a const src argument is acceptable.
  auto remain = fxcrt::spancpy(pdfium::make_span(dst).subspan(1),
                               pdfium::span<const char>(src));
  EXPECT_EQ(dst[0], 'B');
  EXPECT_EQ(dst[1], 'A');
  EXPECT_EQ(dst[2], 'A');
  EXPECT_EQ(dst[3], 'B');
  EXPECT_EQ(remain.size(), 1u);
  EXPECT_EQ(remain.data(), &dst[3]);
}

TEST(Spancpy, EmptyCopyWithin) {
  std::vector<char> src(2, 'A');
  std::vector<char> dst(4, 'B');
  auto remain = fxcrt::spancpy(pdfium::make_span(dst).subspan(1),
                               pdfium::make_span(src).subspan(2));
  EXPECT_EQ(dst[0], 'B');
  EXPECT_EQ(dst[1], 'B');
  EXPECT_EQ(dst[2], 'B');
  EXPECT_EQ(dst[3], 'B');
  EXPECT_EQ(remain.size(), 3u);
  EXPECT_EQ(remain.data(), &dst[1]);
}

TEST(Spancpy, EmptyCopyToEmpty) {
  std::vector<char> src(2, 'A');
  std::vector<char> dst(4, 'B');
  auto remain = fxcrt::spancpy(pdfium::make_span(dst).subspan(4),
                               pdfium::make_span(src).subspan(2));
  EXPECT_EQ(dst[0], 'B');
  EXPECT_EQ(dst[1], 'B');
  EXPECT_EQ(dst[2], 'B');
  EXPECT_EQ(dst[3], 'B');
  EXPECT_TRUE(remain.empty());
}

TEST(Spanmove, FitsWithin) {
  std::vector<char> src(2, 'A');
  std::vector<char> dst(4, 'B');
  // Also show that a const src argument is acceptable.
  auto remain = fxcrt::spanmove(pdfium::make_span(dst).subspan(1),
                                pdfium::span<const char>(src));
  EXPECT_EQ(dst[0], 'B');
  EXPECT_EQ(dst[1], 'A');
  EXPECT_EQ(dst[2], 'A');
  EXPECT_EQ(dst[3], 'B');
  EXPECT_EQ(remain.size(), 1u);
  EXPECT_EQ(remain.data(), &dst[3]);
}

TEST(SpanEquals, Empty) {
  std::vector<int> vec = {1, 2};
  std::vector<int> vec2 = {3, 4};
  pdfium::span<int> empty;
  pdfium::span<int> some = pdfium::make_span(vec);
  pdfium::span<int> some2 = pdfium::make_span(vec2);
  EXPECT_FALSE(fxcrt::span_equals(empty, some));
  EXPECT_FALSE(fxcrt::span_equals(some, empty));
  EXPECT_TRUE(fxcrt::span_equals(empty, empty));
  EXPECT_TRUE(fxcrt::span_equals(empty, some.first(0)));
  EXPECT_TRUE(fxcrt::span_equals(some.first(0), empty));
  EXPECT_TRUE(fxcrt::span_equals(some2.first(0), some.first(0)));
  EXPECT_TRUE(fxcrt::span_equals(some.first(0), some2.first(0)));
}

TEST(SpanEquals, NonEmpty) {
  std::vector<int> vec = {1, 2, 3};
  std::vector<int> vec2 = {1, 2, 4};
  pdfium::span<int> some = pdfium::make_span(vec);
  pdfium::span<int> some2 = pdfium::make_span(vec2);
  EXPECT_FALSE(fxcrt::span_equals(some, some2));
  EXPECT_FALSE(fxcrt::span_equals(some.first(2), some2));
  EXPECT_FALSE(fxcrt::span_equals(some, some2.first(2)));
  EXPECT_TRUE(fxcrt::span_equals(some.first(2), some2.first(2)));
}

TEST(Span, AssignOverOnePastEnd) {
  std::vector<char> src(2, 'A');
  pdfium::span<char> span = pdfium::make_span(src);
  span = span.subspan(2);
  span = pdfium::make_span(src);
  EXPECT_EQ(span.size(), 2u);
}

TEST(ReinterpretSpan, Empty) {
  pdfium::span<uint8_t> empty;
  pdfium::span<uint32_t> converted = fxcrt::reinterpret_span<uint32_t>(empty);
  EXPECT_EQ(converted.data(), nullptr);
  EXPECT_EQ(converted.size(), 0u);
}

TEST(ReinterpretSpan, LegalConversions) {
  uint8_t aaaabbbb[8] = {0x61, 0x61, 0x61, 0x61, 0x62, 0x62, 0x62, 0x62};
  pdfium::span<uint8_t> original = pdfium::make_span(aaaabbbb);
  pdfium::span<uint32_t> converted =
      fxcrt::reinterpret_span<uint32_t>(original);
  ASSERT_NE(converted.data(), nullptr);
  ASSERT_EQ(converted.size(), 2u);
  EXPECT_EQ(converted[0], 0x61616161u);
  EXPECT_EQ(converted[1], 0x62626262u);
}

TEST(ReinterpretSpan, BadLength) {
  uint8_t ab[2] = {0x61, 0x62};
  EXPECT_DEATH(fxcrt::reinterpret_span<uint32_t>(pdfium::make_span(ab)), "");
}

TEST(ReinterpretSpan, BadAlignment) {
  uint8_t abcabc[6] = {0x61, 0x62, 0x63, 0x61, 0x62, 0x63};
  EXPECT_DEATH(fxcrt::reinterpret_span<uint32_t>(
                   pdfium::make_span(abcabc).subspan(1, 4)),
               "");
}
