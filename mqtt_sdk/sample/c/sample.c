#include "mqtt/mqtt.h"

#ifdef WIN32
#error Not support Windows now.
#endif // WIN32

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <getopt.h>


#define  WRITEUINT16(A,V) \
{\
    *(A) = (uint8_t)(((V) & 0xFF00) >> 8);      \
    *(A+1) = (uint8_t)((V) & 0x00FF);              \
}
#define  WRITEFLOAT(A, V) \
{\
    memcpy(A, &V, sizeof(float));\
}


struct MqttSampleContext
{
    int epfd;
    int mqttfd;
    uint32_t sendedbytes;
    struct MqttContext mqttctx[1];
    struct MqttBuffer mqttbuf[1];

    const char *host;
    unsigned short port;

    const char *proid;
    const char *devid;
    const char *apikey;

    int dup;
    enum MqttQosLevel qos;
    int retain;

    uint16_t pkt_to_ack;
    char cmdid[1024];
};

struct Command
{
    const char *cmd;
    int (*func)(struct MqttSampleContext *ctx);
    const char *desc;
};

#define buf_size 1024
#define STRLEN 64
char buf[buf_size];
char g_cmdid[STRLEN];
char* prjid = "339"; //project_id
char* auth_info = "{\"SYS\":\"F8E2ABB4278D47188CF6C1B3741D0DA1\"}"; //authoriz info
char* devid = "9277";  //device_id
#define MAX_TOPICS_NUMB 100
char* topics[MAX_TOPICS_NUMB+1];
int g_pkt_id = 1;

static int MqttSample_CmdConnect(struct MqttSampleContext *ctx);
static int MqttSample_CmdPing(struct MqttSampleContext *ctx);
static int MqttSample_RespCmdPublish(struct MqttSampleContext *ctx);
static int MqttSample_CmdPublish(struct MqttSampleContext *ctx);
static int MqttSample_CmdPushDp(struct MqttSampleContext *ctx);
static int MqttSample_CmdSubscribe(struct MqttSampleContext *ctx);
static int MqttSample_CmdUnsubscribe(struct MqttSampleContext *ctx);
static int MqttSample_CmdDisconnect(struct MqttSampleContext *ctx);
static int MqttSample_CmdExit(struct MqttSampleContext *ctx);
static int MqttSample_CmdHelp(struct MqttSampleContext *ctx);
static int MqttSample_CmdPublishFullJson(struct MqttSampleContext *ctx,int  qos);
static int MqttSample_CmdPublishBin(struct MqttSampleContext *ctx, int qos);
static int MqttSample_CmdPublishSimpleJsonWithoutTime(struct MqttSampleContext *ctx, int qos);
static int MqttSample_CmdPublishSimpleJsonWithTime(struct MqttSampleContext *ctx, int qos);
static int MqttSample_CmdPublishString(struct MqttSampleContext *ctx, int qos);
static int MqttSample_CmdPublishStringWithTime(struct MqttSampleContext *ctx, int qos);
static int MqttSample_CmdPublishFloat(struct MqttSampleContext *ctx, int  qos);


static const struct Command commands[] = {
    {"connect", MqttSample_CmdConnect, "Establish the connection."},
    {"ping", MqttSample_CmdPing, "Send ping packet."},
    {"publish",MqttSample_CmdPublish,"send data points, support parameter -q 0/1/2 (Qos0/Qos1/Qos2), -t 1-7 "},
    {"push_dp",MqttSample_CmdPushDp,"push data points"},
    {"cmdret",MqttSample_RespCmdPublish,"reponse cmd to server, support parameter -q 0/1/2 (Qos0/Qos1/Qos2)"},
    {"subscribe", MqttSample_CmdSubscribe, "Subscribe the data streams."},
    {"unsubscribe", MqttSample_CmdUnsubscribe, "Unsubscribe the data streams."},
    {"disconnect", MqttSample_CmdDisconnect, "Close the connection."},
    {"exit", MqttSample_CmdExit, "Exit the sample."},
    {"help", MqttSample_CmdHelp, "Print the usage of the commands."}
};

char** str_split(char* buf, size_t* topics_len){
    char** result = 0;
    result = topics;
    int len = strlen(buf);
    int i=0;
    *topics_len = 0;
    for(i=0;i<len;++i){
        while(buf[i]=='\x20' && i<len){
            buf[i] = '\x00';
            ++i;
        }
        if(len == i){
            return result;
        }
        if(buf[i] != '\x20'){
            topics[*topics_len] = buf + i;
            *topics_len += 1;

            while( buf[i] != '\x20' && i<len){
                ++i;
            }
            if(len == i){
                return result;
            }
            if(buf[i] == '\x20'){
                buf[i] = '\x00';
                continue;
            }
        }
    }
    return result;
}


