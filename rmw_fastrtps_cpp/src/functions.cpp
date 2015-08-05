#include "rmw/rmw.h"
#include "rmw/error_handling.h"
#include "rosidl_typesupport_introspection_cpp/identifier.hpp"
#include "rmw_fastrtps_cpp/MessageTypeSupport.h"
#include "rmw_fastrtps_cpp/ServiceTypeSupport.h"

#include <fastrtps/Domain.h>
#include <fastrtps/participant/Participant.h>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/publisher/Publisher.h>
#include <fastrtps/attributes/PublisherAttributes.h>
#include <fastrtps/subscriber/Subscriber.h>
#include <fastrtps/subscriber/SubscriberListener.h>
#include <fastrtps/subscriber/SampleInfo.h>
#include <fastrtps/attributes/SubscriberAttributes.h>

/**  New include  **/
#include <fastrtps/rtps/reader/RTPSReader.h>
#include <fastrtps/rtps/participant/RTPSParticipant.h>
#include <fastrtps/rtps/RTPSDomain.h>
#include <fastrtps/rtps/reader/ReaderListener.h>

#include <fastrtps/rtps/attributes/RTPSParticipantAttributes.h>
#include <fastrtps/rtps/attributes/ReaderAttributes.h>
#include <fastrtps/rtps/attributes/HistoryAttributes.h>

#include "fastrtps/rtps/history/ReaderHistory.h"
/**  End include  **/

#include <cassert>
#include <mutex>
#include <condition_variable>
#include <list>

using namespace eprosima::fastrtps;

class ClientListener;

typedef struct CustomClientInfo
{
    rmw_fastrtps_cpp::RequestTypeSupport *request_type_support_;
    rmw_fastrtps_cpp::ResponseTypeSupport *response_type_support_;
    ClientListener *listener_;
} CustomClientInfo;

class ClientListener
{
    public:

        ClientListener(CustomClientInfo *info) : info_(info),
        conditionMutex_(NULL), conditionVariable_(NULL) {}


        void onNewResponse(rmw_fastrtps_cpp::ResponseTypeSupport::ResponseBuffer *buffer)
        {
            assert(buffer);
            std::lock_guard<std::mutex> lock(internalMutex_);

            if(conditionMutex_ != NULL)
            {
                std::unique_lock<std::mutex> clock(*conditionMutex_);
                list.push_back(buffer);
                clock.unlock();
                conditionVariable_->notify_one();
            }
            else
                list.push_back(buffer);

        }

        rmw_fastrtps_cpp::ResponseTypeSupport::ResponseBuffer* getResponse()
        {
            std::lock_guard<std::mutex> lock(internalMutex_);
            rmw_fastrtps_cpp::ResponseTypeSupport::ResponseBuffer *buffer = nullptr;

            if(conditionMutex_ != NULL)
            {
                std::unique_lock<std::mutex> clock(*conditionMutex_);
                buffer = list.front();
                list.pop_front();
            }
            else
            {
                buffer = list.front();
                list.pop_front();
            }

            return buffer;
        }

        void attachCondition(std::mutex *conditionMutex, std::condition_variable *conditionVariable)
        {
            std::lock_guard<std::mutex> lock(internalMutex_);
            conditionMutex_ = conditionMutex;
            conditionVariable_ = conditionVariable;
        }

        void dettachCondition()
        {
            std::lock_guard<std::mutex> lock(internalMutex_);
            conditionMutex_ = NULL;
            conditionVariable_ = NULL;
        }

        bool hasData()
        {
            return !list.empty();
        }

    private:

        CustomClientInfo *info_;
        std::mutex internalMutex_;
        std::list<rmw_fastrtps_cpp::ResponseTypeSupport::ResponseBuffer*> list;
        std::mutex *conditionMutex_;
        std::condition_variable *conditionVariable_;
};


