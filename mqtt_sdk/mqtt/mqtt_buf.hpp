#ifndef ONENET_MQTT_BUF_HPP
#define ONENET_MQTT_BUF_HPP

#include "mqtt/mqtt.h"

class MqttBuf
{
    MqttBuf(const MqttBuf&);
    MqttBuf &operator=(const MqttBuf&);
public:
    MqttBuf()
    { MqttBuffer_Init(m_buf); }

    ~MqttBuf()
    { MqttBuffer_Destroy(m_buf); }

    void Clear()
    { MqttBuffer_Reset(m_buf); }

    operator const MqttBuffer*() const
    { return m_buf; }

    operator MqttBuffer*()
    { return m_buf; }

    int PackConnectPkt(uint16_t keep_alive, const char *id,
                       bool clean_session, const char *will_topic,
                       const char *will_msg, uint16_t msg_len,
                       enum MqttQosLevel qos, bool will_retain, const char *user,
                       const char *password, uint16_t pswd_len)
    {
        return Mqtt_PackConnectPkt(m_buf, keep_alive, id, clean_session, will_topic, will_msg,
                                   msg_len, qos, will_retain, user, password, pswd_len);
    }

    int PackPublishPkt(uint16_t pkt_id, const char *topic,
                        const char *payload, uint32_t size,
                        enum MqttQosLevel qos, bool retain, bool own)
    { return Mqtt_PackPublishPkt(m_buf, pkt_id, topic, payload, size, qos, retain, own); }

    int SetPktDup()
    { return Mqtt_SetPktDup(m_buf); }

    int PackPubAckPkt(uint16_t pkt_id)
    { return Mqtt_PackPubAckPkt(m_buf, pkt_id); }

    int PackPubRecPkt(uint16_t pkt_id)
    { return Mqtt_PackPubRecPkt(m_buf, pkt_id); }

    int PackPubRelPkt(uint16_t pkt_id)
    { return Mqtt_PackPubRelPkt(m_buf, pkt_id); }

    int PackPubCompPkt(uint16_t pkt_id)
    { return Mqtt_PackPubCompPkt(m_buf, pkt_id); }

    int PackSubscribePkt(uint16_t pkt_id, const char *topic, MqttQosLevel qos)
    { return Mqtt_PackSubscribePkt(m_buf, pkt_id, topic, qos); }

    int AppendSubscribeTopic(const char *topic, MqttQosLevel qos)
    { return Mqtt_AppendSubscribeTopic(m_buf, topic, qos); }

    int PackUnsubscribePkt(uint16_t pkt_id, const char *topic)
    { return Mqtt_PackUnsubscribePkt(m_buf, pkt_id, topic); }

    int AppendUnsubscribeTopic(const char *topic)
    { return Mqtt_AppendUnsubscribeTopic(m_buf, topic); }

    int PackPingReqPkt()
    { return Mqtt_PackPingReqPkt(m_buf); }

    int PackDisconnectPkt()
    { return Mqtt_PackDisconnectPkt(m_buf); }

    int PackCmdRetPkt(uint16_t pkt_id, const char *cmdid,
                       const char *ret, uint32_t ret_len, bool own)
    { return Mqtt_PackCmdRetPkt(m_buf, pkt_id, cmdid, ret, ret_len, own); }

    int PackDataPointStart(uint16_t pkt_id, MqttQosLevel qos, bool retain, bool save)
    { return Mqtt_PackDataPointStart(m_buf, pkt_id, qos, retain, save); }

    int AppendDataPoint(const char *dsid, int64_t ts, int value)
    { return Mqtt_AppendDPInt(m_buf, dsid, ts, value); }

    int AppendDataPoint(const char *dsid, int64_t ts, double value)
    { return Mqtt_AppendDPDouble(m_buf, dsid, ts, value); }

    int AppendDataPoint(const char *dsid, int64_t ts, const char *value)
    { return Mqtt_AppendDPString(m_buf, dsid, ts, value); }

    int AppendDataPointStartObject(const char *dsid, int64_t ts)
    { return Mqtt_AppendDPStartObject(m_buf, dsid, ts); }

    int AppendDataPointSubvalue(const char *name, int value)
    { return Mqtt_AppendDPSubvalueInt(m_buf, name, value); }

    int AppendDataPointSubvalue(const char *name, double value)
    { return Mqtt_AppendDPSubvalueDouble(m_buf, name, value) ;}

    int AppendDataPointSubvalue(const char *name, const char *value)
    { return Mqtt_AppendDPSubvalueString(m_buf, name, value); }

    int AppendDataPointStartSubobject(const char *name)
    { return Mqtt_AppendDPStartSubobject(m_buf, name); }

    int AppendDataPointFinishSubobject(const char *name)
    { return Mqtt_AppendDPFinishSubobect(m_buf); }

    int PackDataPointByBinary(uint16_t pkt_id, const char *dsid, const char *desc,
                              time_t time, const char *bin, uint32_t size,
                              MqttQosLevel qos, bool retain, bool own, bool save)
    {
        return Mqtt_PackDataPointByBinary(m_buf, pkt_id, dsid, desc, time,
                                          bin, size, qos, retain, own, save);
    }

    int PackDataPointFinish()
    { return Mqtt_PackDataPointFinish(m_buf); }

private:
    MqttBuffer m_buf[1];
};

#endif // ONENET_MQTT_BUF_HPP
