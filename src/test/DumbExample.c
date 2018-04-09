
#include "DumbExample.h"

int8_t AverageThreeBytes(int8_t a, int8_t b, int8_t c)
{
	return (int8_t)(((int16_t)a + (int16_t)b + (int16_t)c) / 3);
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

