#pragma once
#include <catch2/catch_test_macros.hpp>

#define E2E_TEST(batch, name) TEST_CASE(name, "[" #batch "]")