static int MqttSample_CreateTcpConnect(const char *host, unsigned short port)
{
    struct sockaddr_in add;
    int fd;
    struct hostent *server;

    bzero(&add, sizeof(add));
    add.sin_family = AF_INET;
    add.sin_port = htons(port);
    server = gethostbyname(host);
    if(NULL == server) {
        printf("Failed to get the ip of the host(%s).\n", host);
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        printf("Failed to create socket file descriptor.\n");
        return fd;
    }

    bcopy((char*)server->h_addr, (char*)&add.sin_addr.s_addr, server->h_length);
    if(-1 == connect(fd, (struct sockaddr*)&add, sizeof(add))) {
        printf("Failed to connect to the server.\n");
        close(fd);
        return -1;
    }

    return fd;
}

static int MqttSample_RecvPkt(void *arg, void *buf, uint32_t count)
{
    int bytes = read((int)(size_t)arg, buf, count);
    return bytes;
}

static int MqttSample_SendPkt(void *arg, const struct iovec *iov, int iovcnt)
{
    int bytes;
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = (struct iovec*)iov;
    msg.msg_iovlen = (size_t)iovcnt;

    int i=0,j=0;
    printf("send one pkt\n");
    for(i=0; i<iovcnt; ++i){
        char *pkg = (char*)iov[i].iov_base;
        if(iov[i].iov_len > 1024){
            printf("length:%d\n", (int)iov[i].iov_len);
        }else{
            for(j=0; j<iov[i].iov_len; ++j)
                printf("%02X ", pkg[j]&0xFF);
            printf("\n");
        }
    }
    printf("send over\n");


    bytes = sendmsg((int)(size_t)arg, &msg, 0);
    return bytes;
}

//------------------------------- packet handlers -------------------------------------------
static int MqttSample_HandleConnAck(void *arg, char flags, char ret_code)
{
    printf("Success to connect to the server, flags(%0x), code(%d).\n",
           flags, ret_code);
    return 0;
}

static int MqttSample_HandlePingResp(void *arg)
{
    printf("Recv the ping response.\n");
    return 0;
}

static int MqttSample_HandlePublish(void *arg, uint16_t pkt_id, const char *topic,
                                    const char *payload, uint32_t payloadsize,
                                    int dup, enum MqttQosLevel qos)
{
    struct MqttSampleContext *ctx = (struct MqttSampleContext*)arg;
    ctx->pkt_to_ack = pkt_id;
    ctx->dup = dup;
    ctx->qos = qos;
    printf("dup=%d, qos=%d, id=%d\ntopic: %s\npayloadsize=%d\n",
           dup, qos, pkt_id, topic, payloadsize);

    //get cmdid
    //$creq/topic_name/cmdid
    memset(g_cmdid, STRLEN, 0);
    if('$' == topic[0] &&
        'c' == topic[1] &&
        'r' == topic[2] &&
        'e' == topic[3] &&
        'q' == topic[4] &&
        '/' == topic[5]){
        int i=6;
        while(topic[i]!='/' && i<strlen(topic)){
            ++i;
        }
        if(i<strlen(topic))
            memcpy(g_cmdid, topic+i+1, strlen(topic+i+1));
    }
    return 0;
}

static int MqttSample_HandlePubAck(void *arg, uint16_t pkt_id)
{
    printf("Recv the publish ack, packet id is %d.\n", pkt_id);
    return 0;
}

static int MqttSample_HandlePubRec(void *arg, uint16_t pkt_id)
{
    struct MqttSampleContext *ctx = (struct MqttSampleContext*)arg;
    ctx->pkt_to_ack = pkt_id;
    printf("Recv the publish rec, packet id is %d.\n", pkt_id);
    return 0;
}

static int MqttSample_HandlePubRel(void *arg, uint16_t pkt_id)
{
    struct MqttSampleContext *ctx = (struct MqttSampleContext*)arg;
    ctx->pkt_to_ack = pkt_id;
    printf("Recv the publish rel, packet id is %d.\n", pkt_id);
    return 0;
}

static int MqttSample_HandlePubComp(void *arg, uint16_t pkt_id)
{
    printf("Recv the publish comp, packet id is %d.\n", pkt_id);
    return 0;
}

static int MqttSample_HandleSubAck(void *arg, uint16_t pkt_id, const char *codes, uint32_t count)
{
    uint32_t i;
    printf("Recv the subscribe ack, packet id is %d, return code count is %d:.\n", pkt_id, count);
    for(i = 0; i < count; ++i) {
        unsigned int code = ((unsigned char*)codes)[i];
        printf("   code%d=%02x\n", i, code);
    }

    return 0;
}

