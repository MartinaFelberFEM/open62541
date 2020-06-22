#include <check.h>

#include <open62541/plugin/pubsub_udp.h>
#include <open62541/server_config_default.h>
#include <open62541/server_pubsub.h>

#include "ua_pubsub.h"

UA_Server *server = NULL;

/* TODO: shall we check the state machine impl? -> getState? */
/* TODO: we think the connection needs a state too ... */

/* Note: only set WriterGroup to operational after all DataSetWriters have been added
    otherwise state of DataSetWriter is still disabled */


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

static void ValidatePublishSubscribe(
    UA_NodeId PublishedVarId,
    UA_NodeId SubscribedVarId,
    UA_Int32 TestValue,
    UA_UInt32 ServerIterateCnt)
{
    /* set variable value to publish */
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_Variant writeValue;
    UA_Variant_setScalar(&writeValue, &TestValue, &UA_TYPES[UA_TYPES_INT32]);
    ck_assert(UA_Server_writeValue(server, PublishedVarId, writeValue) == UA_STATUSCODE_GOOD);


    for (UA_UInt32 i = 0; i < ServerIterateCnt; i++) 
    {
        UA_Server_run_iterate(server, true);
    }

    UA_Variant SubscribedNodeData;
    UA_Variant_init(&SubscribedNodeData);
    retVal = UA_Server_readValue(server, SubscribedVarId, &SubscribedNodeData);
    ck_assert_int_eq(retVal, UA_STATUSCODE_GOOD);
    ck_assert(SubscribedNodeData.data != 0);
    ck_assert_int_eq(TestValue, *(UA_Int32 *)SubscribedNodeData.data);
    UA_Variant_clear(&SubscribedNodeData);
}


