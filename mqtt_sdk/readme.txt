                    ================
                    中移物联MQTT SDK
                    ================


目录
 (*) 在项目中链接MQTT SDK

     - 引用MQTT SDK的源码
     - 编译MQTT SDK动态库

 (*) 使用MQTT SDK
     - 初始化MQTT SDK上下文
     - 与服务器建立连接
     - 发布数据点
     - 订阅用户自定义topic
     - 取消数据流订阅
     - 处理服务器消息


====================
在项目中链接MQTT SDK
====================

引用MQTT SDK的源码
------------------
如果只需C语言版本的SDK，只需要将mqtt目录下的config.h、mqtt.h、cJSON.h
和mqtt_buffer.h，以及src目录下的mqtt.c、cJSON.c和mqtt_buffer.c添加到您
的工程中。
如果需要C++语言版本的SDK，需将除C语言版所需文件外的
mqtt/mqtt_buf.hpp和mqtt_base.hpp添加到您的项目中。

注意：头文件必须放到mqtt目录下，并将mqtt目录的路径添加到工程
的头文件路径中。


编译MQTT SDK动态库
------------------
要编译MQTT SDK动态库，需要使用到CMake项目构建工具，下载地址为：
http://www.cmake.org/download/ ，待安装完毕后，可进行SDK编译。
以下以linux下编译MQTT SDK为例：
1.打开命令行控制台
2.创建一个目录用于存放cmake所产生的所有文件，以及编译后的动态库，
如：~/mqtt_cmake。
3.切换当前工作目录到新创建的目录： cd ~/mqtt_cmake
4.运行 cmake <mqtt_sdk 根目录路径>，如mqtt_sdk存放在~/mqtt_sdk中，
则运行 cmake ~/mqtt_sdk。如果要生成debug版的sdk，则运行：
cmake -DCMAKE_BUILD_TYPE=Release ~/mqtt_sdk
5. 运行make命令
最终生成的动态库会存放在~/mqtt_sdk/bin目录下

如果要在Windows平台下编译MQTT SDK，可以参考cmake的使用手册，生成
对应的Visual Studio或eclipse等IDE的工程文件


============
使用MQTT SDK
============

初始化MQTT SDK上下文
--------------------
调用Mqtt_InitContext初始化MqttContext，并将设置MqttContext中的回
调函数及关联参数。如sample中的MqttSample_Init函数：
    ...
    ctx->mqttctx->handle_conn_ack = MqttSample_HandleConnAck;
    ctx->mqttctx->handle_conn_ack_arg = ctx;
    ctx->mqttctx->handle_ping_resp = MqttSample_HandlePingResp;
    ctx->mqttctx->handle_ping_resp_arg = ctx;
    ...

与服务器建立连接
----------------
1.创建MqttBuffer, 并通过MqttBuffer_Init进行初始化。
2.调用Mqtt_PackConnectPkt，封装MQTT连接包。
需要注意的是： id需设置为设备ID，user需设置为project ID，password
               需设置为auth-info
3.调用Mqtt_SendPkt发送MQTT连接包
4.调用MqttBuffer_Destroy销毁MQTT连接包
5.will_topic,will_msg,msg_len,will_retain 暂不支持,分别设为:NULL,NULL,0,0

代码示例：
    ...
    MqttBuffer_Init(ctx->mqttbuf);
    ...
    err = Mqtt_PackConnectPkt(ctx->mqttbuf, keep_alive, ctx->devid, 1,
                              NULL, NULL, 0,
                              MQTT_QOS_LEVEL0, 0, ctx->proid,
                              auth_info, strlen(auth_info));
    if(MQTTERR_NOERROR != err) {
        // do some error handling.
    }
    ...
    bytes = Mqtt_SendPkt(ctx->mqttctx, ctx->mqttbuf, 0);
    if(bytes < 0) {
        // do some error handling.
    }
    ...
    MqttBuffer_Destroy(ctx->mqttbuf);
    ...