static int MqttSample_HandleUnsubAck(void *arg, uint16_t pkt_id)
{
    printf("Recv the unsubscribe ack, packet id is %d.\n", pkt_id);
    return 0;
}

static int MqttSample_HandleCmd(void *arg, uint16_t pkt_id, const char *cmdid,
                                int64_t timestamp, const char *desc, const char *cmdarg,
                                uint32_t cmdarg_len, int dup, enum MqttQosLevel qos)
{
    uint32_t i;
    struct MqttSampleContext *ctx = (struct MqttSampleContext*)arg;
    ctx->pkt_to_ack = pkt_id;
    strcpy(ctx->cmdid, cmdid);
    printf("Recv the command, packet id is %d, cmduuid is %s, qos=%d, dup=%d.\n",
           pkt_id, cmdid, qos, dup);

    if(0 != timestamp) {
        time_t seconds = timestamp / 1000;
        struct tm *st = localtime(&seconds);

        printf("    The timestampe is %04d-%02d-%02dT%02d:%02d:%02d.%03d.\n",
               st->tm_year + 1900, st->tm_mon + 1, st->tm_mday,
               st->tm_hour, st->tm_min, st->tm_sec, (int)(timestamp % 1000));
    }
    else {
        printf("    There is no timestamp.\n");
    }

    if(NULL != desc) {
        printf("    The description is: %s.\n", desc);
    }
    else {
        printf("    There is no description.\n");
    }

    printf("    The length of the command argument is %d, the argument is:", cmdarg_len);

    for(i = 0; i < cmdarg_len; ++i) {
        const char c = cmdarg[i];
        if(0 == i % 16) {
            printf("\n        ");
        }
        printf("%02X'%c' ", c, c);
    }
    printf("\n");


    printf("send the cmd resp with Qos=1\n");
    int err = Mqtt_PackCmdRetPkt(ctx->mqttbuf, 1, ctx->cmdid,
                             "hello MQTT", 11, MQTT_QOS_LEVEL1,1);
    if(MQTTERR_NOERROR != err) {
        printf("Critical bug: failed to pack the cmd ret packet.\n");
        return -1;
    }


    return 0;
}

//-------------------------------- Commands ------------------------------------------------------
static int MqttSample_CmdConnect(struct MqttSampleContext *ctx)
{
    int err, flags;
    struct epoll_event event;

    if(ctx->mqttfd >= 0) {
        close(ctx->mqttfd);
        epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, ctx->mqttfd, NULL);
    }

    ctx->mqttfd = MqttSample_CreateTcpConnect(ctx->host, ctx->port);
    if(ctx->mqttfd < 0) {
        return -1;
    }
    ctx->mqttctx->read_func_arg = (void*)(size_t)ctx->mqttfd;
    ctx->mqttctx->writev_func_arg = (void*)(size_t)ctx->mqttfd;

    flags = fcntl(ctx->mqttfd, F_GETFL, 0);
	if(-1 == flags) {
	    printf("Failed to get the socket file flags, errcode is %d.\n", errno);
	}
	
    if(fcntl(ctx->mqttfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        printf("Failed to set the socket to nonblock mode, errcode is %d.\n", errno);
        return -1;
    }

    event.data.fd = ctx->mqttfd;
    event.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
    if(epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ctx->mqttfd, &event) < 0) {
        printf("Failed to add the socket to the epoll, errcode is %d.\n", errno);
        return -1;
    }
    
    ctx->devid = devid;
    int keep_alive = 1200;
    printf("dev id:%s\n", devid );
    printf("project id:%s\n", prjid);
    printf("auth info:%s\n", auth_info);
    err = Mqtt_PackConnectPkt(ctx->mqttbuf, keep_alive, ctx->devid,
                              1, NULL,
                              NULL, 0,
                              MQTT_QOS_LEVEL0, 0, prjid,
                              auth_info, strlen(auth_info));
    if(MQTTERR_NOERROR != err) {
        printf("Failed to pack the MQTT CONNECT PACKET, errcode is %d.\n", err);
        return -1;
    }

    return 0;
}

static int MqttSample_CmdPing(struct MqttSampleContext *ctx)
{
    int err;
    err = Mqtt_PackPingReqPkt(ctx->mqttbuf);
    if(MQTTERR_NOERROR != err) {
        printf("Critical bug: failed to pack the ping request packet.\n");
        return -1;
    }

    return 0;
}


