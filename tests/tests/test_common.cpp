#include <random>

#include <gtest/gtest.h>

#include "common/constants.hpp"
#include "common/bit_ops.hpp"

TEST(constants, value_check) {
	ASSERT_FLOAT_EQ(constants::sqrt2_v<float>, 1.414213562373095048801688724209698079f);
	ASSERT_FLOAT_EQ(constants::pi_v<float>, 3.141592653589793238462643383279502884f);
}

TEST(bit_ops, has_single_bit) {
	ASSERT_TRUE(bits::has_single_bit(512));
	ASSERT_FALSE(bits::has_single_bit(200));
	ASSERT_TRUE(bits::has_single_bit(64));
	ASSERT_TRUE(bits::has_single_bit(1));
	ASSERT_FALSE(bits::has_single_bit(0));
}

TEST(bit_ops, bit_ceil) {
	ASSERT_EQ(bits::bit_ceil(512), 512);
	ASSERT_EQ(bits::bit_ceil(200), 256);
	ASSERT_EQ(bits::bit_ceil(1236), 2048);
	ASSERT_EQ(bits::bit_ceil(23), 32);
	ASSERT_EQ(bits::bit_ceil(0), 1);
}

TEST(bit_ops, countr_zero) {
	ASSERT_EQ(bits::countr_zero(512), 9);
	ASSERT_EQ(bits::countr_zero(256), 8);
	ASSERT_EQ(bits::countr_zero(128), 7);
	ASSERT_EQ(bits::countr_zero(64), 6);
	ASSERT_EQ(bits::countr_zero(32), 5);
	ASSERT_EQ(bits::countr_zero(16), 4);
	ASSERT_EQ(bits::countr_zero(8), 3);
	ASSERT_EQ(bits::countr_zero(4), 2);
	ASSERT_EQ(bits::countr_zero(2), 1);
}