static void PrepareSimpleMsgReceiveTimeoutTest(
    UA_NodeId *pPublisherConnectionId,
    UA_NodeId *pWriterGroupId,
    UA_NodeId *pDataSetWriterId,
    UA_NodeId *pPublishedVarId,
    const UA_Duration PublishingInterval,
    UA_NodeId *pSubscriberConnectionId,
    UA_NodeId *pReaderGroupId,
    UA_NodeId *pDataSetReaderId,
    UA_NodeId *pSubscribedVarId,
    const UA_Duration MessageReceiveTimeout) {

    UA_NodeId publishedDataSetId1;

    /* Connection 1: Publisher - Publisher Id = 1
        Configure WriterGroup with DataSetWriter, publish Int32 variable */

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
    ck_assert(UA_Server_addPubSubConnection(server, &connectionConfig, pPublisherConnectionId) == UA_STATUSCODE_GOOD);
    ck_assert(UA_PubSubConnection_regist(server, pPublisherConnectionId) == UA_STATUSCODE_GOOD);

    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("WriterGroup 1");
    writerGroupConfig.publishingInterval = PublishingInterval;
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
    ck_assert(UA_Server_addWriterGroup(server, *pPublisherConnectionId, &writerGroupConfig, pWriterGroupId) == UA_STATUSCODE_GOOD);
    UA_UadpWriterGroupMessageDataType_delete(writerGroupMessage);

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
    ck_assert(UA_Server_addVariableNode(server, *pPublishedVarId,
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
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable = *pPublishedVarId;
    dataSetFieldConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
    UA_DataSetFieldResult PdsFieldResult = UA_Server_addDataSetField(server, publishedDataSetId1,
                              &dataSetFieldConfig, &dataSetFieldId);
    ck_assert(PdsFieldResult.result == UA_STATUSCODE_GOOD);

    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(dataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("DataSetWriter 1 ");
    dataSetWriterConfig.dataSetWriterId = 1;
    dataSetWriterConfig.keyFrameCount = 10;
    ck_assert(UA_Server_addDataSetWriter(server, *pWriterGroupId, publishedDataSetId1, &dataSetWriterConfig, pDataSetWriterId) == UA_STATUSCODE_GOOD);
    

    /* Connection 2: Subscriber - Publisher Id = 2
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
    ck_assert(UA_Server_addPubSubConnection(server, &connectionConfig, pSubscriberConnectionId) == UA_STATUSCODE_GOOD);
    ck_assert(UA_PubSubConnection_regist(server, pSubscriberConnectionId) == UA_STATUSCODE_GOOD);

    UA_ReaderGroupConfig readerGroupConfig;
    memset (&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("ReaderGroup1");
    ck_assert(UA_Server_addReaderGroup(server, *pSubscriberConnectionId, &readerGroupConfig,
                                       pReaderGroupId) == UA_STATUSCODE_GOOD);

    UA_DataSetReaderConfig readerConfig;
    memset (&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name = UA_STRING("DataSet Reader 1");
    UA_UInt16 publisherIdentifier = 1;
    UA_Variant_setScalar(&readerConfig.publisherId, &publisherIdentifier, &UA_TYPES[UA_TYPES_UINT16]);
    readerConfig.writerGroupId    = 1;
    readerConfig.dataSetWriterId  = 1;
    readerConfig.messageReceiveTimeout = MessageReceiveTimeout;

    UA_DataSetMetaDataType_init(&readerConfig.dataSetMetaData);
    UA_DataSetMetaDataType *pDataSetMetaData = &readerConfig.dataSetMetaData;
    pDataSetMetaData->name = UA_STRING ("DataSet 1");
    pDataSetMetaData->fieldsSize = 1;
    pDataSetMetaData->fields = (UA_FieldMetaData*) UA_Array_new (pDataSetMetaData->fieldsSize,
                         &UA_TYPES[UA_TYPES_FIELDMETADATA]);

    UA_FieldMetaData_init (&pDataSetMetaData->fields[0]);
    UA_NodeId_copy (&UA_TYPES[UA_TYPES_INT32].typeId,
                    &pDataSetMetaData->fields[0].dataType);
    pDataSetMetaData->fields[0].builtInType = UA_NS0ID_INT32;
    pDataSetMetaData->fields[0].name =  UA_STRING ("Int32 Var");
    pDataSetMetaData->fields[0].valueRank = -1;
    ck_assert(UA_Server_addDataSetReader(server, *pReaderGroupId, &readerConfig,
                                         pDataSetReaderId) == UA_STATUSCODE_GOOD);

    /* Variable to subscribe data */
    attr = UA_VariableAttributes_default;
    attr.description = UA_LOCALIZEDTEXT ("en-US", "Subscribed Int32");
    attr.displayName = UA_LOCALIZEDTEXT ("en-US", "Subscribed Int32");
    attr.dataType    = UA_TYPES[UA_TYPES_INT32].typeId;
    UA_Int32 SubscriberData = 0;
    UA_Variant_setScalar(&attr.value, &SubscriberData, &UA_TYPES[UA_TYPES_INT32]);
    ck_assert(UA_Server_addVariableNode(server, *pSubscribedVarId,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),  UA_QUALIFIEDNAME(1, "Subscribed Int32"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL) == UA_STATUSCODE_GOOD);

    UA_TargetVariablesDataType targetVars;
    targetVars.targetVariablesSize = 1;
    targetVars.targetVariables     = (UA_FieldTargetDataType *)
                                        UA_calloc(targetVars.targetVariablesSize,
                                        sizeof(UA_FieldTargetDataType));
    UA_FieldTargetDataType_init(&targetVars.targetVariables[0]);
    targetVars.targetVariables[0].attributeId  = UA_ATTRIBUTEID_VALUE;
    targetVars.targetVariables[0].targetNodeId = *pSubscribedVarId;
    ck_assert(UA_Server_DataSetReader_createTargetVariables(server, *pDataSetReaderId,
                                                            &targetVars) == UA_STATUSCODE_GOOD);

    UA_TargetVariablesDataType_deleteMembers(&targetVars);
    UA_free(pDataSetMetaData->fields);
    pDataSetMetaData->fields = 0;

    /* finished configuration, check PubSub component states */

    UA_PubSubState state;    
    /* check WriterGroup and DataSetWriter state */
    ck_assert(UA_Server_WriterGroup_getState(server, *pWriterGroupId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_DISABLED);
    ck_assert(UA_Server_DataSetWriter_getState(server, *pDataSetWriterId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_DISABLED);

    /* set WriterGroup operational */
    ck_assert(UA_Server_setWriterGroupOperational(server, *pWriterGroupId) == UA_STATUSCODE_GOOD);

    ck_assert(UA_Server_WriterGroup_getState(server, *pWriterGroupId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_OPERATIONAL);
    ck_assert(UA_Server_DataSetWriter_getState(server, *pDataSetWriterId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_OPERATIONAL);

    /* check ReaderGroup and DataSetReader state */
    ck_assert(UA_Server_ReaderGroup_getState(server, *pReaderGroupId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_DISABLED);
    ck_assert(UA_Server_DataSetReader_getState(server, *pDataSetReaderId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_DISABLED);

    /* set ReaderGroup operational */
    ck_assert(UA_Server_setReaderGroupOperational(server, *pReaderGroupId) == UA_STATUSCODE_GOOD);

    ck_assert(UA_Server_ReaderGroup_getState(server, *pReaderGroupId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_OPERATIONAL);
    ck_assert(UA_Server_DataSetReader_getState(server, *pDataSetReaderId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_OPERATIONAL);
}

START_TEST(SimpleMsgReceiveTimeoutTest) {

    UA_NodeId PublisherConnectionId;
    UA_NodeId_init(&PublisherConnectionId);
    UA_NodeId WriterGroupId;
    UA_NodeId_init(&WriterGroupId);
    UA_NodeId DataSetWriterId;
    UA_NodeId_init(&DataSetWriterId);
    UA_NodeId PublishedVarId = UA_NODEID_NUMERIC(1, 1);
    UA_Duration PublishingInterval = 10.0;

    UA_NodeId SubscriberConnectionId;
    UA_NodeId_init(&SubscriberConnectionId);
    UA_NodeId ReaderGroupId;
    UA_NodeId_init(&ReaderGroupId);
    UA_NodeId DataSetReaderId;
    UA_NodeId_init(&DataSetReaderId);
    UA_NodeId SubscribedVarId = UA_NODEID_NUMERIC(1, 2);
    UA_Duration MessageReceiveTimeout = 20.0;

    PrepareSimpleMsgReceiveTimeoutTest(&PublisherConnectionId, &WriterGroupId, &DataSetWriterId, &PublishedVarId, PublishingInterval, 
        &SubscriberConnectionId, &ReaderGroupId, &DataSetReaderId, &SubscribedVarId, MessageReceiveTimeout);

    /* check that publish/subscribe works */
    ValidatePublishSubscribe(PublishedVarId, SubscribedVarId, 10, 1);

    ValidatePublishSubscribe(PublishedVarId, SubscribedVarId, 33, 1);
    
    UA_PubSubState state;
    ck_assert(UA_Server_DataSetReader_getState(server, DataSetReaderId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_OPERATIONAL);

    /* now we disable the publisher WriterGroup and check if a MessageReceiveTimeout occurs at Subscriber */
    ck_assert(UA_Server_setWriterGroupDisabled(server, WriterGroupId) == UA_STATUSCODE_GOOD);


    ck_assert(UA_Server_WriterGroup_getState(server, WriterGroupId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_DISABLED);
    ck_assert(UA_Server_DataSetWriter_getState(server, DataSetWriterId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_DISABLED);

    /* Server run timeout should be 50 ms -> so timeout should occur immediately */
    for (size_t i = 0; i < 1; i++) {
        UA_Server_run_iterate(server, true);
    }

    /* state of ReaderGroup should still be ok */
    ck_assert(UA_Server_ReaderGroup_getState(server, ReaderGroupId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_OPERATIONAL);
     /* but DataSetReader state shall be error */
    ck_assert(UA_Server_DataSetReader_getState(server, DataSetReaderId, &state) == UA_STATUSCODE_GOOD);
    ck_assert(state == UA_PUBSUBSTATE_ERROR);

} END_TEST


int main(void) {

    /* test case */
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