static int MqttSample_CmdPublishFullJson(struct MqttSampleContext *ctx,int  qos){
    const char *str = "{\"datastreams\":[{ \"id\":\"temperature\", \"datapoints\":[{\"at\":\"2016-12-22 22:22:22\",\"value\": 36.5}]}]}";
    uint32_t size = strlen(str);
    int retain = 0;
    int own = 1;
    int err = MQTTERR_NOERROR;


#ifdef _debug
    printf("package id: %d\n", g_pkt_id);
#endif

    
    MqttBuffer_Init(ctx->mqttbuf);
    err = Mqtt_PackDataPointByString(ctx->mqttbuf, g_pkt_id++, 0, kTypeFullJson, str, size, qos, retain, own);
    
    return err;
    
}

static int MqttSample_CmdPublishBin(struct MqttSampleContext *ctx, int qos){
    const char *dsid = "image";
    const char *desc = "upload image";
    const uint32_t size = 10*2*512;
    char bin[size];
    int retain = 0;
    int own = 1;
    int err = MQTTERR_NOERROR;
    
    MqttBuffer_Init(ctx->mqttbuf);
    err = Mqtt_PackDataPointByBinary(ctx->mqttbuf, g_pkt_id++, dsid, desc, 0, bin, size, qos, retain, own);

    return err;
}

static int MqttSample_CmdPublishSimpleJsonWithoutTime(struct MqttSampleContext *ctx, int qos){
    const char *str = "{\"temperature\":22.5}";
    uint32_t size = strlen(str);
    int retain = 0;
    int own = 1;
    int err = MQTTERR_NOERROR;

    MqttBuffer_Init(ctx->mqttbuf);
    err = Mqtt_PackDataPointByString(ctx->mqttbuf, g_pkt_id++, 0, kTypeSimpleJsonWithoutTime, str, size, qos, retain, own);

    return err;
}

static int MqttSample_CmdPublishSimpleJsonWithTime(struct MqttSampleContext *ctx, int qos){
    const char *str =   "{\"temperature\":{\"2015-03-22 22:31:12\":22.5}}";
    uint32_t size = strlen(str);
    int retain = 0;
    int own = 1;
    int err = MQTTERR_NOERROR;

    MqttBuffer_Init(ctx->mqttbuf);
    err = Mqtt_PackDataPointByString(ctx->mqttbuf, g_pkt_id++, 0, kTypeSimpleJsonWithTime, str, size, qos, retain, own);
    
    return err;
}

static int MqttSample_CmdPublishString(struct MqttSampleContext *ctx, int qos){
    const char *str = ",;temperature,2015-03-22 22:31:12,22.5;102;pm2.5,89;10";
    uint32_t size = strlen(str);
    int retain = 0;
    int own = 1;
    int err = MQTTERR_NOERROR;

    MqttBuffer_Init(ctx->mqttbuf);
    err = Mqtt_PackDataPointByString(ctx->mqttbuf, g_pkt_id++, 0, kTypeString, str, size, qos, retain, own);

    return err;
}

static int MqttSample_CmdPublishStringWithTime(struct MqttSampleContext *ctx, int qos){
    const char *str = ",;temperature,2015-03-22 22:31:12,22.5;102;pm2.5,89;10";
    uint32_t size = strlen(str);
    int retain = 0;
    int own = 1;
    int err = MQTTERR_NOERROR;
    
    MqttBuffer_Init(ctx->mqttbuf);
    err = Mqtt_PackDataPointByString(ctx->mqttbuf, g_pkt_id++, 0, PAYLOADWITHOUTTIME(kTypeStringWithTime), str, size, qos, retain, own);
    
    return err;
}

static int MqttSample_CmdPublishFloat(struct MqttSampleContext *ctx, int  qos){
    int retain = 0;
    int own = 1;
    int err = MQTTERR_NOERROR;
    float dps[2] = {34.0, -20.3};
    int16_t ds_name = 0;
    char float_points[1024];

    MqttBuffer_Init(ctx->mqttbuf);

    WRITEUINT16(float_points, ds_name);
    WRITEUINT16(float_points+2, 2);
    WRITEFLOAT(float_points+4, dps[0]);
    WRITEFLOAT(float_points+8, dps[1]);

    err = Mqtt_PackDataPointByString(ctx->mqttbuf, g_pkt_id++, 0, PAYLOADWITHTIME(kTypeFloat), float_points, 12, qos, retain, own);
    if(err) {
        printf("assemble publish package(type=Float,Qos= %d) error\n", qos);
    }
    return 0;

}


