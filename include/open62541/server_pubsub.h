/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2018 Fraunhofer IOSB (Author: Andreas Ebner)
 * Copyright (c) 2019 Kalycito Infotech Private Limited
 */

#ifndef UA_SERVER_PUBSUB_H
#define UA_SERVER_PUBSUB_H

#include <open62541/util.h>
#include <open62541/types.h>
#include <open62541/types_generated.h>

_UA_BEGIN_DECLS

#ifdef UA_ENABLE_PUBSUB

/**
 * .. _pubsub:
 *
 * Publish/Subscribe
 * =================
 *
 * Work in progress!
 * This part will be a new chapter later.
 *
 * In PubSub the participating OPC UA Applications take their roles as Publishers and Subscribers. Publishers are the
 * sources of data, while Subscribers consume that data. Communication in PubSub is message-based.
 * Publishers send messages to a Message Oriented Middleware, without knowledge of what, if any, Subscribers there may be.
 * Similarly, Subscribers express interest in specific types of data, and process messages that contain this data,
 * without knowledge of what Publishers there are.
 *
 * Message Oriented Middleware is software or hardware infrastructure that supports sending and receiving messages between distributed systems.
 * OPC UA PubSub supports two different Message Oriented Middleware variants, namely the broker-less form and broker-based form.
 * A broker-less form is where the Message Oriented Middleware is the network infrastructure that is able to route datagram-based messages.
 * Subscribers and Publishers use datagram protocols like UDP. In a broker-based form, the core component of the Message Oriented Middleware
 * is a message Broker. Subscribers and Publishers use standard messaging protocols like AMQP or MQTT to communicate with the Broker.
 *
 * This makes PubSub suitable for applications where location independence and/or scalability are required.
 *
 *
 * The Publish/Subscribe (PubSub) extension for OPC UA enables fast and efficient
 * 1:m communication. The PubSub extension is protocol agnostic and can be used
 * with broker based protocols like MQTT and AMQP or brokerless implementations like UDP-Multicasting.
 *
 * The PubSub API uses the following scheme:
 *
 * 1. Create a configuration for the needed PubSub element.
 *
 * 2. Call the add[element] function and pass in the configuration.
 *
 * 3. The add[element] function returns the unique nodeId of the internally created element.
 *
 * Take a look on the PubSub Tutorials for more details about the API usage.::
 *
 *  +-----------+
 *  | UA_Server |
 *  +-----------+
 *   |    |
 *   |    |
 *   |    |
 *   |    |  +----------------------+
 *   |    +--> UA_PubSubConnection  |  UA_Server_addPubSubConnection
 *   |       +----------------------+
 *   |        |    |
 *   |        |    |    +----------------+
 *   |        |    +----> UA_WriterGroup |  UA_PubSubConnection_addWriterGroup
 *   |        |         +----------------+
 *   |        |              |
 *   |        |              |    +------------------+
 *   |        |              +----> UA_DataSetWriter |  UA_WriterGroup_addDataSetWriter     +-+
 *   |        |                   +------------------+                                        |
 *   |        |                                                                               |
 *   |        |         +----------------+                                                    | r
 *   |        +---------> UA_ReaderGroup |    UA_PubSubConnection_addReaderGroup              | e
 *   |                  +----------------+                                                    | f
 *   |                       |                                                                |
 *   |                       |    +------------------+                                        |
 *   |                       +----> UA_DataSetReader |  UA_ReaderGroup_addDataSetReader       |
 *   |                            +------------------+                                        |
 *   |                                 |                                                      |
 *   |                                 |    +----------------------+                          |
 *   |                                 +----> UA_SubscribedDataSet |                          |
 *   |                                      +----------------------+                          |
 *   |                                           |                                            |
 *   |                                           |    +----------------------------+          |
 *   |                                           +----> UA_TargetVariablesDataType |          |
 *   |                                           |    +----------------------------+          |
 *   |                                           |                                            |
 *   |                                           |    +------------------------------------+  |
 *   |                                           +----> UA_SubscribedDataSetMirrorDataType |  |
 *   |                                                +------------------------------------+  |
 *   |                                                                                        |
 *   |       +---------------------------+                                                    |
 *   +-------> UA_PubSubPublishedDataSet |  UA_Server_addPublishedDataSet                   <-+
 *           +---------------------------+
 *                 |
 *                 |    +-----------------+
 *                 +----> UA_DataSetField |  UA_PublishedDataSet_addDataSetField
 *                      +-----------------+
 *
 * PubSub compile flags
 * --------------------
 *
 * **UA_ENABLE_PUBSUB**
 *  Enable the experimental OPC UA PubSub support. The option will include the PubSub UDP multicast plugin. Disabled by default.
 * **UA_ENABLE_PUBSUB_DELTAFRAMES**
 *  The PubSub messages differentiate between keyframe (all published values contained) and deltaframe (only changed values contained) messages.
 *  Deltaframe messages creation consumes some additional ressources and can be disabled with this flag. Disabled by default.
 *  Compile the human-readable name of the StatusCodes into the binary. Disabled by default.
 * **UA_ENABLE_PUBSUB_FILE_CONFIG**
 *  Enable loading OPC UA PubSub configuration from File/ByteString. Enabling PubSub informationmodel methods also will add a method to the Publish/Subscribe object which allows configuring PubSub at runtime.
 * **UA_ENABLE_PUBSUB_INFORMATIONMODEL**
 *  Enable the information model representation of the PubSub configuration. For more details take a look at the following section `PubSub Information Model Representation`. Disabled by default.
 * **UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_HANDLING**
 *  Enable the OPC UA PubSub support with custom callback implementation for Publisher and Subscriber. The option will provide flexibility to use the user defined callback mechanism for sending
 *  packets in Publisher and receiving packets in the Subscriber. Disabled by default.
* **UA_ENABLE_PUBSUB_CUSTOM_TIMEOUT_HANDLING**
 *  Use a custom timer callback implementation for Publisher and Subscriber timeouts. 
 *  This options enables realtime applications to link a custom timer implementation for PubSub timesout handling (e.g. DataSetReader MessageReceiveTimeout).
 *  Disabled by default.
 * **UA_ENABLE_PUBSUB_ETH_UADP**
 *  Enable the OPC UA Ethernet PubSub support to transport UADP NetworkMessages as payload of Ethernet II frame without IP or UDP headers. This option will include Publish and Subscribe based on
 *  EtherType B62C. Disabled by default.
 * **UA_ENABLE_PUBSUB_ETH_UADP_ETF**
 *  Enable ETF feature to allow the user to transmit packets at calculated transmission time with nanosecond precision, in addition to the PubSub support to transport UADP NetworkMessages as payload of Ethernet II frame.
 *  Disabled by default.
 * **UA_ENABLE_PUBSUB_ETH_UADP_XDP**
 *  Enable XDP feature to allow the user to receive packets using the eXpress Data Path (XDP), which bypasses TCP/IP layers and transfers the frames from hardware/netdev to user application thereby reducing the receiving time,
 *  in addition to the PubSub support to transport UADP NetworkMessages as payload of Ethernet II frame. Disabled by default.
 *
 * PubSub Information Model Representation
 * ---------------------------------------
 * .. _pubsub_informationmodel:
 *
 * The complete PubSub configuration is available inside the information model.
 * The entry point is the node 'PublishSubscribe, located under the Server node.
 * The standard defines for PubSub no new Service set. The configuration can optionally
 * done over methods inside the information model. The information model representation
 * of the current PubSub configuration is generated automatically. This feature
 * can enabled/disable by changing the UA_ENABLE_PUBSUB_INFORMATIONMODEL option.
 *
 * Connections
 * -----------
 * The PubSub connections are the abstraction between the concrete transport protocol
 * and the PubSub functionality. It is possible to create multiple connections with
 * different transport protocols at runtime.
 *
 * Take a look on the PubSub Tutorials for mor details about the API usage.
 */

