/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2019 Fraunhofer IOSB (Author: Andreas Ebner)
 * Copyright (c) 2018 Fraunhofer IOSB (Author: Julius Pfrommer)
 */

#include "server/ua_server_internal.h"
#include "ua_pubsub_ns0.h"

#ifdef UA_ENABLE_PUBSUB /* conditional compilation */

#define UA_DATETIMESTAMP_2000 125911584000000000

UA_StatusCode
UA_Server_addPubSubConnection(UA_Server *server,
                              const UA_PubSubConnectionConfig *connectionConfig,
                              UA_NodeId *connectionIdentifier) {
    /* Find the matching UA_PubSubTransportLayers */
    UA_PubSubTransportLayer *tl = NULL;
    for(size_t i = 0; i < server->config.pubsubTransportLayersSize; i++) {
        if(connectionConfig &&
           UA_String_equal(&server->config.pubsubTransportLayers[i].transportProfileUri,
                           &connectionConfig->transportProfileUri)) {
            tl = &server->config.pubsubTransportLayers[i];
        }
    }
    if(!tl) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PubSub Connection creation failed. Requested transport layer not found.");
        return UA_STATUSCODE_BADNOTFOUND;
    }

    /* Create a copy of the connection config */
    UA_PubSubConnectionConfig *tmpConnectionConfig = (UA_PubSubConnectionConfig *)
        UA_calloc(1, sizeof(UA_PubSubConnectionConfig));
    if(!tmpConnectionConfig){
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PubSub Connection creation failed. Out of Memory.");
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }

    UA_StatusCode retval = UA_PubSubConnectionConfig_copy(connectionConfig, tmpConnectionConfig);
    if(retval != UA_STATUSCODE_GOOD){
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PubSub Connection creation failed. Could not copy the config.");
        return retval;
    }

    /* Create new connection and add to UA_PubSubManager */
    UA_PubSubConnection *newConnectionsField = (UA_PubSubConnection *)
        UA_calloc(1, sizeof(UA_PubSubConnection));
    if(!newConnectionsField) {
        UA_PubSubConnectionConfig_clear(tmpConnectionConfig);
        UA_free(tmpConnectionConfig);
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PubSub Connection creation failed. Out of Memory.");
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    if (server->pubSubManager.connectionsSize != 0)
        TAILQ_INSERT_TAIL(&server->pubSubManager.connections, newConnectionsField, listEntry);
    else {
        TAILQ_INIT(&server->pubSubManager.connections);
        TAILQ_INSERT_HEAD(&server->pubSubManager.connections, newConnectionsField, listEntry);
    }

    server->pubSubManager.connectionsSize++;

    LIST_INIT(&newConnectionsField->writerGroups);
    newConnectionsField->config = tmpConnectionConfig;

    /* Open the channel */
    newConnectionsField->channel = tl->createPubSubChannel(newConnectionsField->config);
    if(!newConnectionsField->channel) {
        UA_PubSubConnection_clear(server, newConnectionsField);
        TAILQ_REMOVE(&server->pubSubManager.connections, newConnectionsField, listEntry);
        server->pubSubManager.connectionsSize--;
        UA_free(newConnectionsField);
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PubSub Connection creation failed. Transport layer creation problem.");
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    UA_PubSubManager_generateUniqueNodeId(server, &newConnectionsField->identifier);

    if(connectionIdentifier)
        UA_NodeId_copy(&newConnectionsField->identifier, connectionIdentifier);

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    addPubSubConnectionRepresentation(server, newConnectionsField);
#endif

    /* TODO: shall we add 1 PubSub reflection callback for every connection?
        or only 1 global callback for all connections?
    */
    if (server->pubSubManager.connectionsSize == 1) {
        /* we start with only adding 1 global PubSub reflection callback */

        UA_Duration interval = 0.0;
        retval = UA_Server_calcPubSubReflectionCbInterval(server, &interval);
        if (retval == UA_STATUSCODE_GOOD) {
            retval = UA_PubSubManager_addReflectionCallback(server, UA_PubSubManager_reflectionCallback, 
                interval, &server->pubSubManager.reflectionCallbackId); 
        }
        if (retval != UA_STATUSCODE_GOOD) {
            UA_PubSubConnection_clear(server, newConnectionsField);
            TAILQ_REMOVE(&server->pubSubManager.connections, newConnectionsField, listEntry);
            server->pubSubManager.connectionsSize--;
            UA_free(newConnectionsField);
            UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                "PubSub Connection creation failed. Adding reflection callback failed");
            return UA_STATUSCODE_BADINTERNALERROR;
        }
    }

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_Server_removePubSubConnection(UA_Server *server, const UA_NodeId connection) {
    //search the identified Connection and store the Connection index
    UA_PubSubConnection *currentConnection = UA_PubSubConnection_findConnectionbyId(server, connection);
    if(!currentConnection)
        return UA_STATUSCODE_BADNOTFOUND;

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    removePubSubConnectionRepresentation(server, currentConnection);
#endif
    server->pubSubManager.connectionsSize--;

    UA_PubSubConnection_clear(server, currentConnection);
    TAILQ_REMOVE(&server->pubSubManager.connections, currentConnection, listEntry);
    UA_free(currentConnection);

    if (server->pubSubManager.connectionsSize == 0) {
        UA_PubSubManager_removeReflectionCallback(server, server->pubSubManager.reflectionCallbackId);
    }
    return UA_STATUSCODE_GOOD;
}

UA_AddPublishedDataSetResult
UA_Server_addPublishedDataSet(UA_Server *server, const UA_PublishedDataSetConfig *publishedDataSetConfig,
                              UA_NodeId *pdsIdentifier) {
    UA_AddPublishedDataSetResult result = {UA_STATUSCODE_BADINVALIDARGUMENT, 0, NULL, {0, 0}};
    if(!publishedDataSetConfig){
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PublishedDataSet creation failed. No config passed in.");
        return result;
    }
    if(publishedDataSetConfig->publishedDataSetType != UA_PUBSUB_DATASET_PUBLISHEDITEMS){
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PublishedDataSet creation failed. Unsupported PublishedDataSet type.");
        return result;
    }
    //deep copy the given connection config
    UA_PublishedDataSetConfig tmpPublishedDataSetConfig;
    memset(&tmpPublishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
    if(UA_PublishedDataSetConfig_copy(publishedDataSetConfig, &tmpPublishedDataSetConfig) != UA_STATUSCODE_GOOD){
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PublishedDataSet creation failed. Configuration copy failed.");
        result.addResult = UA_STATUSCODE_BADINTERNALERROR;
        return result;
    }
    //create new PDS and add to UA_PubSubManager
    UA_PublishedDataSet *newPubSubDataSetField = (UA_PublishedDataSet *)
            UA_calloc(1, sizeof(UA_PublishedDataSet));
    if(!newPubSubDataSetField) {
        UA_PublishedDataSetConfig_clear(&tmpPublishedDataSetConfig);
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER,
                     "PublishedDataSet creation failed. Out of Memory.");
        result.addResult = UA_STATUSCODE_BADOUTOFMEMORY;
        return result;
    }
    memset(newPubSubDataSetField, 0, sizeof(UA_PublishedDataSet));
    TAILQ_INIT(&newPubSubDataSetField->fields);
    newPubSubDataSetField->config = tmpPublishedDataSetConfig;

    if (server->pubSubManager.publishedDataSetsSize != 0)
        TAILQ_INSERT_TAIL(&server->pubSubManager.publishedDataSets, newPubSubDataSetField, listEntry);
    else {
        TAILQ_INIT(&server->pubSubManager.publishedDataSets);
        TAILQ_INSERT_HEAD(&server->pubSubManager.publishedDataSets, newPubSubDataSetField, listEntry);
    }
    if(tmpPublishedDataSetConfig.publishedDataSetType == UA_PUBSUB_DATASET_PUBLISHEDITEMS_TEMPLATE){
        //parse template config and add fields (later PubSub batch)
    }
    //generate unique nodeId
    UA_PubSubManager_generateUniqueNodeId(server, &newPubSubDataSetField->identifier);
    if(pdsIdentifier != NULL){
        UA_NodeId_copy(&newPubSubDataSetField->identifier, pdsIdentifier);
    }

    result.addResult = UA_STATUSCODE_GOOD;
    result.fieldAddResults = NULL;
    result.fieldAddResultsSize = 0;

    //fill the DataSetMetaData
    switch(tmpPublishedDataSetConfig.publishedDataSetType){
        case UA_PUBSUB_DATASET_PUBLISHEDITEMS_TEMPLATE:
            if(UA_DataSetMetaDataType_copy(&tmpPublishedDataSetConfig.config.itemsTemplate.metaData,
                    &newPubSubDataSetField->dataSetMetaData) != UA_STATUSCODE_GOOD){
                UA_Server_removeDataSetField(server, newPubSubDataSetField->identifier);
                result.addResult = UA_STATUSCODE_BADINTERNALERROR;
            }
            break;
        case UA_PUBSUB_DATASET_PUBLISHEDEVENTS_TEMPLATE:
            if(UA_DataSetMetaDataType_copy(&tmpPublishedDataSetConfig.config.eventTemplate.metaData,
                    &newPubSubDataSetField->dataSetMetaData) != UA_STATUSCODE_GOOD){
                UA_Server_removeDataSetField(server, newPubSubDataSetField->identifier);
                result.addResult = UA_STATUSCODE_BADINTERNALERROR;
            }
            break;
        case UA_PUBSUB_DATASET_PUBLISHEDEVENTS:
            newPubSubDataSetField->dataSetMetaData.configurationVersion.majorVersion = UA_PubSubConfigurationVersionTimeDifference();
            newPubSubDataSetField->dataSetMetaData.configurationVersion.minorVersion = UA_PubSubConfigurationVersionTimeDifference();
            newPubSubDataSetField->dataSetMetaData.dataSetClassId = UA_GUID_NULL;
            if(UA_String_copy(&tmpPublishedDataSetConfig.name, &newPubSubDataSetField->dataSetMetaData.name) != UA_STATUSCODE_GOOD){
                UA_Server_removeDataSetField(server, newPubSubDataSetField->identifier);
                result.addResult = UA_STATUSCODE_BADINTERNALERROR;
            }
            newPubSubDataSetField->dataSetMetaData.description = UA_LOCALIZEDTEXT_ALLOC("", "");
            break;
        case UA_PUBSUB_DATASET_PUBLISHEDITEMS:
            newPubSubDataSetField->dataSetMetaData.configurationVersion.majorVersion = UA_PubSubConfigurationVersionTimeDifference();
            newPubSubDataSetField->dataSetMetaData.configurationVersion.minorVersion = UA_PubSubConfigurationVersionTimeDifference();
            if(UA_String_copy(&tmpPublishedDataSetConfig.name, &newPubSubDataSetField->dataSetMetaData.name) != UA_STATUSCODE_GOOD){
                UA_Server_removeDataSetField(server, newPubSubDataSetField->identifier);
                result.addResult = UA_STATUSCODE_BADINTERNALERROR;
            }
            newPubSubDataSetField->dataSetMetaData.description = UA_LOCALIZEDTEXT_ALLOC("", "");
            newPubSubDataSetField->dataSetMetaData.dataSetClassId = UA_GUID_NULL;
            break;
    }

    server->pubSubManager.publishedDataSetsSize++;
    result.configurationVersion.majorVersion = UA_PubSubConfigurationVersionTimeDifference();
    result.configurationVersion.minorVersion = UA_PubSubConfigurationVersionTimeDifference();
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    addPublishedDataItemsRepresentation(server, newPubSubDataSetField);
#endif
    return result;
}

UA_StatusCode
UA_Server_removePublishedDataSet(UA_Server *server, const UA_NodeId pds) {
    //search the identified PublishedDataSet and store the PDS index
    UA_PublishedDataSet *publishedDataSet = UA_PublishedDataSet_findPDSbyId(server, pds);
    if(!publishedDataSet){
        return UA_STATUSCODE_BADNOTFOUND;
    }
    if(publishedDataSet->configurationFrozen){
        UA_LOG_WARNING(&server->config.logger, UA_LOGCATEGORY_SERVER,
                       "Remove PublishedDataSet failed. PublishedDataSet is frozen.");
        return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    //search for referenced writers -> delete this writers. (Standard: writer must be connected with PDS)
    UA_PubSubConnection *tmpConnectoin;
    TAILQ_FOREACH(tmpConnectoin, &server->pubSubManager.connections, listEntry){
        UA_WriterGroup *writerGroup;
        LIST_FOREACH(writerGroup, &tmpConnectoin->writerGroups, listEntry){
            UA_DataSetWriter *currentWriter, *tmpWriterGroup;
            LIST_FOREACH_SAFE(currentWriter, &writerGroup->writers, listEntry, tmpWriterGroup){
                if(UA_NodeId_equal(&currentWriter->connectedDataSet, &publishedDataSet->identifier)){
                    UA_Server_removeDataSetWriter(server, currentWriter->identifier);
                }
            }
        }
    }
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL
    removePublishedDataSetRepresentation(server, publishedDataSet);
#endif
    UA_PublishedDataSet_clear(server, publishedDataSet);
    server->pubSubManager.publishedDataSetsSize--;

    TAILQ_REMOVE(&server->pubSubManager.publishedDataSets, publishedDataSet, listEntry);
    UA_free(publishedDataSet);
    return UA_STATUSCODE_GOOD;
}

/* Calculate the time difference between current time and UTC (00:00) on January
 * 1, 2000. */
UA_UInt32
UA_PubSubConfigurationVersionTimeDifference() {
    UA_UInt32 timeDiffSince2000 = (UA_UInt32) (UA_DateTime_now() - UA_DATETIMESTAMP_2000);
    return timeDiffSince2000;
}

/* Generate a new unique NodeId. This NodeId will be used for the information
 * model representation of PubSub entities. */
void
UA_PubSubManager_generateUniqueNodeId(UA_Server *server, UA_NodeId *nodeId) {
    UA_NodeId newNodeId = UA_NODEID_NUMERIC(0, 0);
    UA_Node *newNode = UA_NODESTORE_NEW(server, UA_NODECLASS_OBJECT);
    UA_NODESTORE_INSERT(server, newNode, &newNodeId);
    UA_NodeId_copy(&newNodeId, nodeId);
}

/* Delete the current PubSub configuration including all nested members. This
 * action also delete the configured PubSub transport Layers. */
void
UA_PubSubManager_delete(UA_Server *server, UA_PubSubManager *pubSubManager) {
    UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER, "PubSub cleanup was called.");

    /* Stop and unfreeze all WriterGroups */
    UA_PubSubConnection *tmpConnection;
    TAILQ_FOREACH(tmpConnection, &server->pubSubManager.connections, listEntry){
        for(size_t i = 0; i < pubSubManager->connectionsSize; i++) {
            UA_WriterGroup *writerGroup;
            LIST_FOREACH(writerGroup, &tmpConnection->writerGroups, listEntry) {
                UA_WriterGroup_setPubSubState(server, UA_PUBSUBSTATE_DISABLED, writerGroup);
                UA_Server_unfreezeWriterGroupConfiguration(server, writerGroup->identifier);
            }
        }
    }

    //free the currently configured transport layers
    if (server->config.pubsubTransportLayersSize > 0) {
        UA_free(server->config.pubsubTransportLayers);
        server->config.pubsubTransportLayersSize = 0;
    }

    //remove Connections and WriterGroups
    UA_PubSubConnection *tmpConnection1, *tmpConnection2;
    TAILQ_FOREACH_SAFE(tmpConnection1, &server->pubSubManager.connections, listEntry, tmpConnection2){
        UA_Server_removePubSubConnection(server, tmpConnection1->identifier);
    }
    UA_PublishedDataSet *tmpPDS1, *tmpPDS2;
    TAILQ_FOREACH_SAFE(tmpPDS1, &server->pubSubManager.publishedDataSets, listEntry, tmpPDS2){
        UA_Server_removePublishedDataSet(server, tmpPDS1->identifier);
    }
}

void 
UA_PubSubManager_reflectionCallback(UA_Server *server, void *data) {

    /* check message receive timeout for all DataSetReaders */
    UA_PubSubConnection *connection = 0;
    TAILQ_FOREACH(connection, &server->pubSubManager.connections, listEntry) {
        UA_ReaderGroup *readerGroup = 0;
        LIST_FOREACH(readerGroup, &(connection->readerGroups), listEntry) {
            UA_DataSetReader *datasetReader = 0;
            LIST_FOREACH(datasetReader, &(readerGroup->readers), listEntry) {

                if (datasetReader->firstMsgReceived == UA_TRUE) {
                    /* check the MessageReceiveTimeout of every DataSetReader: max acceptable time between two DataSetMessages */
                    if (datasetReader->tSinceLastMsg >= datasetReader->config.messageReceiveTimeout) {
                        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, "DataSetReader %.*s: MessageReceiveTimeout reached", 
                            (int) datasetReader->config.name.length, datasetReader->config.name.data);
                        UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_ERROR, datasetReader);
                        /* TODO: set target variables state, handle override values etc. 
                            -> see OpcUa spec: Message output to target mapping */
                    } else {
                        /* TODO: we should set the state back from ERROR to OPERATIONAL? if we start receiving again ...
                                what happens if the user/application disables the readerGroup in the meantime?  */

                        UA_DataSetReader_setPubSubState(server, UA_PUBSUBSTATE_OPERATIONAL, datasetReader);
                    }
                }
            }
        }
    }

}

