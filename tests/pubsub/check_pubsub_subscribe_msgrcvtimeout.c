#include <check.h>

#include <open62541/plugin/pubsub_udp.h>
#include <open62541/server_config_default.h>
#include <open62541/server_pubsub.h>

#include "ua_pubsub.h"

UA_Server *server = NULL;


#define PUBLISHING_INTERVAL 5
#define MSG_RCV_TIMEOUT 20.0    // [ms]

static void setup(void) {
    server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);

    config->pubsubTransportLayers = (UA_PubSubTransportLayer*)
        UA_malloc(sizeof(UA_PubSubTransportLayer));
    config->pubsubTransportLayers[0] = UA_PubSubTransportLayerUDPMP();
    config->pubsubTransportLayersSize++;

    UA_Server_run_startup(server);
}

static void teardown(void) {
    UA_Server_run_shutdown(server);
    UA_Server_delete(server);
}

static void PrepareSimpleMsgReceiveTimeoutTest(void) {

    UA_NodeId connectionId1, connectionId2, writerGroupId1, 
        publishedDataSetId1, dataSetWriterId1, publishedDataSetFieldId1,
        readerGroupId1, dataSetReaderId1;

    UA_NodeId PublishedVarId = UA_NODEID_NUMERIC(1, 1);
    UA_NodeId SubscribedVarId = UA_NODEID_NUMERIC(1, 2);


    
    /* Connection 1:
        Configure WriterGroup with DataSetWriter, publish 1 Int32 variable */

    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(UA_PubSubConnectionConfig));
    connectionConfig.name = UA_STRING("UADP Connection 1");
    connectionConfig.enabled = UA_TRUE;
    connectionConfig.publisherIdType = UA_PUBSUB_PUBLISHERID_NUMERIC;
    connectionConfig.publisherId.numeric = 1;
    UA_NetworkAddressUrlDataType networkAddressUrl = {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.transportProfileUri = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    ck_assert(UA_Server_addPubSubConnection(server, &connectionConfig, &connectionId1) == UA_STATUSCODE_GOOD);

    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("WriterGroup 1");
    writerGroupConfig.publishingInterval = PUBLISHING_INTERVAL;
    writerGroupConfig.enabled = UA_FALSE;
    writerGroupConfig.writerGroupId = 1;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;
    writerGroupConfig.messageSettings.encoding             = UA_EXTENSIONOBJECT_DECODED;
    writerGroupConfig.messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE];
    UA_UadpWriterGroupMessageDataType *writerGroupMessage  = UA_UadpWriterGroupMessageDataType_new();
    writerGroupMessage->networkMessageContentMask          = (UA_UadpNetworkMessageContentMask)(UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID |
                                                              (UA_UadpNetworkMessageContentMask)UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER |
                                                              (UA_UadpNetworkMessageContentMask)UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID |
                                                              (UA_UadpNetworkMessageContentMask)UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER);
    writerGroupConfig.messageSettings.content.decoded.data = writerGroupMessage;
    ck_assert(UA_Server_addWriterGroup(server, connectionId1, &writerGroupConfig, &writerGroupId1) == UA_STATUSCODE_GOOD);

    UA_UadpWriterGroupMessageDataType_delete(writerGroupMessage);

    /* set WriterGroup operational */
    ck_assert(UA_Server_setWriterGroupOperational(server, writerGroupId1) == UA_STATUSCODE_GOOD);


    UA_PublishedDataSetConfig pdsConfig;
    memset(&pdsConfig, 0, sizeof(UA_PublishedDataSetConfig));
    pdsConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    pdsConfig.name = UA_STRING("PublishedDataSet 1");
    UA_AddPublishedDataSetResult result = UA_Server_addPublishedDataSet(server, &pdsConfig, &publishedDataSetId1);
    ck_assert(result.addResult == UA_STATUSCODE_GOOD);

    /* Create variable to publish integer data */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.description           = UA_LOCALIZEDTEXT("en-US","Published Int32");
    attr.displayName           = UA_LOCALIZEDTEXT("en-US","Published Int32");
    attr.dataType              = UA_TYPES[UA_TYPES_INT32].typeId;
    UA_Int32 publisherData     = 42;
    UA_Variant_setScalar(&attr.value, &publisherData, &UA_TYPES[UA_TYPES_INT32]);
    ck_assert(UA_Server_addVariableNode(server, PublishedVarId,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                        UA_QUALIFIEDNAME(1, "Published Int32"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                        attr, NULL, NULL) == UA_STATUSCODE_GOOD);

    UA_NodeId dataSetFieldId;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("Int32 Publish var");
    dataSetFieldConfig.field.variable.promotedField = UA_FALSE;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable = PublishedVarId;
    dataSetFieldConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
    UA_DataSetFieldResult PdsFieldResult = UA_Server_addDataSetField(server, publishedDataSetId1,
                              &dataSetFieldConfig, &dataSetFieldId);
    ck_assert(PdsFieldResult.result == UA_STATUSCODE_GOOD);

    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(dataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("DataSetWriter 1 ");
    dataSetWriterConfig.dataSetWriterId = 1;
    dataSetWriterConfig.keyFrameCount = 10;
    ck_assert(UA_Server_addDataSetWriter(server, writerGroupId1, publishedDataSetId1, &dataSetWriterConfig, &dataSetWriterId1) == UA_STATUSCODE_GOOD);


    /* Connection 2:
        Configure counterpart ReaderGroup with DataSetReader */

    memset(&connectionConfig, 0, sizeof(UA_PubSubConnectionConfig));
    connectionConfig.name = UA_STRING("UADP Connection 2");
    connectionConfig.enabled = UA_TRUE;
    connectionConfig.publisherIdType = UA_PUBSUB_PUBLISHERID_NUMERIC;
    connectionConfig.publisherId.numeric = 2;
    UA_NetworkAddressUrlDataType networkAddressUrl1 = {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.23:4840/")};
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl1,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.transportProfileUri = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    ck_assert(UA_Server_addPubSubConnection(server, &connectionConfig, &connectionId2) == UA_STATUSCODE_GOOD);


    UA_ReaderGroupConfig readerGroupConfig;
    memset (&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("ReaderGroup1");
    ck_assert(UA_Server_addReaderGroup(server, connectionId2, &readerGroupConfig,
                                       &readerGroupId1) == UA_STATUSCODE_GOOD);

    ck_assert(UA_Server_setReaderGroupOperational(server, readerGroupId1) == UA_STATUSCODE_GOOD);


    UA_DataSetReaderConfig readerConfig;                             
    memset (&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name = UA_STRING("DataSet Reader 1");
    UA_UInt16 publisherIdentifier = 1;
    UA_Variant_setScalar(&readerConfig.publisherId, &publisherIdentifier, &UA_TYPES[UA_TYPES_UINT16]);
    readerConfig.writerGroupId    = 1;
    readerConfig.dataSetWriterId  = 1;
    readerConfig.messageReceiveTimeout = MSG_RCV_TIMEOUT;

    UA_DataSetMetaDataType_init(&readerConfig.dataSetMetaData);
    UA_DataSetMetaDataType *pDataSetMetaData = &readerConfig.dataSetMetaData;
    pDataSetMetaData->name = UA_STRING ("DataSet 1");
    pDataSetMetaData->fieldsSize = 1;
    pDataSetMetaData->fields = (UA_FieldMetaData*) UA_Array_new (pDataSetMetaData->fieldsSize,
                         &UA_TYPES[UA_TYPES_FIELDMETADATA]);

    UA_FieldMetaData_init (&pDataSetMetaData->fields[0]);
    UA_NodeId_copy (&UA_TYPES[UA_TYPES_DATETIME].typeId,
                    &pDataSetMetaData->fields[0].dataType);
    pDataSetMetaData->fields[0].builtInType = UA_NS0ID_INT32;
    pDataSetMetaData->fields[0].name =  UA_STRING ("Int32 Var");
    pDataSetMetaData->fields[0].valueRank = -1; /* scalar */
    ck_assert(UA_Server_addDataSetReader(server, readerGroupId1, &readerConfig,
                                         &dataSetReaderId1) == UA_STATUSCODE_GOOD);

    /* subscriber variables */
    UA_NodeId folderId;
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName = UA_LOCALIZEDTEXT ("en-US", "Subscribed Variables");
    UA_QualifiedName folderBrowseName = UA_QUALIFIEDNAME (1, "Subscribed Variables");
    ck_assert(UA_Server_addObjectNode (server, UA_NODEID_NULL,
                             UA_NODEID_NUMERIC (0, UA_NS0ID_OBJECTSFOLDER),
                             UA_NODEID_NUMERIC (0, UA_NS0ID_ORGANIZES),
                             folderBrowseName, UA_NODEID_NUMERIC (0,
                             UA_NS0ID_BASEOBJECTTYPE), oAttr, NULL, &folderId) == UA_STATUSCODE_GOOD);
                            
    /* Variable to subscribe data */
    UA_VariableAttributes vAttr = UA_VariableAttributes_default;
    vAttr.description = UA_LOCALIZEDTEXT ("en-US", "Subscribed Int32");
    vAttr.displayName = UA_LOCALIZEDTEXT ("en-US", "Subscribed Int32");
    vAttr.dataType    = UA_TYPES[UA_TYPES_INT32].typeId;
    ck_assert(UA_Server_addVariableNode(server, SubscribedVarId,
                                        folderId,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),  UA_QUALIFIEDNAME(1, "Subscribed Int32"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), vAttr, NULL, NULL) == UA_STATUSCODE_GOOD);

    UA_TargetVariablesDataType targetVars;
    targetVars.targetVariablesSize = 1;
    targetVars.targetVariables     = (UA_FieldTargetDataType *)
                                        UA_calloc(targetVars.targetVariablesSize,
                                        sizeof(UA_FieldTargetDataType));
    UA_FieldTargetDataType_init(&targetVars.targetVariables[0]);
    targetVars.targetVariables[0].attributeId  = UA_ATTRIBUTEID_VALUE;
    targetVars.targetVariables[0].targetNodeId = SubscribedVarId;
    ck_assert(UA_Server_DataSetReader_createTargetVariables(server, dataSetReaderId1,
                                                            &targetVars) == UA_STATUSCODE_GOOD);

    UA_TargetVariablesDataType_deleteMembers(&targetVars);
    UA_free(pDataSetMetaData->fields);
    pDataSetMetaData->fields = 0;
}

START_TEST(SimpleMsgReceiveTimeoutTest) {

    PrepareSimpleMsgReceiveTimeoutTest();



    /* everything should be up and running */
   

    /* check that transmission works */


    /* disable WriterGroup */
    ck_assert(UA_Server_setWriterGroupDisabled(server, writerGroupId1) == UA_STATUSCODE_GOOD);

    /* MsgReceiveTimeout shall occur */


} END_TEST


int main(void) {
    TCase *tc_subscribe_msgrcvtimeout = tcase_create("PubSub subscriber message receive timeout test");
    tcase_add_checked_fixture(tc_subscribe_msgrcvtimeout, setup, teardown);
    tcase_add_test(tc_subscribe_msgrcvtimeout, SimpleMsgReceiveTimeoutTest);

    Suite *s = suite_create("PubSub subscriber message receive timeout test suite");
    suite_add_tcase(s, tc_subscribe_msgrcvtimeout);

    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr,CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