static int MqttSample_CmdPublish(struct MqttSampleContext *ctx)
{


    int Qos=1;
    int type = 7;
    int i = 0;
    /*-q 0/1/2   ----> Qos0/Qos1/Qos2
      -t 1/2/3/4/5/6/7   ----> json/float datapoint
    */
    for(i=0; i<strlen(buf); ++i){
        if(('-' == buf[i])&&('q'== buf[i+1])){
            Qos = atoi(buf+i+2);
        }
        else if(('-' == buf[i])&&('t'==buf[i+1])){
            type = atoi(buf+i+2);
        }

    }

    if(0==Qos){
        if(kTypeFullJson == type){
            MqttSample_CmdPublishFullJson(ctx, MQTT_QOS_LEVEL0);
        }else if(kTypeBin == type){
            MqttSample_CmdPublishBin(ctx, MQTT_QOS_LEVEL0);
        }else if(kTypeSimpleJsonWithoutTime == type){
            MqttSample_CmdPublishSimpleJsonWithoutTime(ctx, MQTT_QOS_LEVEL0);
        }else if(kTypeSimpleJsonWithTime == type){
            MqttSample_CmdPublishSimpleJsonWithTime(ctx, MQTT_QOS_LEVEL0);
        }else if(kTypeString == type){
            MqttSample_CmdPublishString(ctx, MQTT_QOS_LEVEL0);
        }else if(kTypeStringWithTime == type){
            MqttSample_CmdPublishStringWithTime(ctx, MQTT_QOS_LEVEL0);
        }
        else if(kTypeFloat == type){
            MqttSample_CmdPublishFloat(ctx, MQTT_QOS_LEVEL0);
        }

    }else if(1==Qos){
        if(kTypeFullJson == type){
            MqttSample_CmdPublishFullJson(ctx, MQTT_QOS_LEVEL1);
        }else if(kTypeBin == type){
            MqttSample_CmdPublishBin(ctx, MQTT_QOS_LEVEL1);
        }else if(kTypeSimpleJsonWithoutTime == type){
            MqttSample_CmdPublishSimpleJsonWithoutTime(ctx, MQTT_QOS_LEVEL1);
        }else if(kTypeSimpleJsonWithTime == type){
            MqttSample_CmdPublishSimpleJsonWithTime(ctx, MQTT_QOS_LEVEL1);
        }else if(kTypeString == type){
            MqttSample_CmdPublishString(ctx, MQTT_QOS_LEVEL1);
        }else if(kTypeStringWithTime == type){
            MqttSample_CmdPublishStringWithTime(ctx, MQTT_QOS_LEVEL1);
        }
        else if(kTypeFloat == type){
            MqttSample_CmdPublishFloat(ctx, MQTT_QOS_LEVEL1);
        }
        
    }else if(2 == Qos){
        if(kTypeFullJson == type){
            MqttSample_CmdPublishFullJson(ctx, MQTT_QOS_LEVEL2);
        }else if(kTypeBin == type){
            MqttSample_CmdPublishBin(ctx, MQTT_QOS_LEVEL2);
        }else if(kTypeSimpleJsonWithoutTime == type){
            MqttSample_CmdPublishSimpleJsonWithoutTime(ctx, MQTT_QOS_LEVEL2);
        }else if(kTypeSimpleJsonWithTime == type){
            MqttSample_CmdPublishSimpleJsonWithTime(ctx, MQTT_QOS_LEVEL2);
        }else if(kTypeString == type){
            MqttSample_CmdPublishString(ctx, MQTT_QOS_LEVEL2);
        }else if(kTypeStringWithTime == type){
            MqttSample_CmdPublishStringWithTime(ctx, MQTT_QOS_LEVEL2);
        }
        else if(kTypeFloat == type){
            MqttSample_CmdPublishFloat(ctx, MQTT_QOS_LEVEL2);
        }
        
    }
    return 0;
}


static int MqttSample_CmdPushDp(struct MqttSampleContext *ctx)
{

    int err;
    char **topics;
    size_t topics_len = 0;
    int i=0;

/*去掉最后回车键
 */
    for(i=strlen(buf); i>0; --i){
        if(buf[i-1] == 0x0a)
            buf[i-1] = 0x00;
    }

    topics = str_split(buf, &topics_len);

    if(4 != topics_len){
        printf("usage:push_dp topicname payload pkg_id\n");
        return 0;
    }

    printf("topic anme:%s\n", *(topics+1));
    printf("payload:%s\n", *(topics+2));
    printf("pkg_id:%s\n", *(topics+3));
    printf("\n");

    if(ctx->mqttbuf->first_ext) {
        return MQTTERR_INVALID_PARAMETER;
    }

    int pkg_id = atoi(*(topics+3));
    err = Mqtt_PackPublishPkt(ctx->mqttbuf, pkg_id, *(topics+1), *(topics+2), strlen(*(topics+2)), MQTT_QOS_LEVEL1, 0, 1);

    if(err != MQTTERR_NOERROR) {
        return err;
    }

    return 0;


}