/* TODO: we only support integer intervals */
/* we think we need a reflection callback for every WriterGroup 
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

static const UA_Double ReflectionCbDefaultIntervalms = 5.0;
UA_StatusCode
UA_Server_calcPubSubReflectionCbInterval(UA_Server *server, UA_Duration *interval) {

    if ((!server) || (!interval)) {
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    UA_UInt32 tmpInterval = (UA_UInt32) ReflectionCbDefaultIntervalms;

    /* get the MessageReceivetimeout of every DataSetReader and calculate 
        common reflection callback interval */
    UA_PubSubConnection *connection = 0;
    TAILQ_FOREACH(connection, &server->pubSubManager.connections, listEntry) {
        UA_ReaderGroup *readerGroup = 0;
        LIST_FOREACH(readerGroup, &(connection->readerGroups), listEntry) {

            if (readerGroup->readersCount == 0) {
                UA_LOG_INFO(&server->config.logger, UA_LOGCATEGORY_SERVER, 
                    "UA_Server_calcPubSubReflectionCbInterval(): There are no DataSetReaders configured");
                return UA_STATUSCODE_GOODNODATA;                        
            } else if (readerGroup->readersCount == 1) {
                tmpInterval = (UA_UInt32) readerGroup->readers.lh_first->config.messageReceiveTimeout;
            } else {
                /* calculate gcd of all MessageReceiveTimeouts */
                UA_STACKARRAY(UA_UInt32, Timeouts, readerGroup->readersCount);
                UA_UInt32 i = 0;
                UA_DataSetReader *datasetReader = 0;
                LIST_FOREACH(datasetReader, &(readerGroup->readers), listEntry) {
                    Timeouts[i] = (UA_UInt32) datasetReader->config.messageReceiveTimeout;
                    i++;
                }

                /* TODO: check if any msgreceivetimeout is 0 -> not sure if this can work */

                
                /* TODO: not correct */
                if (gcd_arr(Timeouts, readerGroup->readersCount, &tmpInterval) != UA_STATUSCODE_GOOD) {

                    /* TODO: */
                }
            }
        }
    }
    *interval = (UA_Duration) tmpInterval;
    return UA_STATUSCODE_GOOD;
}