typedef enum {
    UA_PUBSUB_PUBLISHERID_NUMERIC,
    UA_PUBSUB_PUBLISHERID_STRING
} UA_PublisherIdType;

#ifdef UA_ENABLE_PUBSUB_ETH_UADP_ETF
typedef struct {
    UA_Int32 socketPriority;
    UA_Boolean sotxtimeEnabled;
    /* SO_TXTIME-specific additional socket config */
    UA_Int32 sotxtimeDeadlinemode;
    UA_Int32 sotxtimeReceiveerrors;
} UA_ETFConfiguration;
#endif

typedef struct {
    UA_String name;
    UA_Boolean enabled;
    UA_PublisherIdType publisherIdType;
    union { /* std: valid types UInt or String */
        UA_UInt32 numeric;
        UA_String string;
    } publisherId;
    UA_String transportProfileUri;
    UA_Variant address;
    size_t connectionPropertiesSize;
    UA_KeyValuePair *connectionProperties;
    UA_Variant connectionTransportSettings;
#ifdef UA_ENABLE_PUBSUB_ETH_UADP_ETF
    /* ETF related connection configuration - Not in PubSub specfication */
    UA_ETFConfiguration etfConfiguration;
#endif
} UA_PubSubConnectionConfig;

UA_StatusCode UA_EXPORT
UA_Server_addPubSubConnection(UA_Server *server,
                              const UA_PubSubConnectionConfig *connectionConfig,
                              UA_NodeId *connectionIdentifier);

