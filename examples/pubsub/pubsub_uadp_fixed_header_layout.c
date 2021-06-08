#include <open62541/plugin/pubsub_udp.h>
#include <open62541/server_config_default.h>
#include <open62541/server_pubsub.h>
#include <open62541/plugin/log_stdout.h>

#include "ua_pubsub.h"

#include <assert.h>

static UA_Server *server = NULL;

UA_Boolean running = true;
static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}

/* global variables for fast-path configuration */
static UA_Boolean UseFastPath = UA_FALSE;
static UA_DataValue *pFastPathPublisherValue = 0;
static UA_DataValue *pFastPathSubscriberValue = 0;

UA_UadpNetworkMessageContentMask cNetworkMessageContentMask = 0x3F;
UA_DataSetFieldContentMask cDataSetFieldContentMask = UA_DATASETFIELDCONTENTMASK_RAWDATA;

/***************************************************************************************************/
/***************************************************************************************************/
/* utility functions to setup the PubSub configuration */

/***************************************************************************************************/
static void AddConnection(
    char *pName, 
    UA_UInt32 PublisherId,
    UA_NodeId *opConnectionId) {

    assert(pName != 0);
    assert(opConnectionId != 0);

    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(UA_PubSubConnectionConfig));
    connectionConfig.name = UA_STRING(pName);
    connectionConfig.enabled = UA_TRUE;
    connectionConfig.transportProfileUri = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType networkAddressUrl = {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);

    connectionConfig.publisherIdType = UA_PUBSUB_PUBLISHERID_NUMERIC;
    connectionConfig.publisherId.numeric = PublisherId;

    assert(UA_Server_addPubSubConnection(server, &connectionConfig, opConnectionId) == UA_STATUSCODE_GOOD);
    assert(UA_PubSubConnection_regist(server, opConnectionId) == UA_STATUSCODE_GOOD);
}

/***************************************************************************************************/
static void AddWriterGroup(
    UA_NodeId *pConnectionId,
    char *pName, 
    UA_UInt32 WriterGroupId,
    UA_Duration PublishingInterval,
    UA_NodeId *opWriterGroupId) {

    assert(pConnectionId != 0);
    assert(pName != 0);
    assert(opWriterGroupId != 0);

    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING(pName);
    writerGroupConfig.publishingInterval = PublishingInterval;
    writerGroupConfig.enabled = UA_FALSE;
    writerGroupConfig.writerGroupId = (UA_UInt16) WriterGroupId;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;
    writerGroupConfig.messageSettings.encoding             = UA_EXTENSIONOBJECT_DECODED;
    writerGroupConfig.messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE];
    UA_UadpWriterGroupMessageDataType *writerGroupMessage  = UA_UadpWriterGroupMessageDataType_new();
    writerGroupMessage->networkMessageContentMask          = cNetworkMessageContentMask;
    writerGroupMessage->dataSetOrdering = UA_DATASETORDERINGTYPE_ASCENDINGWRITERID;
    writerGroupMessage->groupVersion = 0;   // indicates that no version information is available
    writerGroupConfig.messageSettings.content.decoded.data = writerGroupMessage;
    writerGroupConfig.securityMode = UA_MESSAGESECURITYMODE_NONE;

    if (UseFastPath) {
        writerGroupConfig.rtLevel = UA_PUBSUB_RT_FIXED_SIZE;
    }
    assert(UA_Server_addWriterGroup(server, *pConnectionId, &writerGroupConfig, opWriterGroupId) == UA_STATUSCODE_GOOD);
    UA_UadpWriterGroupMessageDataType_delete(writerGroupMessage);
}