static int MqttSample_RespCmdPublish(struct MqttSampleContext *ctx){

    int Qos=1;
    int i = 0;

    /*-q 0/1   ----> Qos0/Qos1
    */
    for(i=0; i<strlen(buf); ++i){
        if(('-' == buf[i])&&('q'== buf[i+1])){
            Qos = atoi(buf+i+2);
        }
    }

    int err = 0;
    if(0==Qos){
         err = Mqtt_PackCmdRetPkt(ctx->mqttbuf, g_pkt_id++, ctx->cmdid,
                                  "hello MQTT Qos0", 11, MQTT_QOS_LEVEL0, 1);
    }else if(1==Qos){
        err = Mqtt_PackCmdRetPkt(ctx->mqttbuf, g_pkt_id++, ctx->cmdid,
                                 "hello MQTT Qos1", 11, MQTT_QOS_LEVEL1,1);
    }else if(2 == Qos){
        err = Mqtt_PackCmdRetPkt(ctx->mqttbuf, g_pkt_id++, ctx->cmdid, "hello MQTT Qos2", 11, MQTT_QOS_LEVEL2, 1);
    }

    if(MQTTERR_NOERROR != err) {
        printf("Critical bug: failed to pack the cmd ret packet.\n");
        return -1;
    }

    return 0;
}

/*
static int MqttSample_CmdPublishCommandResp(struct MqttSampleContext *ctx)
{
    int err = 0;

    err |= Mqtt_PackDataPointStart(ctx->mqttbuf, 1, MQTT_QOS_LEVEL1, 0, 0);


    char t_payload[] = {'\x07','\x01','\x01','\x01','\x01','\x41','\x42','\x43','\x44'};
    MqttBuffer_Append(ctx->mqttbuf, t_payload, strlen(t_payload), 1);

    return 0;
}
*/


static int MqttSample_CmdSubscribe(struct MqttSampleContext *ctx)
{
    int err;
    char **topics;
    size_t topics_len = 0;
    int i = 0;

    
/*去掉最后回车键
 */
    for(i=strlen(buf); i>0; --i){
        if(buf[i-1] == 0x0a)
            buf[i-1] = 0x00;
    }

    topics = str_split(buf, &topics_len);

    for(i=1;i<topics_len;++i){
        printf("topic name:%s\n", *(topics + i));
    }

    err = Mqtt_PackSubscribePkt(ctx->mqttbuf, 11, MQTT_QOS_LEVEL1, topics+1, topics_len-1);
    if(err != MQTTERR_NOERROR) {
        printf("Critical bug: failed to pack the subscribe packet.\n");
        return -1;
    }

    return 0;
}

static int MqttSample_CmdUnsubscribe(struct MqttSampleContext *ctx)
{
    int err;
    char **topics;
    size_t topics_len = 0;
    int i = 0;

    
/*去掉最后回车键
 */
    for(i=strlen(buf); i>0; --i){
        if(buf[i-1] == 0x0a)
            buf[i-1] = 0x00;
    }

    topics = str_split(buf, &topics_len);


    for(i=1;i<topics_len;++i){
        printf("unsubscribe topic name:%s\n", *(topics + i));
    }
    printf("\n");
    
    err = Mqtt_PackUnsubscribePkt(ctx->mqttbuf, 11, topics+1, topics_len-1);
    if(err != MQTTERR_NOERROR) {
        printf("Critical bug: failed to pack the unsubscribe packet.\n");
        return -1;
    }

    return 0;
}

static int MqttSample_CmdDisconnect(struct MqttSampleContext *ctx)
{
    int err;
    err = Mqtt_PackDisconnectPkt(ctx->mqttbuf);
    if(MQTTERR_NOERROR != err) {
        printf("Critical bug: failed to pack the disconnect packet.\n");
        return -1;
    }

    return 1;
}
/*
static int MqttSample_CmdCmdRet(struct MqttSampleContext *ctx)
{
    int err;
    err = Mqtt_PackCmdRetPkt(ctx->mqttbuf, 1, ctx->cmdid,
                             "hello MQTT", 11, MQTT_QOS_LEVEL1, 1);
    if(MQTTERR_NOERROR != err) {
        printf("Critical bug: failed to pack the cmd ret packet.\n");
        return -1;
    }

    return 0;
}
*/
static int MqttSample_CmdExit(struct MqttSampleContext *ctx)
{
    (void)ctx;
    return -1;
}