extern "C"
{
    const char* const eprosima_fastrtps_identifier = "fastrtps";

    const char* rmw_get_implementation_identifier()
    {
        return eprosima_fastrtps_identifier;
    }

    rmw_ret_t rmw_init()
    {
        return RMW_RET_OK;
    }

    rmw_node_t* rmw_create_node(const char *name)
    {
        assert(name);

	RTPSParticipantAttributes PParam;
        PParam.builtin.use_SIMPLE_RTPSParticipantDiscoveryProtocol = false;
        PParam.builtin.use_WriterLivelinessProtocol = false;
        RTPSParticipant *participant = RTPSDomain::createParticipant(PParam);

        if(!participant)
        {
            rmw_set_error_string("create_node() could not create participant");
            return NULL;
        }

        rmw_node_t *node_handle = new rmw_node_t;
        node_handle->implementation_identifier = eprosima_fastrtps_identifier;
        node_handle->data = participant;

        return node_handle;
    }

    typedef struct CustomPublisherInfo
    {
        Publisher *publisher_;
        rmw_fastrtps_cpp::MessageTypeSupport *type_support_;
    } CustomPublisherInfo;

    rmw_publisher_t* rmw_create_publisher(const rmw_node_t *node, const rosidl_message_type_support_t *type_support,
            const char* topic_name, size_t queue_size)
    {
        assert(node);
        assert(type_support);
        assert(topic_name);

        if(node->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("node handle not from this implementation");
            return NULL;
        }

        Participant *participant = static_cast<Participant*>(node->data);

        if(strcmp(type_support->typesupport_identifier, rosidl_typesupport_introspection_cpp::typesupport_introspection_identifier) != 0)
        {
            rmw_set_error_string("type support not from this implementation");
            return NULL;
        }

        CustomPublisherInfo *info = new CustomPublisherInfo();

        /*const rosidl_typesupport_introspection_cpp::MessageMembers *members = static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers*>(type_support->data);
        info->type_support_ = new rmw_fastrtps_cpp::MessageTypeSupport(members);

        Domain::registerType(participant, info->type_support_);

        PublisherAttributes publisherParam;
        publisherParam.topic.topicKind = NO_KEY;
        publisherParam.topic.topicDataType = std::string(members->package_name_) + "::dds_::" + members->message_name_ + "_";
        publisherParam.topic.topicName = topic_name;

        info->publisher_ = Domain::createPublisher(participant, publisherParam, NULL);

        if(!info->publisher_)
        {
            rmw_set_error_string("create_publisher() could not create publisher");
            return NULL;
        }*/

        rmw_publisher_t *rmw_publisher = new rmw_publisher_t;
        rmw_publisher->implementation_identifier = eprosima_fastrtps_identifier;
        rmw_publisher->data = info;

        return rmw_publisher;
    }


    rmw_ret_t rmw_publish(const rmw_publisher_t *publisher, const void *ros_message)
    {
        assert(publisher);
        assert(ros_message);
        rmw_ret_t returnedValue = RMW_RET_ERROR;

        if(publisher->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("publisher handle not from this implementation");
            return RMW_RET_ERROR;
        }

        CustomPublisherInfo *info = (CustomPublisherInfo*)publisher->data;
        assert(info);

        rmw_fastrtps_cpp::MessageTypeSupport::Buffer *buffer = (rmw_fastrtps_cpp::MessageTypeSupport::Buffer*)info->type_support_->createData();

        if(info->type_support_->serializeROSmessage(ros_message, buffer))
        {
            if(info->publisher_->write((void*)buffer))
                returnedValue = RMW_RET_OK;
            else
                rmw_set_error_string("cannot publish data");
        }
        else
            rmw_set_error_string("cannot serialize data");

        info->type_support_->deleteData(buffer);

        return returnedValue;
    }

    class SubListener;

    typedef struct CustomSubscriberInfo
    {
	    ReaderHistory *history_;
        RTPSReader *reader_;
        SubListener *listener_;
        rmw_fastrtps_cpp::MessageTypeSupport *type_support_;
    } CustomSubscriberInfo;

    class SubListener : public ReaderListener
    {
        public:

            SubListener(CustomSubscriberInfo *info) : info_(info), hasData_(0),
            conditionMutex_(NULL), conditionVariable_(NULL) {}

            void onNewCacheChangeAdded(RTPSReader *reader, const CacheChange_t* const change)
            {
                std::lock_guard<std::mutex> lock(internalMutex_);

                if(change->kind == ALIVE)
                {
                    if(conditionMutex_ != NULL)
                    {
                        std::unique_lock<std::mutex> clock(*conditionMutex_);
                        hasData_++;
                        clock.unlock();
                        conditionVariable_->notify_one();
                    }
                    else
                        hasData_++;
                }

            }

            void attachCondition(std::mutex *conditionMutex, std::condition_variable *conditionVariable)
            {
                std::lock_guard<std::mutex> lock(internalMutex_);
                conditionMutex_ = conditionMutex;
                conditionVariable_ = conditionVariable;
            }

            void dettachCondition()
            {
                std::lock_guard<std::mutex> lock(internalMutex_);
                conditionMutex_ = NULL;
                conditionVariable_ = NULL;
            }

            bool hasData()
            {
                return hasData_ > 0;
            }

            bool getHasData()
            {
                int ret = hasData_;
                if(hasData_ > 0)
                    hasData_--;
                return ret > 0;
            }

        private:

            CustomSubscriberInfo *info_;
            std::mutex internalMutex_;
            int hasData_;
            std::mutex *conditionMutex_;
            std::condition_variable *conditionVariable_;
    };

    rmw_subscription_t* rmw_create_subscription(const rmw_node_t *node, const rosidl_message_type_support_t *type_support,
            const char *topic_name, size_t queue_size, bool ignore_local_publications)
    {
        assert(node);
        assert(type_support);
        assert(topic_name);

        if(node->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("node handle not from this implementation");
            return NULL;
        }

        RTPSParticipant *participant = static_cast<RTPSParticipant*>(node->data);

        if(strcmp(type_support->typesupport_identifier, rosidl_typesupport_introspection_cpp::typesupport_introspection_identifier) != 0)
        {
            rmw_set_error_string("type support not from this implementation");
            return NULL;
        }

        CustomSubscriberInfo *info = new CustomSubscriberInfo();

        const rosidl_typesupport_introspection_cpp::MessageMembers *members = static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers*>(type_support->data);
        info->type_support_ = new rmw_fastrtps_cpp::MessageTypeSupport(members);

        HistoryAttributes hatt;
        hatt.payloadMaxSize = 255;
        info->history_ = new ReaderHistory(hatt);

        //CREATE READER
        ReaderAttributes ratt;
        Locator_t loc;
        std::string ip("239.255.0.1");
        loc.set_IP4_address(ip);
        loc.port = 7400;
        ratt.endpoint.multicastLocatorList.push_back(loc);

        info->listener_ = new SubListener(info);
        info->reader_ = RTPSDomain::createRTPSReader(participant,ratt,info->history_,info->listener_);

        if(!info->reader_)
        {
            rmw_set_error_string("create_subscriber() could not create subscriber");
            return NULL;
        }

        rmw_subscription_t *subscription = new rmw_subscription_t; 
        subscription->implementation_identifier = eprosima_fastrtps_identifier;
        subscription->data = info;

        return subscription;
    }

    rmw_ret_t rmw_take(const rmw_subscription_t *subscription, void *ros_message, bool *taken)
    {
        assert(subscription);
        assert(ros_message);
        assert(taken);

        *taken = false;

        if(subscription->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("publisher handle not from this implementation");
            return RMW_RET_ERROR;
        }

        CustomSubscriberInfo *info = (CustomSubscriberInfo*)subscription->data;
        assert(info);

        rmw_fastrtps_cpp::MessageTypeSupport::Buffer buffer;
        CacheChange_t *change;
        WriterProxy *wp;

        if(info->reader_->nextUntakenCache(&change, &wp))
        {
            change->isRead = true;
            if(change->kind == ALIVE)
            {
                buffer.pointer = (char*)change->serializedPayload.data;
                buffer.length = change->serializedPayload.length;
                info->type_support_->deserializeROSmessage(&buffer, ros_message);
                *taken = true;
            }

	    info->history_->remove_change(change);
        }

        return RMW_RET_OK;
    }

    class GuardCondition
    {
        public:

            GuardCondition() : hasTriggered_(false),
            conditionMutex_(NULL), conditionVariable_(NULL) {}

            void trigger()
            {
                std::lock_guard<std::mutex> lock(internalMutex_);

                if(conditionMutex_ != NULL)
                {
                    std::unique_lock<std::mutex> clock(*conditionMutex_);
                    hasTriggered_ = true;
                    clock.unlock();
                    conditionVariable_->notify_one();
                }
                else
                    hasTriggered_ = true;

            }

            void attachCondition(std::mutex *conditionMutex, std::condition_variable *conditionVariable)
            {
                std::lock_guard<std::mutex> lock(internalMutex_);
                conditionMutex_ = conditionMutex;
                conditionVariable_ = conditionVariable;
            }

            void dettachCondition()
            {
                std::lock_guard<std::mutex> lock(internalMutex_);
                conditionMutex_ = NULL;
                conditionVariable_ = NULL;
            }

            bool hasTriggered()
            {
                return hasTriggered_;
            }

            bool getHasTriggered()
            {
                bool ret = hasTriggered_;
                hasTriggered_ = false;
                return ret;
            }

        private:

            std::mutex internalMutex_;
            bool hasTriggered_;
            std::mutex *conditionMutex_;
            std::condition_variable *conditionVariable_;
    };

    rmw_guard_condition_t* rmw_create_guard_condition()
    {
        rmw_guard_condition_t *guard_condition_handle = new rmw_guard_condition_t;
        guard_condition_handle->implementation_identifier = eprosima_fastrtps_identifier;
        guard_condition_handle->data = new GuardCondition();
        return guard_condition_handle;
    }


    rmw_ret_t rmw_destroy_guard_condition(rmw_guard_condition_t *guard_condition)
    {
        if(guard_condition)
        {
            delete (GuardCondition*)guard_condition->data;
            delete guard_condition;
            return RMW_RET_OK;
        }

        return RMW_RET_ERROR;
    }

    rmw_ret_t rmw_trigger_guard_condition(const rmw_guard_condition_t *guard_condition_handle)
    {
        assert(guard_condition_handle);

        if(guard_condition_handle->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("guard condition handle not from this implementation");
            return RMW_RET_ERROR;
        }

        GuardCondition *guard_condition = (GuardCondition*)guard_condition_handle->data;
        guard_condition->trigger();
        return RMW_RET_OK;
    }

    class ServiceListener;

    typedef struct CustomServiceInfo
    {
        rmw_fastrtps_cpp::RequestTypeSupport *request_type_support_;
        rmw_fastrtps_cpp::ResponseTypeSupport *response_type_support_;
        ServiceListener *listener_;
    } CustomServiceInfo;

    class ServiceListener
    {
        public:

            ServiceListener(CustomServiceInfo *info) : info_(info),
            conditionMutex_(NULL), conditionVariable_(NULL) {}


            void onNewRequest(rmw_fastrtps_cpp::RequestTypeSupport::RequestBuffer *buffer)
            {
                assert(buffer);
                std::lock_guard<std::mutex> lock(internalMutex_);

                if(conditionMutex_ != NULL)
                {
                    std::unique_lock<std::mutex> clock(*conditionMutex_);
                    list.push_back(buffer);
                    clock.unlock();
                    conditionVariable_->notify_one();
                }
                else
                    list.push_back(buffer);

            }

            rmw_fastrtps_cpp::RequestTypeSupport::RequestBuffer* getRequest()
            {
                std::lock_guard<std::mutex> lock(internalMutex_);
                rmw_fastrtps_cpp::RequestTypeSupport::RequestBuffer *buffer = nullptr;

                if(conditionMutex_ != NULL)
                {
                    std::unique_lock<std::mutex> clock(*conditionMutex_);
                    buffer = list.front();
                    list.pop_front();
                }
                else
                {
                    buffer = list.front();
                    list.pop_front();
                }

                return buffer;
            }

            void attachCondition(std::mutex *conditionMutex, std::condition_variable *conditionVariable)
            {
                std::lock_guard<std::mutex> lock(internalMutex_);
                conditionMutex_ = conditionMutex;
                conditionVariable_ = conditionVariable;
            }

            void dettachCondition()
            {
                std::lock_guard<std::mutex> lock(internalMutex_);
                conditionMutex_ = NULL;
                conditionVariable_ = NULL;
            }

            bool hasData()
            {
                return !list.empty();
            }

        private:

            CustomServiceInfo *info_;
            std::mutex internalMutex_;
            std::list<rmw_fastrtps_cpp::RequestTypeSupport::RequestBuffer*> list;
            std::mutex *conditionMutex_;
            std::condition_variable *conditionVariable_;
    };

    rmw_client_t* rmw_create_client(const rmw_node_t *node,
            const rosidl_service_type_support_t *type_support,
            const char *service_name)
    {
        assert(node);
        assert(type_support);
        assert(service_name);

        if(node->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("node handle not from this implementation");
            return NULL;
        }

        Participant *participant = static_cast<Participant*>(node->data);

        if(strcmp(type_support->typesupport_identifier, rosidl_typesupport_introspection_cpp::typesupport_introspection_identifier) != 0)
        {
            rmw_set_error_string("type support not from this implementation");
            return NULL;
        }

        CustomClientInfo *info = new CustomClientInfo();

        const rosidl_typesupport_introspection_cpp::ServiceMembers *members = static_cast<const rosidl_typesupport_introspection_cpp::ServiceMembers*>(type_support->data);
        info->request_type_support_ = new rmw_fastrtps_cpp::RequestTypeSupport(members);
        info->response_type_support_ = new rmw_fastrtps_cpp::ResponseTypeSupport(members);

        Domain::registerType(participant, info->request_type_support_);
        Domain::registerType(participant, info->response_type_support_);

        info->listener_ = new ClientListener(info);

        rmw_client_t *client = new rmw_client_t; 
        client->implementation_identifier = eprosima_fastrtps_identifier;
        client->data = info;

        return client;
    }

    rmw_ret_t rmw_send_request(const rmw_client_t *client,
            const void *ros_request,
            int64_t *sequence_id)
    {
        assert(client);
        assert(ros_request);
        assert(sequence_id);

        rmw_ret_t returnedValue = RMW_RET_ERROR;

        if(client->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("node handle not from this implementation");
            return RMW_RET_ERROR;
        }

        CustomClientInfo *info = (CustomClientInfo*)client->data;
        assert(info);

        rmw_fastrtps_cpp::RequestTypeSupport::RequestBuffer *buffer = static_cast<rmw_fastrtps_cpp::RequestTypeSupport::RequestBuffer*>(info->request_type_support_->createData());

        if(info->request_type_support_->serializeROSmessage(ros_request, buffer))
        {
        }
        else
            rmw_set_error_string("cannot serialize data");

        info->request_type_support_->deleteData(buffer);

        return returnedValue;
    }

    rmw_ret_t rmw_take_request(const rmw_service_t *service,
            void *ros_request_header,
            void *ros_request,
            bool *taken)
    {
        assert(service);
        assert(ros_request_header);
        assert(ros_request);
        assert(taken);

        *taken = false;

        if(service->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("service handle not from this implementation");
            return RMW_RET_ERROR;
        }

        CustomServiceInfo *info = (CustomServiceInfo*)service->data;
        assert(info);

        rmw_fastrtps_cpp::RequestTypeSupport::RequestBuffer *buffer = info->listener_->getRequest();

        if(buffer != nullptr)
        {
            info->request_type_support_->deserializeROSmessage(buffer, ros_request);

            // Get header
            rmw_request_id_t &req_id = *(static_cast<rmw_request_id_t*>(ros_request_header));

            info->request_type_support_->deleteData(buffer);

            *taken = true;
        }

        return RMW_RET_OK;
    }

    rmw_ret_t rmw_take_response(const rmw_client_t *client,
            void *ros_request_header,
            void *ros_response,
            bool *taken)
    {
        assert(client);
        assert(ros_request_header);
        assert(ros_response);
        assert(taken);

        *taken = false;

        if(client->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("service handle not from this implementation");
            return RMW_RET_ERROR;
        }

        CustomClientInfo *info = (CustomClientInfo*)client->data;
        assert(info);

        rmw_request_id_t &req_id = *(static_cast<rmw_request_id_t*>(ros_request_header));

        rmw_fastrtps_cpp::ResponseTypeSupport::ResponseBuffer *buffer = info->listener_->getResponse();

        if(buffer != nullptr)
        {
            info->response_type_support_->deserializeROSmessage(buffer, ros_response);

            *taken = true;

            info->request_type_support_->deleteData(buffer);
        }

        return RMW_RET_OK;
    }

    rmw_ret_t rmw_send_response(const rmw_service_t *service,
            void *ros_request_header,
            void *ros_response)
    {
        assert(service);
        assert(ros_request_header);
        assert(ros_response);

        rmw_ret_t returnedValue = RMW_RET_ERROR;

        if(service->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("service handle not from this implementation");
            return RMW_RET_ERROR;
        }

        CustomServiceInfo *info = (CustomServiceInfo*)service->data;
        assert(info);

        rmw_fastrtps_cpp::ResponseTypeSupport::ResponseBuffer *buffer = static_cast<rmw_fastrtps_cpp::ResponseTypeSupport::ResponseBuffer*>(info->response_type_support_->createData());

        if(buffer != nullptr)
        {
            info->response_type_support_->serializeROSmessage(ros_response, buffer);

            //Set header
            rmw_request_id_t &req_id = *(static_cast<rmw_request_id_t*>(ros_request_header));


            returnedValue = RMW_RET_OK;

        }

        info->response_type_support_->deleteData(buffer);

        return returnedValue;
    }

    rmw_service_t *rmw_create_service(const rmw_node_t *node,
            const rosidl_service_type_support_t *type_support,
            const char *service_name)
    {
        assert(node);
        assert(type_support);
        assert(service_name);

        if(node->implementation_identifier != eprosima_fastrtps_identifier)
        {
            rmw_set_error_string("node handle not from this implementation");
            return NULL;
        }

        Participant *participant = static_cast<Participant*>(node->data);

        if(strcmp(type_support->typesupport_identifier, rosidl_typesupport_introspection_cpp::typesupport_introspection_identifier) != 0)
        {
            rmw_set_error_string("type support not from this implementation");
            return NULL;
        }

        CustomServiceInfo *info = new CustomServiceInfo();

        const rosidl_typesupport_introspection_cpp::ServiceMembers *members = static_cast<const rosidl_typesupport_introspection_cpp::ServiceMembers*>(type_support->data);
        info->request_type_support_ = new rmw_fastrtps_cpp::RequestTypeSupport(members);
        info->response_type_support_ = new rmw_fastrtps_cpp::ResponseTypeSupport(members);

        Domain::registerType(participant, info->request_type_support_);
        Domain::registerType(participant, info->response_type_support_);

        info->listener_ = new ServiceListener(info);

        rmw_service_t *service = new rmw_service_t; 
        service->implementation_identifier = eprosima_fastrtps_identifier;
        service->data = info;

        return service;
    }

    rmw_ret_t rmw_destroy_service(rmw_service_t *service)
    {
        return RMW_RET_ERROR;
    }

    rmw_ret_t rmw_destroy_client(rmw_client_t *client)
    {
        return RMW_RET_ERROR;
    }

    rmw_ret_t rmw_wait(rmw_subscriptions_t *subscriptions,
            rmw_guard_conditions_t *guard_conditions,
            rmw_services_t *services,
            rmw_clients_t *clients,
            bool non_blocking)
    {
        std::mutex conditionMutex;
        std::condition_variable conditionVariable;

        for(unsigned long i = 0; i < subscriptions->subscriber_count; ++i)
        {
            void *data = subscriptions->subscribers[i];
            CustomSubscriberInfo *custom_subscriber_info = (CustomSubscriberInfo*)data;
            custom_subscriber_info->listener_->attachCondition(&conditionMutex, &conditionVariable);
        }

        for(unsigned long i = 0; i < clients->client_count; ++i)
        {
            void *data = clients->clients[i];
            CustomClientInfo *custom_client_info = (CustomClientInfo*)data;
            custom_client_info->listener_->attachCondition(&conditionMutex, &conditionVariable);
        }

        for(unsigned long i = 0; i < services->service_count; ++i)
        {
            void *data = services->services[i];
            CustomServiceInfo *custom_service_info = (CustomServiceInfo*)data;
            custom_service_info->listener_->attachCondition(&conditionMutex, &conditionVariable);
        }

        for(unsigned long i = 0; i < guard_conditions->guard_condition_count; ++i)
        {
            void *data = guard_conditions->guard_conditions[i];
            GuardCondition *guard_condition = (GuardCondition*)data;
            guard_condition->attachCondition(&conditionMutex, &conditionVariable);
        }

        std::unique_lock<std::mutex> lock(conditionMutex);

        // First check variables.
        bool hasToWait = true;

        for(unsigned long i = 0; hasToWait && i < subscriptions->subscriber_count; ++i)
        {
            void *data = subscriptions->subscribers[i];
            CustomSubscriberInfo *custom_subscriber_info = (CustomSubscriberInfo*)data;
            if(custom_subscriber_info->listener_->hasData())
                hasToWait = false;
        }

        for(unsigned long i = 0; hasToWait && i < clients->client_count; ++i)
        {
            void *data = clients->clients[i];
            CustomClientInfo *custom_client_info = (CustomClientInfo*)data;
            if(custom_client_info->listener_->hasData())
                hasToWait = false;
        }

        for(unsigned long i = 0; hasToWait && i < services->service_count; ++i)
        {
            void *data = services->services[i];
            CustomServiceInfo *custom_service_info = (CustomServiceInfo*)data;
            if(custom_service_info->listener_->hasData())
                hasToWait = false;
        }

        for(unsigned long i = 0; hasToWait && i < guard_conditions->guard_condition_count; ++i)
        {
            void *data = guard_conditions->guard_conditions[i];
            GuardCondition *guard_condition = (GuardCondition*)data;
            if(guard_condition->hasTriggered())
                hasToWait = false;
        }

        if(hasToWait)
            conditionVariable.wait(lock);
        
        for(unsigned long i = 0; i < subscriptions->subscriber_count; ++i)
        {
            void *data = subscriptions->subscribers[i];
            CustomSubscriberInfo *custom_subscriber_info = (CustomSubscriberInfo*)data;
            if(!custom_subscriber_info->listener_->getHasData())
            {
                subscriptions->subscribers[i] = 0;
            }
            custom_subscriber_info->listener_->dettachCondition();
        }

        for(unsigned long i = 0; i < clients->client_count; ++i)
        {
            void *data = clients->clients[i];
            CustomClientInfo *custom_client_info = (CustomClientInfo*)data;
            if(!custom_client_info->listener_->hasData())
            {
                clients->clients[i] = 0;
            }
            custom_client_info->listener_->dettachCondition();
        }

        for(unsigned long i = 0; i < services->service_count; ++i)
        {
            void *data = services->services[i];
            CustomServiceInfo *custom_service_info = (CustomServiceInfo*)data;
            if(!custom_service_info->listener_->hasData())
            {
                services->services[i] = 0;
            }
            custom_service_info->listener_->dettachCondition();
        }

        for(unsigned long i = 0; i < guard_conditions->guard_condition_count; ++i)
        {
            void *data = guard_conditions->guard_conditions[i];
            GuardCondition *guard_condition = (GuardCondition*)data;
            if(!guard_condition->getHasTriggered())
            {
                guard_conditions->guard_conditions[i] = 0;
            }
            guard_condition->dettachCondition();
        }

        return RMW_RET_OK;
    }
}

