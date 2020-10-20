/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2019 Fraunhofer IOSB (Author: Andreas Ebner)
 */

#ifndef UA_PUBSUB_MANAGER_H_
#define UA_PUBSUB_MANAGER_H_

#include <open62541/server_pubsub.h>

#include "ua_pubsub.h"

_UA_BEGIN_DECLS

#ifdef UA_ENABLE_PUBSUB /* conditional compilation */

typedef struct UA_PubSubManager{
    //Connections and PublishedDataSets can exist alone (own lifecycle) -> top level components
    size_t connectionsSize;
    TAILQ_HEAD(UA_ListOfPubSubConnection, UA_PubSubConnection) connections;
    size_t publishedDataSetsSize;
    TAILQ_HEAD(UA_ListOfPublishedDataSet, UA_PublishedDataSet) publishedDataSets;
} UA_PubSubManager;

typedef enum UA_PubSubMonitoringType {
    eMessageReceiveTimeout
    // extend as needed
} UA_PubSubMonitoringType;

void
UA_PubSubManager_delete(UA_Server *server, UA_PubSubManager *pubSubManager);

void
UA_PubSubManager_generateUniqueNodeId(UA_Server *server, UA_NodeId *nodeId);

UA_UInt32
UA_PubSubConfigurationVersionTimeDifference(void);

/***********************************/
/*      PubSub Jobs abstraction    */
/***********************************/
UA_StatusCode
UA_PubSubManager_addRepeatedCallback(UA_Server *server, UA_ServerCallback callback,
                                     void *data, UA_Double interval_ms, UA_UInt64 *callbackId);
UA_StatusCode
UA_PubSubManager_changeRepeatedCallbackInterval(UA_Server *server, UA_UInt64 callbackId,
                                                UA_Double interval_ms);
void
UA_PubSubManager_removeRepeatedPubSubCallback(UA_Server *server, UA_UInt64 callbackId);

/*************************************************/
/*      PubSub component monitoring              */
/*************************************************/

UA_StatusCode
UA_PubSubComponent_createMonitoring(UA_Server *server, void *component, UA_PubSubMonitoringType eMonitoringType, UA_ServerCallback callback);

UA_StatusCode
UA_PubSubComponent_startMonitoring(UA_Server *server, void *component, UA_PubSubMonitoringType eMonitoringType);

UA_StatusCode
UA_PubSubComponent_stopMonitoring(UA_Server *server, void *component, UA_PubSubMonitoringType eMonitoringType);

UA_StatusCode
UA_PubSubComponent_updateMonitoringInterval(UA_Server *server, void *component, UA_PubSubMonitoringType eMonitoringType);

UA_StatusCode
UA_PubSubComponent_deleteMonitoring(UA_Server *server, void *component, UA_PubSubMonitoringType eMonitoringType);

#endif /* UA_ENABLE_PUBSUB */

_UA_END_DECLS

#endif /* UA_PUBSUB_MANAGER_H_ */