/***********************************/
/*      PubSub Jobs abstraction    */
/***********************************/

#ifndef UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_HANDLING

/* If UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_HANDLING is enabled, a custom callback
 * management must be linked to the application */

/* update PubSub reflection callback interval */
static UA_StatusCode UA_PubSubManager_updateReflectionCallbackInterval(UA_Server *server) {
    if (!server) {
        return UA_STATUSCODE_BADINVALIDARGUMENT;
    }

    UA_Duration interval = 0.0;      
    UA_StatusCode ret = UA_Server_calcPubSubReflectionCbInterval(server, &interval);
    if (ret != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, 
        "UA_PubSubManager_addRepeatedCallback(): Calculating new PubSub reflection callback interval failed");
        return ret;
    }
    ret = UA_Timer_changeRepeatedCallbackInterval(&server->timer, server->pubSubManager.reflectionCallbackId, 
        interval);
    if (ret != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(&server->config.logger, UA_LOGCATEGORY_SERVER, 
        "UA_PubSubManager_addRepeatedCallback(): Updating PubSub reflection callback interval failed");
    }
    return ret;
}

UA_StatusCode
UA_PubSubManager_addRepeatedCallback(UA_Server *server, UA_ServerCallback callback,
                                     void *data, UA_Double interval_ms, UA_UInt64 *callbackId) {
    UA_StatusCode ret = UA_PubSubManager_updateReflectionCallbackInterval(server);
    if (ret == UA_STATUSCODE_GOOD) {
        return UA_Timer_addRepeatedCallback(&server->timer, (UA_ApplicationCallback)callback,
                                        server, data, interval_ms, callbackId);
    }
    return ret;
}

