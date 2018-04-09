
#include "unity.h"

#include "bam_lmdb.h"  
#include "bam_api.h"

#include "htslib/bgzf.h"
#include "htslib/hts.h"



// get_default_dbname(const char *filename)

// TEST_ASSERT_EQUAL_STRING_ARRAY


// filename = 'example.bam'
// dbname = 'example.bam_lmdb

// based on examples on website and github, there are two "unity" styles

// website uses "TestDumbExample.c" form, which we use here

//  TEST_ASSERT_EQUAL_STRING_ARRAY() example from here: https://github.com/rdammkoehler/PrimeFactorsC/blob/master/src/test/prime_factors_tests.c

void test_get_default_dbname(void)
{
    int expected[] = { "example_lmdb" };
    int *actual = get_default_dbname("example.bam");
    TEST_ASSERT_EQUAL_STRING_ARRAY(expected, actual, 1);
    free(actual);
}


int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_get_default_dbname);
	return UNITY_END();
}