/* Returns a deep copy of the config */
UA_StatusCode UA_EXPORT
UA_Server_getPubSubConnectionConfig(UA_Server *server,
                                    const UA_NodeId connection,
                                    UA_PubSubConnectionConfig *config);

/* Remove Connection, identified by the NodeId. Deletion of Connection
 * removes all contained WriterGroups and Writers. */
UA_StatusCode UA_EXPORT
UA_Server_removePubSubConnection(UA_Server *server, const UA_NodeId connection);

/**
 * PublishedDataSets
 * -----------------
 * The PublishedDataSets (PDS) are containers for the published information. The
 * PDS contain the published variables and meta informations. The metadata is
 * commonly autogenerated or given as constant argument as part of the template
 * functions. The template functions are standard defined and intended for
 * configuration tools. You should normally create a empty PDS and call the
 * functions to add new fields. */

/* The UA_PUBSUB_DATASET_PUBLISHEDITEMS has currently no additional members and
 * thus no dedicated config structure. */

typedef enum {
    UA_PUBSUB_DATASET_PUBLISHEDITEMS,
    UA_PUBSUB_DATASET_PUBLISHEDEVENTS,
    UA_PUBSUB_DATASET_PUBLISHEDITEMS_TEMPLATE,
    UA_PUBSUB_DATASET_PUBLISHEDEVENTS_TEMPLATE,
} UA_PublishedDataSetType;

typedef struct {
    UA_DataSetMetaDataType metaData;
    size_t variablesToAddSize;
    UA_PublishedVariableDataType *variablesToAdd;
} UA_PublishedDataItemsTemplateConfig;

typedef struct {
    UA_NodeId eventNotfier;
    UA_ContentFilter filter;
} UA_PublishedEventConfig;

typedef struct {
    UA_DataSetMetaDataType metaData;
    UA_NodeId eventNotfier;
    size_t selectedFieldsSize;
    UA_SimpleAttributeOperand *selectedFields;
    UA_ContentFilter filter;
} UA_PublishedEventTemplateConfig;