/***************************************************************************************************/
static void AddPublishedDataSet(
    UA_NodeId *pWriterGroupId,
    char *pPublishedDataSetName, 
    char *pDataSetWriterName,
    UA_UInt32 DataSetWriterId,
    UA_NodeId *opPublishedDataSetId, 
    UA_NodeId *opPublishedVarId,
    UA_NodeId *opDataSetWriterId) {

    assert(pWriterGroupId != 0);
    assert(pPublishedDataSetName != 0);
    assert(pDataSetWriterName != 0);
    assert(opPublishedDataSetId != 0);
    assert(opPublishedVarId != 0);
    assert(opDataSetWriterId != 0);

    UA_PublishedDataSetConfig pdsConfig;
    memset(&pdsConfig, 0, sizeof(UA_PublishedDataSetConfig));
    pdsConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    pdsConfig.name = UA_STRING(pPublishedDataSetName);
    UA_AddPublishedDataSetResult result = UA_Server_addPublishedDataSet(server, &pdsConfig, opPublishedDataSetId);
    assert(result.addResult == UA_STATUSCODE_GOOD);

    /* Create variable to publish integer data */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.description           = UA_LOCALIZEDTEXT("en-US","Published Int32");
    attr.displayName           = UA_LOCALIZEDTEXT("en-US","Published Int32");
    attr.dataType              = UA_TYPES[UA_TYPES_INT32].typeId;
    UA_Int32 publisherData     = 42;
    UA_Variant_setScalar(&attr.value, &publisherData, &UA_TYPES[UA_TYPES_INT32]);
    assert(UA_Server_addVariableNode(server, UA_NODEID_NULL,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                        UA_QUALIFIEDNAME(1, "Published Int32"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                        attr, NULL, opPublishedVarId) == UA_STATUSCODE_GOOD);

    UA_NodeId dataSetFieldId;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("Int32 Publish var");
    dataSetFieldConfig.field.variable.promotedField = UA_FALSE;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable = *opPublishedVarId;
    dataSetFieldConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;


    if (UseFastPath) {
        dataSetFieldConfig.field.variable.rtValueSource.rtInformationModelNode = UA_TRUE;
        pFastPathPublisherValue = UA_DataValue_new();
        assert(pFastPathPublisherValue != 0);
        UA_Int32 *pPublisherData  = UA_Int32_new();
        assert(pPublisherData != 0);
        *pPublisherData = 42;
        UA_Variant_setScalar(&pFastPathPublisherValue->value, pPublisherData, &UA_TYPES[UA_TYPES_INT32]);
        /* add external value backend for fast-path */
        UA_ValueBackend valueBackend;
        memset(&valueBackend, 0, sizeof(valueBackend));
        valueBackend.backendType = UA_VALUEBACKENDTYPE_EXTERNAL;
        valueBackend.backend.external.value = &pFastPathPublisherValue;
        assert(UA_STATUSCODE_GOOD == UA_Server_setVariableNode_valueBackend(server, *opPublishedVarId, valueBackend));
    }
    UA_DataSetFieldResult PdsFieldResult = UA_Server_addDataSetField(server, *opPublishedDataSetId,
                              &dataSetFieldConfig, &dataSetFieldId);
    assert(PdsFieldResult.result == UA_STATUSCODE_GOOD);

    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(dataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING(pDataSetWriterName);
    dataSetWriterConfig.dataSetWriterId = (UA_UInt16) DataSetWriterId;
    dataSetWriterConfig.keyFrameCount = 1;
    dataSetWriterConfig.dataSetFieldContentMask = cDataSetFieldContentMask;
    UA_UadpDataSetWriterMessageDataType *pDsWMessageSettings = UA_UadpDataSetWriterMessageDataType_new();
    assert(pDsWMessageSettings != 0);
    UA_UadpDataSetWriterMessageDataType_init(pDsWMessageSettings);
    pDsWMessageSettings->dataSetMessageContentMask = UA_UADPDATASETMESSAGECONTENTMASK_STATUS |
        UA_UADPDATASETMESSAGECONTENTMASK_SEQUENCENUMBER;
    dataSetWriterConfig.messageSettings.encoding = UA_EXTENSIONOBJECT_DECODED;
    dataSetWriterConfig.messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_UADPDATASETWRITERMESSAGEDATATYPE];
    dataSetWriterConfig.messageSettings.content.decoded.data = pDsWMessageSettings;

    assert(UA_Server_addDataSetWriter(server, *pWriterGroupId, *opPublishedDataSetId, &dataSetWriterConfig, opDataSetWriterId) == UA_STATUSCODE_GOOD);
    UA_free(pDsWMessageSettings);
    pDsWMessageSettings = 0;
}

/***************************************************************************************************/
static void AddReaderGroup(
    UA_NodeId *pConnectionId,
    char *pName, 
    UA_NodeId *opReaderGroupId) {

    assert(pConnectionId != 0);
    assert(pName != 0);
    assert(opReaderGroupId != 0);

    UA_ReaderGroupConfig readerGroupConfig;
    memset (&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING(pName);
    if (UseFastPath) {
        readerGroupConfig.rtLevel = UA_PUBSUB_RT_FIXED_SIZE;
    }
    assert(UA_Server_addReaderGroup(server, *pConnectionId, &readerGroupConfig,
                                       opReaderGroupId) == UA_STATUSCODE_GOOD);
}

/***************************************************************************************************/
static void AddDataSetReader(
    UA_NodeId *pReaderGroupId,
    char *pName, 
    UA_UInt32 PublisherId,
    UA_UInt32 WriterGroupId,
    UA_UInt32 DataSetWriterId,
    UA_Duration MessageReceiveTimeout,
    UA_NodeId *opSubscriberVarId,
    UA_NodeId *opDataSetReaderId) {

    assert(pReaderGroupId != 0);
    assert(pName != 0);
    assert(opSubscriberVarId != 0);
    assert(opDataSetReaderId != 0);

    UA_DataSetReaderConfig readerConfig;
    memset (&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name = UA_STRING(pName);
    UA_Variant_setScalar(&readerConfig.publisherId, (UA_UInt16*) &PublisherId, &UA_TYPES[UA_TYPES_UINT16]);
    readerConfig.writerGroupId    = (UA_UInt16) WriterGroupId;
    readerConfig.dataSetWriterId  = (UA_UInt16) DataSetWriterId;
    readerConfig.dataSetFieldContentMask = cDataSetFieldContentMask;
    readerConfig.messageReceiveTimeout = MessageReceiveTimeout;
    readerConfig.messageSettings.encoding = UA_EXTENSIONOBJECT_DECODED;
    readerConfig.messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_UADPDATASETREADERMESSAGEDATATYPE];
    UA_UadpDataSetReaderMessageDataType *dsReaderMessage = UA_UadpDataSetReaderMessageDataType_new();
    dsReaderMessage->networkMessageContentMask = cNetworkMessageContentMask;
    dsReaderMessage->groupVersion = 0;
    dsReaderMessage->networkMessageNumber = 1;
    readerConfig.messageSettings.content.decoded.data = dsReaderMessage;

    UA_DataSetMetaDataType_init(&readerConfig.dataSetMetaData);
    UA_DataSetMetaDataType *pDataSetMetaData = &readerConfig.dataSetMetaData;
    pDataSetMetaData->name = UA_STRING (pName);
    pDataSetMetaData->fieldsSize = 1;
    pDataSetMetaData->fields = (UA_FieldMetaData*) UA_Array_new (pDataSetMetaData->fieldsSize,
                         &UA_TYPES[UA_TYPES_FIELDMETADATA]);

    UA_FieldMetaData_init (&pDataSetMetaData->fields[0]);
    UA_NodeId_copy (&UA_TYPES[UA_TYPES_INT32].typeId,
                    &pDataSetMetaData->fields[0].dataType);
    pDataSetMetaData->fields[0].builtInType = UA_NS0ID_INT32;
    pDataSetMetaData->fields[0].name =  UA_STRING ("Int32 Var");
    pDataSetMetaData->fields[0].valueRank = -1;
    assert(UA_Server_addDataSetReader(server, *pReaderGroupId, &readerConfig,
                                         opDataSetReaderId) == UA_STATUSCODE_GOOD);
    UA_UadpDataSetReaderMessageDataType_delete(dsReaderMessage);
    dsReaderMessage = 0;

    /* Variable to subscribe data */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.description = UA_LOCALIZEDTEXT ("en-US", "Subscribed Int32");
    attr.displayName = UA_LOCALIZEDTEXT ("en-US", "Subscribed Int32");
    attr.dataType    = UA_TYPES[UA_TYPES_INT32].typeId;
    UA_Int32 SubscriberData = 0;
    UA_Variant_setScalar(&attr.value, &SubscriberData, &UA_TYPES[UA_TYPES_INT32]);
    assert(UA_Server_addVariableNode(server, UA_NODEID_NULL,
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),  UA_QUALIFIEDNAME(1, "Subscribed Int32"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, opSubscriberVarId) == UA_STATUSCODE_GOOD);

    if (UseFastPath) {
        pFastPathSubscriberValue = UA_DataValue_new();
        assert(pFastPathSubscriberValue != 0);
        UA_Int32 *pSubscriberData  = UA_Int32_new();
        assert(pSubscriberData != 0);
        *pSubscriberData = 0;
        UA_Variant_setScalar(&pFastPathSubscriberValue->value, pSubscriberData, &UA_TYPES[UA_TYPES_INT32]);
        /* add external value backend for fast-path */
        UA_ValueBackend valueBackend;
        memset(&valueBackend, 0, sizeof(valueBackend));
        valueBackend.backendType = UA_VALUEBACKENDTYPE_EXTERNAL;
        valueBackend.backend.external.value = &pFastPathSubscriberValue;
        assert(UA_STATUSCODE_GOOD == UA_Server_setVariableNode_valueBackend(server, *opSubscriberVarId, valueBackend));
    }

    UA_FieldTargetVariable *pTargetVariables =  (UA_FieldTargetVariable *)
        UA_calloc(readerConfig.dataSetMetaData.fieldsSize, sizeof(UA_FieldTargetVariable));
    assert(pTargetVariables != 0);
    
    UA_FieldTargetDataType_init(&pTargetVariables[0].targetVariable);

    pTargetVariables[0].targetVariable.attributeId  = UA_ATTRIBUTEID_VALUE;
    pTargetVariables[0].targetVariable.targetNodeId = *opSubscriberVarId;

    assert(UA_Server_DataSetReader_createTargetVariables(server, *opDataSetReaderId,
        readerConfig.dataSetMetaData.fieldsSize, pTargetVariables) == UA_STATUSCODE_GOOD);
    
    UA_FieldTargetDataType_clear(&pTargetVariables[0].targetVariable);
    UA_free(pTargetVariables);
    pTargetVariables = 0;

    UA_free(pDataSetMetaData->fields);
    pDataSetMetaData->fields = 0;
}

/***************************************************************************************************/
/***************************************************************************************************/


int main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);
    UA_ServerConfig_addPubSubTransportLayer(config, UA_PubSubTransportLayerUDPMP());

    /* setup Connection 1: 1 writergroup, 1 writer */
    UA_NodeId ConnId_1;
    UA_NodeId_init(&ConnId_1);
    AddConnection("Conn1", 1, &ConnId_1);

    UA_NodeId WGId_Conn1_WG1;
    UA_NodeId_init(&WGId_Conn1_WG1);
    UA_Duration PublishingInterval_Conn1WG1 = 300.0;
    AddWriterGroup(&ConnId_1, "Conn1_WG1", 1, PublishingInterval_Conn1WG1, &WGId_Conn1_WG1);

    UA_NodeId DsWId_Conn1_WG1_DS1;
    UA_NodeId_init(&DsWId_Conn1_WG1_DS1);
    UA_NodeId VarId_Conn1_WG1;
    UA_NodeId_init(&VarId_Conn1_WG1);
    UA_NodeId PDSId_Conn1_WG1_PDS1;
    UA_NodeId_init(&PDSId_Conn1_WG1_PDS1);

    AddPublishedDataSet(&WGId_Conn1_WG1, "Conn1_WG1_PDS1", "Conn1_WG1_DS1", 1, &PDSId_Conn1_WG1_PDS1, 
        &VarId_Conn1_WG1, &DsWId_Conn1_WG1_DS1);


    /* setup corresponding readergroup and reader for Connection 1 */

    UA_NodeId RGId_Conn1_RG1;
    UA_NodeId_init(&RGId_Conn1_RG1);
    AddReaderGroup(&ConnId_1, "Conn1_RG1", &RGId_Conn1_RG1);
    UA_NodeId DSRId_Conn1_RG1_DSR1;
    UA_NodeId_init(&DSRId_Conn1_RG1_DSR1);
    UA_NodeId VarId_Conn1_RG1_DSR1;
    UA_NodeId_init(&VarId_Conn1_RG1_DSR1);
    UA_Duration MessageReceiveTimeout = 400.0;
    AddDataSetReader(&RGId_Conn1_RG1, "Conn1_RG1_DSR1", 1, 1, 1, MessageReceiveTimeout, &VarId_Conn1_RG1_DSR1, &DSRId_Conn1_RG1_DSR1);


    UA_StatusCode retval = UA_STATUSCODE_GOOD;

    if (UseFastPath) {
        assert(UA_Server_freezeWriterGroupConfiguration(server, WGId_Conn1_WG1) == UA_STATUSCODE_GOOD);
        assert(UA_Server_freezeReaderGroupConfiguration(server, RGId_Conn1_RG1) == UA_STATUSCODE_GOOD);
    }

    assert(UA_Server_setWriterGroupOperational(server, WGId_Conn1_WG1) == UA_STATUSCODE_GOOD);
    assert(UA_Server_setReaderGroupOperational(server, RGId_Conn1_RG1) == UA_STATUSCODE_GOOD);


    retval |= UA_Server_run(server, &running);


    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;

    return 0;
}