static int MqttSample_CmdHelp(struct MqttSampleContext *ctx)
{
    int i;
    (void)ctx;
    printf("Commands: \n");
    for(i = 0; i < sizeof(commands) / sizeof(*commands); ++i) {
        printf("  %-12s    %s\n", commands[i].cmd, commands[i].desc);
    }
    printf("\n");

    return 0;
}

static int MqttSample_HandleStdin(struct MqttSampleContext *ctx, uint32_t events)
{
    int bytes, i, ret = 0;

    if(-1 != ctx->sendedbytes) {
        printf("There are something to be send, please wait a moment to retry.\n");
        return 0;
    }

    memset(buf, buf_size, 0);
    bytes = read(STDIN_FILENO, buf, buf_size);
    buf[bytes - 1] = 0;


    char tmp_buf[1024];
    memcpy(tmp_buf, buf, bytes);
    for(i=0; i<bytes; ++i){
        if(tmp_buf[i]==' '){
            tmp_buf[i]=0;
            break;
        }
    }
    for(i = 0; i < sizeof(commands) / sizeof(*commands); ++i) {
        if(strcmp(commands[i].cmd, tmp_buf) == 0) {
            if((ret = commands[i].func(ctx)) < 0) {
                return -1;
            }
            break;
        }
    }

    bytes = Mqtt_SendPkt(ctx->mqttctx, ctx->mqttbuf, 0);
    if(bytes < 0) {
        printf("Failed to send the packet to the server.\n");
        return -1;
    }
    else if(bytes != ctx->mqttbuf->buffered_bytes) {
        struct epoll_event evt[1];

        ctx->sendedbytes = bytes;
        printf("There are some data not sended(%d bytes).\n",
               ctx->mqttbuf->buffered_bytes - bytes);

        evt->data.fd = ctx->mqttfd;
        evt->events = EPOLLIN | EPOLLOUT | EPOLLONESHOT | EPOLLET;
        epoll_ctl(ctx->epfd, EPOLL_CTL_MOD, ctx->mqttfd, evt);
        return 0;
    }

    MqttBuffer_Reset(ctx->mqttbuf);
    if(ret > 0) {
        close(ctx->mqttfd);
        epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, ctx->mqttfd, NULL);
        ctx->mqttfd = -1;
    }

    return 0;
}

static int MqttSample_HandleSocket(struct MqttSampleContext *ctx, uint32_t events)
{
    struct epoll_event evt[1];
    evt->data.fd = ctx->mqttfd;
    evt->events = EPOLLIN;

    if(events & EPOLLIN) {
        while(1) {
            int err;
            err = Mqtt_RecvPkt(ctx->mqttctx);
            if(MQTTERR_ENDOFFILE == err) {
                printf("The connection is disconnected.\n");
                close(ctx->mqttfd);
                epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, ctx->mqttfd, NULL);
                ctx->mqttfd = -1;
                return 0;
            }

            if(MQTTERR_IO == err) {
                if((EAGAIN == errno) || (EWOULDBLOCK == errno)) {
                    break;
                }

                printf("Send TCP data error: %s.\n", strerror(errno));
                return -1;
            }

            if(MQTTERR_NOERROR != err) {
                printf("Mqtt_RecvPkt error is %d.\n", err);
                return -1;
            }
        }
    }

    if(events & EPOLLOUT) {
        if(-1 != ctx->sendedbytes) {
            int bytes = Mqtt_SendPkt(ctx->mqttctx, ctx->mqttbuf, ctx->sendedbytes);
            if(bytes < 0) {
                return -1;
            }
            else {
                ctx->sendedbytes += bytes;
                if(ctx->sendedbytes == ctx->mqttbuf->buffered_bytes) {
                    MqttBuffer_Reset(ctx->mqttbuf);
                    ctx->sendedbytes = -1;
                }
                else {
                    evt->events |= EPOLLOUT;
                }
            }
        }
    }

    epoll_ctl(ctx->epfd, EPOLL_CTL_MOD, ctx->mqttfd, evt);
    return 0;
}