/* Configuration structure for PublishedDataSet */
typedef struct {
    UA_String name;
    UA_PublishedDataSetType publishedDataSetType;
    union {
        /* The UA_PUBSUB_DATASET_PUBLISHEDITEMS has currently no additional members
         * and thus no dedicated config structure.*/
        UA_PublishedDataItemsTemplateConfig itemsTemplate;
        UA_PublishedEventConfig event;
        UA_PublishedEventTemplateConfig eventTemplate;
    } config;
} UA_PublishedDataSetConfig;

void UA_EXPORT
UA_PublishedDataSetConfig_clear(UA_PublishedDataSetConfig *pdsConfig);

typedef struct {
    UA_StatusCode addResult;
    size_t fieldAddResultsSize;
    UA_StatusCode *fieldAddResults;
    UA_ConfigurationVersionDataType configurationVersion;
} UA_AddPublishedDataSetResult;

UA_AddPublishedDataSetResult UA_EXPORT
UA_Server_addPublishedDataSet(UA_Server *server,
                              const UA_PublishedDataSetConfig *publishedDataSetConfig,
                              UA_NodeId *pdsIdentifier);

/* Returns a deep copy of the config */
UA_StatusCode UA_EXPORT
UA_Server_getPublishedDataSetConfig(UA_Server *server, const UA_NodeId pds,
                                    UA_PublishedDataSetConfig *config);

/* Returns a deep copy of the DataSetMetaData for an specific PDS */
UA_StatusCode UA_EXPORT
UA_Server_getPublishedDataSetMetaData(UA_Server *server, const UA_NodeId pds,
                                      UA_DataSetMetaDataType *metaData);

/* Remove PublishedDataSet, identified by the NodeId. Deletion of PDS removes
 * all contained and linked PDS Fields. Connected WriterGroups will be also
 * removed. */
UA_StatusCode UA_EXPORT
UA_Server_removePublishedDataSet(UA_Server *server, const UA_NodeId pds);

/**
 * DataSetFields
 * -------------
 * The description of published variables is named DataSetField. Each
 * DataSetField contains the selection of one information model node. The
 * DataSetField has additional parameters for the publishing, sampling and error
 * handling process. */

typedef struct{
    UA_ConfigurationVersionDataType configurationVersion;
    UA_String fieldNameAlias;
    UA_Boolean promotedField;
    UA_PublishedVariableDataType publishParameters;

    /* non std. field */
    struct {
        UA_Boolean rtFieldSourceEnabled;
        /* If the rtInformationModelNode is set, the nodeid in publishParameter must point
         * to a node with external data source backend defined
         * */
        UA_Boolean rtInformationModelNode;
        //TODO -> decide if suppress C++ warnings and use 'UA_DataValue * * const staticValueSource;'
        UA_DataValue ** staticValueSource;
    } rtValueSource;


} UA_DataSetVariableConfig;

typedef enum {
    UA_PUBSUB_DATASETFIELD_VARIABLE,
    UA_PUBSUB_DATASETFIELD_EVENT
} UA_DataSetFieldType;

typedef struct {
    UA_DataSetFieldType dataSetFieldType;
    union {
        /* events need other config later */
        UA_DataSetVariableConfig variable;
    } field;
} UA_DataSetFieldConfig;

void UA_EXPORT
UA_DataSetFieldConfig_clear(UA_DataSetFieldConfig *dataSetFieldConfig);

typedef struct {
    UA_StatusCode result;
    UA_ConfigurationVersionDataType configurationVersion;
} UA_DataSetFieldResult;

UA_DataSetFieldResult UA_EXPORT
UA_Server_addDataSetField(UA_Server *server,
                          const UA_NodeId publishedDataSet,
                          const UA_DataSetFieldConfig *fieldConfig,
                          UA_NodeId *fieldIdentifier);

/* Returns a deep copy of the config */
UA_StatusCode UA_EXPORT
UA_Server_getDataSetFieldConfig(UA_Server *server, const UA_NodeId dsf,
                                UA_DataSetFieldConfig *config);

UA_DataSetFieldResult UA_EXPORT
UA_Server_removeDataSetField(UA_Server *server, const UA_NodeId dsf);

