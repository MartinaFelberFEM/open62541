#include <check.h>
#include <assert.h>


#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>

static UA_Server *server = NULL;

/***************************************************************************************************/
static void setup(void) {

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "setup");

    server = UA_Server_new();
    assert(server != 0);
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);
}

/***************************************************************************************************/
static void teardown(void) {

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "teardown");

    UA_Server_delete(server);
}



/***************************************************************************************************/
START_TEST(TranslateBrowsepath) {

    // search for '0:DataTypes' node from types folder


    // this works:
    UA_RelativePathElement rpe;
    UA_RelativePathElement_init(&rpe);
    rpe.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);    // reference Type Id seems to be necessary now
    rpe.targetName = UA_QUALIFIEDNAME(0, "DataTypes");

    UA_BrowsePath bp;
    UA_BrowsePath_init(&bp);
    bp.startingNode = UA_NODEID_NUMERIC(0, UA_NS0ID_TYPESFOLDER);
    bp.relativePath.elementsSize = 1;
    bp.relativePath.elements = &rpe;

    UA_BrowsePathResult bpr =
        UA_Server_translateBrowsePathToNodeIds(server, &bp);
    ck_assert_int_eq(UA_STATUSCODE_GOOD, bpr.statusCode);
    ck_assert_int_eq(1, bpr.targetsSize);


    // this does not work anymore (but worked with release version 1.1.2)
    rpe.referenceTypeId = UA_NODEID_NULL;
    bpr = UA_Server_translateBrowsePathToNodeIds(server, &bp);
    ck_assert_int_eq(UA_STATUSCODE_GOOD, bpr.statusCode);   // return value BADNOMATCH
    ck_assert_int_eq(1, bpr.targetsSize);


} END_TEST


/***************************************************************************************************/
int main(void) {

    TCase *tc = tcase_create("translate_browsepath");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, TranslateBrowsepath);

    Suite *s = suite_create("suite");
    suite_add_tcase(s, tc);

    /* TODO: how to provoke and test an error state? */

    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr,CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}