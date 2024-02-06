#include <zephyr/ztest.h>

// #include "GPS.h"

#include <stdint.h>


// Pretend I'm including a master "global test state" header file
struct GlobalTestState
{
    uint8_t val = 0;
};



#define SUITE_NAME TEST_GPS



// Test suite asks this function whether this suite should run given the
// configuration of the global state in which the test is currently
// being run though
static bool SuiteShouldRun(const void *globalState)
{
    GlobalTestState *gs = (GlobalTestState *)globalState;

    // pretend I'm deciding if this suite should run based on global state
    return gs->val == 0 || gs->val != 0;
}


struct SuiteData
{
    uint8_t val = 0;
};

static SuiteData data;

static void *SuiteSetup()
{
    data.val = 5;

    return &data;
}

static void SuiteTeardown(void *p)
{
    // but really, you already know what the data is,
    // you don't have to look through the pointer

    SuiteData *data = (SuiteData *)p;

    data->val = 0;
}

static void SuiteSetupBeforeTest(void *suiteData)
{
    SuiteData *sd = (SuiteData *)suiteData;
}

static void SuiteTeardownAfterTest(void *suiteData)
{
    SuiteData *sd = (SuiteData *)suiteData;
}


ZTEST_SUITE(SUITE_NAME,
            SuiteShouldRun,
            SuiteSetup,
            SuiteTeardown,
            SuiteSetupBeforeTest,
            SuiteTeardownAfterTest);


ZTEST(SUITE_NAME, test_Basic_Test)
{
    zassert_equal(5, data.val);
}