/**
 * WriterGroup
 * -----------
 * All WriterGroups are created within a PubSubConnection and automatically
 * deleted if the connection is removed. The WriterGroup is primary used as
 * container for :ref:`dsw` and network message settings. The WriterGroup can be
 * imagined as producer of the network messages. The creation of network
 * messages is controlled by parameters like the publish interval, which is e.g.
 * contained in the WriterGroup. */

typedef enum {
    UA_PUBSUB_ENCODING_BINARY,
    UA_PUBSUB_ENCODING_JSON,
    UA_PUBSUB_ENCODING_UADP
} UA_PubSubEncodingType;

/**
 * WriterGroup
 * -----------
 * The message publishing can be configured for realtime requirements. The RT-levels
 * go along with different requirements. The below listed levels can be configured:
 *
 * UA_PUBSUB_RT_NONE -
 * ---> Description: Default "none-RT" Mode
 * ---> Requirements: -
 * ---> Restrictions: -
 * UA_PUBSUB_RT_DIRECT_VALUE_ACCESS (Preview - not implemented)
 * ---> Description: Normally, the latest value for each DataSetField is read out of the information model. Within this RT-mode, the
 * value source of each field configured as static pointer to an DataValue. The publish cycle won't use call the server read function.
 * ---> Requirements: All fields must be configured with a 'staticValueSource'.
 * ---> Restrictions: -
 * UA_PUBSUB_RT_FIXED_LENGTH (Preview - not implemented)
 * ---> Description: All DataSetFields have a known, non-changing length. The server will pre-generate some
 * buffers and use only memcopy operations to generate requested PubSub packages.
 * ---> Requirements: DataSetFields with variable size can't be used within this mode.
 * ---> Restrictions: The configuration must be frozen and changes are not allowed while the WriterGroup is 'Operational'.
 * UA_PUBSUB_RT_DETERMINISTIC (Preview - not implemented)
 * ---> Description: -
 * ---> Requirements: -
 * ---> Restrictions: -
 *
 * WARNING! For hard real time requirements the underlying system must be rt-capable.
 *
 */
typedef enum {
    UA_PUBSUB_RT_NONE = 0,
    UA_PUBSUB_RT_DIRECT_VALUE_ACCESS = 1,
    UA_PUBSUB_RT_FIXED_SIZE = 2,
    UA_PUBSUB_RT_DETERMINISTIC = 4,
} UA_PubSubRTLevel;

typedef struct {
    UA_String name;
    UA_Boolean enabled;
    UA_UInt16 writerGroupId;
    UA_Duration publishingInterval;
    UA_Double keepAliveTime;
    UA_Byte priority;
    UA_MessageSecurityMode securityMode;
    UA_ExtensionObject transportSettings;
    UA_ExtensionObject messageSettings;
    size_t groupPropertiesSize;
    UA_KeyValuePair *groupProperties;
    UA_PubSubEncodingType encodingMimeType;

    /* non std. config parameter. maximum count of embedded DataSetMessage in
     * one NetworkMessage */
    UA_UInt16 maxEncapsulatedDataSetMessageCount;
    /* non std. field */
    UA_PubSubRTLevel rtLevel;
} UA_WriterGroupConfig;

void UA_EXPORT
UA_WriterGroupConfig_clear(UA_WriterGroupConfig *writerGroupConfig);

/* Add a new WriterGroup to an existing Connection */
UA_StatusCode UA_EXPORT
UA_Server_addWriterGroup(UA_Server *server, const UA_NodeId connection,
                         const UA_WriterGroupConfig *writerGroupConfig,
                         UA_NodeId *writerGroupIdentifier);

/* Returns a deep copy of the config */
UA_StatusCode UA_EXPORT
UA_Server_getWriterGroupConfig(UA_Server *server, const UA_NodeId writerGroup,
                               UA_WriterGroupConfig *config);