static int MqttSample_Init(struct MqttSampleContext *ctx)
{
    struct epoll_event event;
    int err;

    ctx->sendedbytes = -1;
    ctx->mqttfd = -1;

    err = Mqtt_InitContext(ctx->mqttctx, 1 << 20);
    if(MQTTERR_NOERROR != err) {
        printf("Failed to init MQTT context errcode is %d", err);
        return -1;
    }

    ctx->mqttctx->read_func = MqttSample_RecvPkt;
    ctx->mqttctx->read_func_arg =  (void*)(size_t)ctx->mqttfd;
    ctx->mqttctx->writev_func_arg =  (void*)(size_t)ctx->mqttfd;
    ctx->mqttctx->writev_func = MqttSample_SendPkt;

    ctx->mqttctx->handle_conn_ack = MqttSample_HandleConnAck;
    ctx->mqttctx->handle_conn_ack_arg = ctx;
    ctx->mqttctx->handle_ping_resp = MqttSample_HandlePingResp;
    ctx->mqttctx->handle_ping_resp_arg = ctx;
    ctx->mqttctx->handle_publish = MqttSample_HandlePublish;
    ctx->mqttctx->handle_publish_arg = ctx;
    ctx->mqttctx->handle_pub_ack = MqttSample_HandlePubAck;
    ctx->mqttctx->handle_pub_ack_arg = ctx;
    ctx->mqttctx->handle_pub_rec = MqttSample_HandlePubRec;
    ctx->mqttctx->handle_pub_rec_arg = ctx;
    ctx->mqttctx->handle_pub_rel = MqttSample_HandlePubRel;
    ctx->mqttctx->handle_pub_rel_arg = ctx;
    ctx->mqttctx->handle_pub_comp = MqttSample_HandlePubComp;
    ctx->mqttctx->handle_pub_comp_arg = ctx;
    ctx->mqttctx->handle_sub_ack = MqttSample_HandleSubAck;
    ctx->mqttctx->handle_sub_ack_arg = ctx;
    ctx->mqttctx->handle_unsub_ack = MqttSample_HandleUnsubAck;
    ctx->mqttctx->handle_unsub_ack_arg = ctx;
    ctx->mqttctx->handle_cmd = MqttSample_HandleCmd;
    ctx->mqttctx->handle_cmd_arg = ctx;

    ctx->cmdid[0] = '\0';
    MqttBuffer_Init(ctx->mqttbuf);

    ctx->epfd = epoll_create(10);
    if(ctx->epfd < 0) {
        printf("Failed to create the epoll instance.\n");
        return -1;
    }

    if(fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) < 0) {
        printf("Failed to set the stdin to nonblock mode, errcode is %d.\n", errno);
        return -1;
    }

    event.data.fd = STDIN_FILENO;
    event.events = EPOLLIN;
    if(epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, STDIN_FILENO, &event) < 0) {
        printf("Failed to add the stdin to epoll, errcode is %d.\n", errno);
        return -1;
    }

    return 0;
}

void useage(char *argv){
    printf("-i ip\n");
    printf("-p port\n");
    printf("-j project id\n");
    printf("-a auth-info\n");
    printf("-d devid\n");
    exit(1);
}

int main(int argc, char **argv)
{
    struct MqttSampleContext smpctx[1];
    int evt_cnt;
    const int evt_max_cnt = 2;
    struct epoll_event events[evt_max_cnt];
    int exit;

    smpctx->host = "172.19.3.1";
    smpctx->port = 10021;

    char opt;

    while ((opt = getopt(argc, argv, "hi:p:j:a:d:")) != -1) {
        switch(opt){
        case 'i':
            smpctx->host = optarg;
            break;

        case 'p':
            smpctx->port = atoi(optarg);
            break;

        case 'h':
            useage(argv[0]);
            break;

        case 'j':
            prjid = optarg;
            break;

        case 'a':
            auth_info = optarg;
            break;

        case 'd':
            devid = optarg;
            break;

        default:
            return 1;
            break;
        }
    }



    if(MqttSample_Init(smpctx) < 0) {
        return -1;
    }

    (void)MqttSample_CmdHelp(smpctx);

    exit = 0;
    while(!exit && (evt_cnt = epoll_wait(smpctx->epfd, events, evt_max_cnt, -1)) >= 0) {
        int i;
        for(i = 0; i < evt_cnt; ++i) {
            if(STDIN_FILENO == events[i].data.fd) {
                if(MqttSample_HandleStdin(smpctx, events[i].events) < 0) {
                    exit = 1;
                    break;
                }
                events[i].events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                epoll_ctl(smpctx->epfd, EPOLL_CTL_MOD, events[i].data.fd, events + i);
            }
            else {
                if(MqttSample_HandleSocket(smpctx, events[i].events) < 0) {
                    exit = 1;
                    break;
                }
            }
        }
    }

    // reclaim the resource
    MqttBuffer_Destroy(smpctx->mqttbuf);
    Mqtt_DestroyContext(smpctx->mqttctx);

    if(smpctx->epfd >= 0) {
        close(smpctx->epfd);
        smpctx->epfd = -1;
    }

    if(smpctx->mqttfd >= 0) {
        close(smpctx->mqttfd);
        smpctx->mqttfd = -1;
    }

    printf("bye bye......\n");

    return 0;
}
