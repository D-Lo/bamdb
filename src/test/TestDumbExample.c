
#include "unity.h"
#include "DumbExample.h"

void test_AverageThreeBytes_should_AverageMidRangeValues(void)
{
	TEST_ASSERT_EQUAL_HEX8(40, AverageThreeBytes(30, 40, 50));
	TEST_ASSERT_EQUAL_HEX8(40, AverageThreeBytes(10, 70, 40));
	TEST_ASSERT_EQUAL_HEX8(33, AverageThreeBytes(33, 33, 33));
}
 

/// strange test
void test_AverageThreeBytes_should_AverageHighValues(void)
{
	TEST_ASSERT_EQUAL_HEX8(80, AverageThreeBytes(70, 80, 90));
	TEST_ASSERT_EQUAL_HEX8(127, AverageThreeBytes(127, 127, 127));
	TEST_ASSERT_EQUAL_HEX8(84, AverageThreeBytes(0, 126, 126));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_AverageThreeBytes_should_AverageMidRangeValues);
	RUN_TEST(test_AverageThreeBytes_should_AverageMidRangeValues);
	return UNITY_END();
}

/// We will build the tests as follows:
/// gcc TestDumbExample.c DumbExample.c ./unity/src/unity.c -o TestDumbExample
/// ./TestDumbExample

// EVAN NOTE: This didn't work. Move all /unity/src files to 'DumbExample'

// $ gcc TestDumbExample.c DumbExample.c unity.c -o testdumm
// $ ./testdumm
// TestDumbExample.c:24:test_AverageThreeBytes_should_AverageMidRangeValues:PASS
// TestDumbExample.c:25:test_AverageThreeBytes_should_AverageMidRangeValues:PASS
// 
// -----------------------
// 2 Tests 0 Failures 0 Ignored 
// OK