UA_StatusCode UA_EXPORT
UA_Server_updateWriterGroupConfig(UA_Server *server, UA_NodeId writerGroupIdentifier,
                                  const UA_WriterGroupConfig *config);

/* Get state of WriterGroup */
UA_StatusCode UA_EXPORT
UA_Server_WriterGroup_getState(UA_Server *server, UA_NodeId writerGroupIdentifier,
                               UA_PubSubState *state);

UA_StatusCode UA_EXPORT
UA_Server_removeWriterGroup(UA_Server *server, const UA_NodeId writerGroup);

UA_StatusCode UA_EXPORT
UA_Server_freezeWriterGroupConfiguration(UA_Server *server, const UA_NodeId writerGroup);

UA_StatusCode UA_EXPORT
UA_Server_unfreezeWriterGroupConfiguration(UA_Server *server, const UA_NodeId writerGroup);

UA_StatusCode UA_EXPORT
UA_Server_setWriterGroupOperational(UA_Server *server, const UA_NodeId writerGroup);

UA_StatusCode UA_EXPORT
UA_Server_setWriterGroupDisabled(UA_Server *server, const UA_NodeId writerGroup);

/**
 * .. _dsw:
 *
 * DataSetWriter
 * -------------
 * The DataSetWriters are the glue between the WriterGroups and the
 * PublishedDataSets. The DataSetWriter contain configuration parameters and
 * flags which influence the creation of DataSet messages. These messages are
 * encapsulated inside the network message. The DataSetWriter must be linked
 * with an existing PublishedDataSet and be contained within a WriterGroup. */

typedef struct {
    UA_String name;
    UA_UInt16 dataSetWriterId;
    UA_DataSetFieldContentMask dataSetFieldContentMask;
    UA_UInt32 keyFrameCount;
    UA_ExtensionObject messageSettings;
    UA_ExtensionObject transportSettings;
    UA_String dataSetName;
    size_t dataSetWriterPropertiesSize;
    UA_KeyValuePair *dataSetWriterProperties;
} UA_DataSetWriterConfig;

void UA_EXPORT
UA_DataSetWriterConfig_clear(UA_DataSetWriterConfig *pdsConfig);

/* Add a new DataSetWriter to a existing WriterGroup. The DataSetWriter must be
 * coupled with a PublishedDataSet on creation.
 *
 * Part 14, 7.1.5.2.1 defines: The link between the PublishedDataSet and
 * DataSetWriter shall be created when an instance of the DataSetWriterType is
 * created. */
UA_StatusCode UA_EXPORT
UA_Server_addDataSetWriter(UA_Server *server,
                           const UA_NodeId writerGroup, const UA_NodeId dataSet,
                           const UA_DataSetWriterConfig *dataSetWriterConfig,
                           UA_NodeId *writerIdentifier);

/* Returns a deep copy of the config */
UA_StatusCode UA_EXPORT
UA_Server_getDataSetWriterConfig(UA_Server *server, const UA_NodeId dsw,
                                 UA_DataSetWriterConfig *config);

/* Get state of DataSetWriter */
UA_StatusCode UA_EXPORT
UA_Server_DataSetWriter_getState(UA_Server *server, UA_NodeId dataSetWriterIdentifier,
                               UA_PubSubState *state);

UA_StatusCode UA_EXPORT
UA_Server_removeDataSetWriter(UA_Server *server, const UA_NodeId dsw);

/**
 * SubscribedDataSet
 * -----------------
 * SubscribedDataSet describes the processing of the received DataSet. SubscribedDataSet defines which field
 * in the DataSet is mapped to which Variable in the OPC UA Application. SubscribedDataSet has two sub-types
 * called the TargetVariablesType and SubscribedDataSetMirrorType.
 * SubscribedDataSetMirrorType is currently not supported. SubscribedDataSet is set to TargetVariablesType
 * and then the list of target Variables are created in the Subscriber AddressSpace.
 * TargetVariables are a list of variables that are to be added in the Subscriber AddressSpace.
 * It defines a list of Variable mappings between received DataSet fields and added Variables
 * in the Subscriber AddressSpace. */