发布数据点
----------
1.创建MqttBuffer, 并通过MqttBuffer_Init进行初始化。
2.调用Mqtt_PackDataPointBy*封装数据包
3.调用Mqtt_SendPkt发送数据点
4.调用MqttBuffer_Destroy销毁MqttBuffer

代码示例：
    ...
    MqttBuffer_Init(ctx->mqttbuf);
    ...
    const char *str = ",;temperature,2015-03-22 22:31:12,22.5;102;pm2.5,89;10";
    uint32_t size = strlen(str);
    int retain = 0;
    int own = 1;
    int err = MQTTERR_NOERROR;
    err = Mqtt_PackDataPointByString(ctx->mqttbuf, g_pkt_id++, 0, kTypeString, str, size, qos, retain, own);

    if(err) {
        // do some error handling
    }

    bytes = Mqtt_SendPkt(ctx->mqttctx, ctx->mqttbuf, 0);
    if(bytes < 0) {
        // do some error handling.
    }
    ...
    MqttBuffer_Destroy(ctx->mqttbuf);
    ...

订阅用户自定义topic
----------
1.创建MqttBuffer, 并通过MqttBuffer_Init进行初始化
2.调用Mqtt_PackSubscribePkt封装订阅消息包
3.调用Mqtt_SendPkt发送订阅消息包
4.调用MqttBuffer_Destroy销毁MqttBuffer

代码示例：
    ...
    MqttBuffer_Init(ctx->mqttbuf);
    ...
    char **topics;
    ...
    err = Mqtt_PackSubscribePkt(ctx->mqttbuf, 1, MQTT_QOS_LEVEL1, topics, topics_len);
    if(err != MQTTERR_NOERROR) {
        // do some error handling.
    }

    bytes = Mqtt_SendPkt(ctx->mqttctx, ctx->mqttbuf, 0);
    if(bytes < 0) {
        // do some error handling.
    }
    ...
    MqttBuffer_Destroy(ctx->mqttbuf);
    ...

取消数据流订阅
--------------
1.创建MqttBuffer, 并通过MqttBuffer_Init进行初始化
2.调用Mqtt_PackUnsubscribePkt封装订阅消息包
3.调用Mqtt_SendPkt发送订阅消息包
4.调用MqttBuffer_Destroy销毁MqttBuffer

代码示例：
    ...
    MqttBuffer_Init(ctx->mqttbuf);
    ...
    char **topics;
    ...
    err = Mqtt_PackUnsubscribePkt(ctx->mqttbuf, 1, topics, topics_len);
        if(err != MQTTERR_NOERROR) {
        // do some error handling.
    }

    bytes = Mqtt_SendPkt(ctx->mqttctx, ctx->mqttbuf, 0);
    if(bytes < 0) {
        // do some error handling.
    }
    ...
    MqttBuffer_Destroy(ctx->mqttbuf);
    ...

处理服务器消息
--------------
1. 调用Mqtt_RecvPkt接收服务器消息，当收到完整的消息后，Mqtt_RecvPkt
自动调用对应的消息处理函数
2. 在相应的服务器消息处理函数中处理消息

代码示例：
    switch(err = Mqtt_RecvPkt(ctx->mqttctx)) {
    case MQTTERR_ENDOFFILE:
        printf("The connection is disconnected.\n");
        close(ctx->mqttfd);
        epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, ctx->mqttfd, NULL);
        ctx->mqttfd = -1;
        return 0;

    case MQTTERR_IO:
        printf("Send TCP data error: %s.\n", strerror(errno));
        return -1;
    case MQTTERR_NOERROR:
        return 0;

    default:
        printf("Mqtt_RecvPkt error is %d.\n", err);
        return -1;
    }

    static int MqttSample_HandleConnAck(...)
    {
        // do something
        return 0;
    }
