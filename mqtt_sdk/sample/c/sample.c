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
#include <strings.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

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

static int MqttSample_CmdConnect(struct MqttSampleContext *ctx);
static int MqttSample_CmdPing(struct MqttSampleContext *ctx);
static int MqttSample_CmdPublish(struct MqttSampleContext *ctx);
static int MqttSample_CmdSubscribe(struct MqttSampleContext *ctx);
static int MqttSample_CmdUnsubscribe(struct MqttSampleContext *ctx);
static int MqttSample_CmdDisconnect(struct MqttSampleContext *ctx);
static int MqttSample_CmdCmdRet(struct MqttSampleContext *ctx);
static int MqttSample_CmdExit(struct MqttSampleContext *ctx);
static int MqttSample_CmdHelp(struct MqttSampleContext *ctx);

static const struct Command commands[] = {
    {"connect", MqttSample_CmdConnect, "Establish the connection."},
    {"ping", MqttSample_CmdPing, "Send ping packet."},
    {"publish", MqttSample_CmdPublish, "Send data points."},
    {"subscribe", MqttSample_CmdSubscribe, "Subscribe the data streams."},
    {"unsubscribe", MqttSample_CmdUnsubscribe, "Unsubscribe the data streams."},
    {"disconnect", MqttSample_CmdDisconnect, "Close the connection."},
    {"cmdret", MqttSample_CmdCmdRet, "Sed the command returen information."},
    {"exit", MqttSample_CmdExit, "Exit the sample."},
    {"help", MqttSample_CmdHelp, "Print the usage of the commands."}
};

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

    bytes = sendmsg((int)(size_t)arg, &msg, 0);
    return bytes;
}

//------------------------------- packet handlers -------------------------------------------
static int MqttSample_HandleConnAck(void *arg, char flags, char ret_code)
{
    printf("Success to connect to the server, flags(%d), code(%d).\n",
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
        printf("   code%d=%02x\n", i, codes[i]);
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
    printf("Recv the command, packet id is %d, cmdid is %s, qos=%d, dup=%d.\n",
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

    err = Mqtt_PackConnectPkt(ctx->mqttbuf, 0, ctx->devid, 1, "WillTopic",
                              "will message-xxxx", 17, MQTT_QOS_LEVEL0, 0, ctx->proid,
                              ctx->apikey, strlen(ctx->apikey));
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

static int MqttSample_CmdPublish(struct MqttSampleContext *ctx)
{
    int err = 0;
    int64_t ts;

    ts = (int64_t)time(NULL) * 1000;

    err |= Mqtt_PackDataPointStart(ctx->mqttbuf, 1, MQTT_QOS_LEVEL2, 0, 1);
    err |= Mqtt_AppendDPStartObject(ctx->mqttbuf, "dsid", ts);
    err |= Mqtt_AppendDPSubvalueInt(ctx->mqttbuf, "subvalue", 23);
    err |= Mqtt_AppendDPSubvalueDouble(ctx->mqttbuf, "sub2", 23.167);
    err |= Mqtt_AppendDPSubvalueString(ctx->mqttbuf, "str3", "strvalue");
    err |= Mqtt_AppendDPFinishObject(ctx->mqttbuf);
    err |= Mqtt_PackDataPointFinish(ctx->mqttbuf);
    if(err) {
        printf("Failed to pack data point package.\n");
        return -1;
    }

    return 0;
}

static int MqttSample_CmdSubscribe(struct MqttSampleContext *ctx)
{
    int err;
    err = Mqtt_PackSubscribePkt(ctx->mqttbuf, 1, "433223/Bs04OCJioNgpmvjRphRak15j7Z8=/45523/die", MQTT_QOS_LEVEL1);
    if(err != MQTTERR_NOERROR) {
        printf("Critical bug: failed to pack the subscribe packet.\n");
        return -1;
    }

    /* err = Mqtt_AppendSubscribeTopic(ctx->mqttbuf, "433223/Bs04OCJioNgpmvjRphRak15j7Z8=/25267/test-2", MQTT_QOS_LEVEL2); */
    if(err != MQTTERR_NOERROR) {
        printf("Critical bug: failed to append the topic to the "
               "subscribe packet.\n");
        return -1;
    }

    return 0;
}

static int MqttSample_CmdUnsubscribe(struct MqttSampleContext *ctx)
{
    int err;
    err = Mqtt_PackUnsubscribePkt(ctx->mqttbuf, 11, "433223/Bs04OCJioNgpmvjRphRak15j7Z8=/25267/test-1");
    if(err != MQTTERR_NOERROR) {
        printf("Critical bug: failed to pack the unsubscribe packet.\n");
        return -1;
    }

    err = Mqtt_AppendUnsubscribeTopic(ctx->mqttbuf, "433223/Bs04OCJioNgpmvjRphRak15j7Z8=/25267/test-1");
    if(err != MQTTERR_NOERROR) {
        printf("Critical bug: failed to append the topic to the "
               "unsubscribe packet.\n");
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

static int MqttSample_CmdCmdRet(struct MqttSampleContext *ctx)
{
    int err;
    err = Mqtt_PackCmdRetPkt(ctx->mqttbuf, 1, ctx->cmdid,
                             "dkdkkxiiii", 11, 0);
    if(MQTTERR_NOERROR != err) {
        printf("Critical bug: failed to pack the cmd ret packet.\n");
        return -1;
    }

    return 0;
}

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
    const int buf_size = 1024;
    int bytes, i, ret = 0;

    char buf[buf_size];
    if(-1 != ctx->sendedbytes) {
        printf("There are something to be send, please wait a moment to retry.\n");
        return 0;
    }

    bytes = read(STDIN_FILENO, buf, buf_size);
    buf[bytes - 1] = 0;

    for(i = 0; i < sizeof(commands) / sizeof(*commands); ++i) {
        if(strcmp(commands[i].cmd, buf) == 0) {
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
    ctx->host = "183.230.40.39";
    ctx->port = 6002;
    ctx->proid = "25343";
    ctx->devid = "284198";
    ctx->apikey = "HCawppv7FpL6S5F4uCNazF5NufYA";


    err = Mqtt_InitContext(ctx->mqttctx, 1 << 20);
    if(MQTTERR_NOERROR != err) {
        printf("Failed to init the MQTT context errcode is %d", err);
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

int main(int argc, char **argv)
{
    struct MqttSampleContext smpctx[1];
    int evt_cnt;
    const int evt_max_cnt = 2;
    struct epoll_event events[evt_max_cnt];
    int exit;


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

    printf("bye...\n");

    return 0;
}