/* SubscribedDataSetDataType Definition */
typedef enum {
    UA_PUBSUB_SDS_TARGET,
    UA_PUBSUB_SDS_MIRROR
} UA_SubscribedDataSetEnumType;

typedef struct {
    /* Standard-defined FieldTargetDataType */
    UA_FieldTargetDataType targetVariable;

    /* If realtime-handling is required, set this pointer non-NULL and it will be used
     * to memcpy the value instead of using the Write service.
     * If the afterWrite method pointer is set, it will be called after a memcpy update
     * to the value. */
    UA_DataValue **externalDataValue;
    void *targetVariableContext; /* user-defined pointer */
    void (*afterWrite)(UA_Server *server,
                       const UA_NodeId *readerIdentifier,
                       const UA_NodeId *readerGroupIdentifier,
                       const UA_NodeId *targetVariableIdentifier,
                       void *targetVariableContext,
                       UA_DataValue **externalDataValue);
} UA_FieldTargetVariable;

typedef struct {
    size_t targetVariablesSize;
    UA_FieldTargetVariable *targetVariables;
} UA_TargetVariables;

/* Return Status Code after creating TargetVariables in Subscriber AddressSpace */
UA_StatusCode UA_EXPORT
UA_Server_DataSetReader_createTargetVariables(UA_Server *server,
                                              UA_NodeId dataSetReaderIdentifier,
                                              size_t targetVariablesSize,
                                              const UA_FieldTargetVariable *targetVariables);

/* To Do:Implementation of SubscribedDataSetMirrorType
 * UA_StatusCode
 * A_PubSubDataSetReader_createDataSetMirror(UA_Server *server, UA_NodeId dataSetReaderIdentifier,
 * UA_SubscribedDataSetMirrorDataType* mirror) */

/**
 * DataSetReader
 * -------------
 * DataSetReader can receive NetworkMessages with the DataSetMessage
 * of interest sent by the Publisher. DataSetReaders represent
 * the configuration necessary to receive and process DataSetMessages
 * on the Subscriber side. DataSetReader must be linked with a
 * SubscribedDataSet and be contained within a ReaderGroup. */

/* Parameters for PubSubSecurity */
typedef struct {
    UA_Int32 securityMode;          /* placeholder datatype 'MessageSecurityMode' */
    UA_String securityGroupId;
    size_t keyServersSize;
    UA_Int32 *keyServers;
} UA_PubSubSecurityParameters;

/* Parameters for PubSub DataSetReader Configuration */
typedef struct {
    UA_String name;
    UA_Variant publisherId;
    UA_UInt16 writerGroupId;
    UA_UInt16 dataSetWriterId;
    UA_DataSetMetaDataType dataSetMetaData;
    UA_DataSetFieldContentMask dataSetFieldContentMask;
    UA_Double messageReceiveTimeout;
    UA_PubSubSecurityParameters securityParameters;
    UA_ExtensionObject messageSettings;
    UA_ExtensionObject transportSettings;
    UA_SubscribedDataSetEnumType subscribedDataSetType;
    /* TODO UA_SubscribedDataSetMirrorDataType subscribedDataSetMirror */
    union {
        UA_TargetVariables subscribedDataSetTarget;
        // UA_SubscribedDataSetMirrorDataType subscribedDataSetMirror;
    } subscribedDataSet;
} UA_DataSetReaderConfig;

/* Update configuration to the dataSetReader */
UA_StatusCode UA_EXPORT
UA_Server_DataSetReader_updateConfig(UA_Server *server, UA_NodeId dataSetReaderIdentifier,
                                     UA_NodeId readerGroupIdentifier,
                                     const UA_DataSetReaderConfig *config);