UA_StatusCode
UA_PubSubManager_changeRepeatedCallbackInterval(UA_Server *server, UA_UInt64 callbackId,
                                                UA_Double interval_ms) {
    UA_StatusCode ret = UA_PubSubManager_updateReflectionCallbackInterval(server);
    if (ret == UA_STATUSCODE_GOOD) {
        return UA_Timer_changeRepeatedCallbackInterval(&server->timer, callbackId, interval_ms);
    }
    return ret;
}

void
UA_PubSubManager_removeRepeatedPubSubCallback(UA_Server *server, UA_UInt64 callbackId) {
    UA_PubSubManager_updateReflectionCallbackInterval(server);
    UA_Timer_removeCallback(&server->timer, callbackId);
}

UA_StatusCode
UA_PubSubManager_addReflectionCallback(UA_Server *server, UA_ServerCallback callback, 
    UA_Double interval_ms, UA_UInt64 *callbackId) {
    return UA_Timer_addRepeatedCallback(&server->timer, (UA_ApplicationCallback) callback,
            server, 0, interval_ms, callbackId);
}

void
UA_PubSubManager_removeReflectionCallback(UA_Server *server, UA_UInt64 callbackId) {
    UA_Timer_removeCallback(&server->timer, callbackId);
}

#endif /* UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_HANDLING */

#endif /* UA_ENABLE_PUBSUB */
