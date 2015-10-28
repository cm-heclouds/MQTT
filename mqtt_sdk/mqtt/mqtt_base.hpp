#ifndef ONENET_MQTT_BASE_HPP
#define ONENET_MQTT_BASE_HPP

#include "mqtt/mqtt.h"


template<class T>
class MqttBase
{
protected:
    bool m_valid;
    MqttContext m_ctx[1];

    MqttBase() : m_valid(false)
    {}

    ~MqttBase()
    {
        if(m_valid) {
            Mqtt_DestroyContext(m_ctx);
        }
    }
public:
    int Init(uint32_t buf_size)
    {
        int ret = Mqtt_InitContext(m_ctx, buf_size);
        if(MQTTERR_NOERROR != ret) {
            return ret;
        }

        T *real_obj = static_cast<T*>(this);

        m_ctx->read_func_arg = real_obj;
        m_ctx->read_func = &T::_Read;

        m_ctx->writev_func_arg = real_obj;
        m_ctx->writev_func = &T::_Writev;

        m_ctx->handle_ping_resp_arg = real_obj;
        m_ctx->handle_ping_resp = &T::_HandlePingResp;

        m_ctx->handle_conn_ack_arg = real_obj;
        m_ctx->handle_conn_ack = &T::_HandleConnAck;

        m_ctx->handle_publish_arg = real_obj;
        m_ctx->handle_publish = &T::_HandlePublish;

        m_ctx->handle_pub_ack_arg = real_obj;
        m_ctx->handle_pub_ack = &T::_HandlePubAck;

        m_ctx->handle_pub_rec_arg = real_obj;
        m_ctx->handle_pub_rec = &T::_HandlePubRec;

        m_ctx->handle_pub_rel_arg = real_obj;
        m_ctx->handle_pub_rel = &T::_HandlePubRel;

        m_ctx->handle_pub_comp_arg = real_obj;
        m_ctx->handle_pub_comp = &T::_HandlePubComp;

        m_ctx->handle_sub_ack_arg = real_obj;
        m_ctx->handle_sub_ack = &T::_HandleSubAck;

        m_ctx->handle_unsub_ack_arg = real_obj;
        m_ctx->handle_unsub_ack = &T::_HandleUnsubAck;

        m_ctx->handle_cmd_arg = real_obj;
        m_ctx->handle_cmd = &T::_HandleCmd;

        m_valid = true;
        return MQTTERR_NOERROR;
    }

    int Recv()
    {
        return Mqtt_RecvPkt(m_ctx);
    }

    int Send(MqttBuffer *buf, uint32_t offset)
    {
        return Mqtt_SendPkt(m_ctx, buf, offset);
    }


    virtual int Read(void *buf, uint32_t size)
    {
        (void)buf; (void)size;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int Writev(const struct iovec *iov, int count)
    {
        (void)iov; (void)count;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int HandlePingResp()
    { return MQTTERR_EMPTY_CALLBACK; }

    virtual int HandleConnAck(char flags, char ret_code)
    {
        (void)flags; (void)ret_code;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int HandlePublish(uint16_t pkt_id, const char *topic, const char *payload,
                              uint32_t payloadsize, bool dup, MqttQosLevel qos)
    {
        (void)pkt_id; (void)topic; (void)payload;
        (void)payloadsize; (void)dup; (void)qos;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int HandlePubAck(uint16_t pkt_id)
    {
        (void)pkt_id;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int HandlePubRec(uint16_t pkt_id)
    {
        (void)pkt_id;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int HandlePubRel(uint16_t pkt_id)
    {
        (void)pkt_id;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int HandlePubComp(uint16_t pkt_id)
    {
        (void)pkt_id;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int HandleSubAck(uint16_t pkt_id, const char *codes, uint32_t count)
    {
        (void)pkt_id; (void)codes; (void)count;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int HandleUnsubAck(uint16_t pkt_id)
    {
        (void)pkt_id;
        return MQTTERR_EMPTY_CALLBACK;
    }

    virtual int HandleCmd(uint16_t pkt_id, const char *cmdid, const char *data,
                          uint32_t size, int dup, MqttQosLevel qos)
    {
        (void)pkt_id; (void)cmdid; (void)data; (void)size; (void)dup; (void)qos;
        return MQTTERR_EMPTY_CALLBACK;
    }

protected:
    static int _Read(void *arg, void *buf, uint32_t count)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->Read(buf, count);
    }

    static int _Writev(void *arg, const iovec *iov,  int count)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->Writev(iov, count);
    }

    static int _HandlePingResp(void *arg)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandlePingResp();
    }

    static int _HandleConnAck(void *arg, char flags, char ret_code)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandleConnAck(flags, ret_code);
    }

    static int _HandlePublish(void *arg, uint16_t pkt_id, const char *topic, const char *payload,
                              uint32_t payloadsize, int dup, MqttQosLevel qos)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandlePublish(pkt_id, topic, payload, payloadsize, dup, qos);
    }

    static int _HandlePubAck(void *arg, uint16_t pkt_id)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandlePubAck(pkt_id);
    }

    static int _HandlePubRec(void *arg, uint16_t pkt_id)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandlePubRec(pkt_id);
    }

    static int _HandlePubRel(void *arg, uint16_t pkt_id)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandlePubRel(pkt_id);
    }

    static int _HandlePubComp(void *arg, uint16_t pkt_id)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandlePubComp(pkt_id);
    }

    static int _HandleSubAck(void *arg, uint16_t pkt_id, const char *codes, uint32_t count)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandleSubAck(pkt_id, codes, count);
    }

    static int _HandleUnsubAck(void *arg, uint16_t pkt_id)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandleUnsubAck(pkt_id);
    }

    static int _HandleCmd(void *arg, uint16_t pkt_id, const char *cmdid, const char *data,
                          uint32_t size, int dup, MqttQosLevel qos)
    {
        MqttBase *self = reinterpret_cast<MqttBase*>(arg);
        return self->HandleCmd(pkt_id, cmdid, data, size, dup, qos);
    }
};


#endif // ONENET_MQTT_BASE_HPP