/* Get configuration of the dataSetReader */
UA_StatusCode UA_EXPORT
UA_Server_DataSetReader_getConfig(UA_Server *server, UA_NodeId dataSetReaderIdentifier,
                                  UA_DataSetReaderConfig *config);

/* Get state of DataSetReader */
UA_StatusCode UA_EXPORT
UA_Server_DataSetReader_getState(UA_Server *server, UA_NodeId dataSetReaderIdentifier,
                               UA_PubSubState *state);

/**
 * ReaderGroup
 * -----------
 * ReaderGroup is used to group a list of DataSetReaders. All ReaderGroups are
 * created within a PubSubConnection and automatically deleted if the connection
 * is removed. All network message related filters are only available in the DataSetReader.
 *
 * The RT-levels go along with different requirements. The below listed levels can be configured
 * for a ReaderGroup.
 * UA_PUBSUB_RT_NONE --> No RT applied to this level
 * PUBSUB_CONFIG_FASTPATH_FIXED_OFFSETS --> Extends PubSub RT functionality and implements fast path
 * message decoding in the Subscriber. Uses a buffered network message and only decodes the necessary
 * offsets stored in an offset buffer. */

/* ReaderGroup configuration */
typedef struct {
    UA_String name;
    UA_PubSubSecurityParameters securityParameters;
    /* non std. field */
    UA_PubSubRTLevel rtLevel;
} UA_ReaderGroupConfig;

/* Add DataSetReader to the ReaderGroup */
UA_StatusCode UA_EXPORT
UA_Server_addDataSetReader(UA_Server *server, UA_NodeId readerGroupIdentifier,
                                      const UA_DataSetReaderConfig *dataSetReaderConfig,
                                      UA_NodeId *readerIdentifier);

/* Remove DataSetReader from ReaderGroup */
UA_StatusCode UA_EXPORT
UA_Server_removeDataSetReader(UA_Server *server, UA_NodeId readerIdentifier);

/* To Do: Update Configuration of ReaderGroup
 * UA_StatusCode UA_EXPORT
 * UA_Server_ReaderGroup_updateConfig(UA_Server *server, UA_NodeId readerGroupIdentifier,
 *                                    const UA_ReaderGroupConfig *config);
 */

/* Get configuraiton of ReaderGroup */
UA_StatusCode UA_EXPORT
UA_Server_ReaderGroup_getConfig(UA_Server *server, UA_NodeId readerGroupIdentifier,
                               UA_ReaderGroupConfig *config);

/* Get state of ReaderGroup */
UA_StatusCode UA_EXPORT
UA_Server_ReaderGroup_getState(UA_Server *server, UA_NodeId readerGroupIdentifier,
                               UA_PubSubState *state);

/* Add ReaderGroup to the created connection */
UA_StatusCode UA_EXPORT
UA_Server_addReaderGroup(UA_Server *server, UA_NodeId connectionIdentifier,
                                   const UA_ReaderGroupConfig *readerGroupConfig,
                                   UA_NodeId *readerGroupIdentifier);

/* Remove ReaderGroup from connection */
UA_StatusCode UA_EXPORT
UA_Server_removeReaderGroup(UA_Server *server, UA_NodeId groupIdentifier);

UA_StatusCode UA_EXPORT
UA_Server_freezeReaderGroupConfiguration(UA_Server *server, const UA_NodeId readerGroupId);

UA_StatusCode UA_EXPORT
UA_Server_unfreezeReaderGroupConfiguration(UA_Server *server, const UA_NodeId readerGroupId);

UA_StatusCode UA_EXPORT
UA_Server_setReaderGroupOperational(UA_Server *server, const UA_NodeId readerGroupId);

UA_StatusCode UA_EXPORT
UA_Server_setReaderGroupDisabled(UA_Server *server, const UA_NodeId readerGroupId);

#endif /* UA_ENABLE_PUBSUB */

_UA_END_DECLS

#endif /* UA_SERVER_PUBSUB_H */
