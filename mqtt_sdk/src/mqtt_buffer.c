#include "mqtt/mqtt_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mqtt/mqtt.h"

static const uint32_t MQTT_MIN_EXTENT_SIZE = 1024;

void MqttBuffer_Init(struct MqttBuffer *buf)
{
    buf->first_ext = NULL;
    buf->last_ext = NULL;
    buf->available_bytes = 0;
    buf->allocations = NULL;
    buf->alloc_count = 0;
    buf->alloc_max_count = 0;
    buf->first_available = NULL;
    buf->buffered_bytes = 0;
}

void MqttBuffer_Destroy(struct MqttBuffer *buf)
{
    MqttBuffer_Reset(buf);
}

void MqttBuffer_Reset(struct MqttBuffer *buf)
{
    uint32_t i;
    for(i = 0; i < buf->alloc_count; ++i) {
        free(buf->allocations[i]);
    }
    
    free(buf->allocations);

    MqttBuffer_Init(buf);
}

struct MqttExtent *MqttBuffer_AllocExtent(struct MqttBuffer *buf, uint32_t bytes)
{
    struct MqttExtent *ext;
    uint32_t aligned_bytes = bytes + sizeof(struct MqttExtent);
    aligned_bytes = aligned_bytes + (MQTT_DEFAULT_ALIGNMENT -
        (aligned_bytes % MQTT_DEFAULT_ALIGNMENT)) % MQTT_DEFAULT_ALIGNMENT;

    if(buf->available_bytes < aligned_bytes) {
        uint32_t alloc_bytes;
        char *chunk;

        if(buf->alloc_count == buf->alloc_max_count) {
            uint32_t max_count = buf->alloc_max_count * 2 + 1;
            char **tmp = (char**)malloc(max_count * sizeof(char**));
            if(NULL == tmp) {
                return NULL;
            }

            memset(tmp, 0, max_count * sizeof(char**));
            memcpy(tmp, buf->allocations, buf->alloc_max_count * sizeof(char**));
            free(buf->allocations);

            buf->alloc_max_count = max_count;
            buf->allocations = tmp;
        }

        alloc_bytes = aligned_bytes < MQTT_MIN_EXTENT_SIZE ? MQTT_MIN_EXTENT_SIZE : aligned_bytes;
        chunk = (char*)malloc(alloc_bytes);
        if(NULL == chunk) {
            return NULL;
        }

        buf->alloc_count += 1;
        buf->allocations[buf->alloc_count - 1] = chunk;
        buf->available_bytes = alloc_bytes;
        buf->first_available = chunk;
    }

    assert(buf->available_bytes >= bytes);
    assert(buf->alloc_count > 0);

    ext = (struct MqttExtent*)(buf->first_available);
    ext->len = bytes;
    ext->payload = buf->first_available + sizeof(struct MqttExtent);
    ext->next = NULL;

    buf->first_available += aligned_bytes;
    buf->available_bytes -= aligned_bytes;

    return ext;
}

int MqttBuffer_Append(struct MqttBuffer *buf, char *payload, uint32_t size, int own)
{
    const uint32_t bytes = own ? size : sizeof(struct MqttExtent);

   
    struct MqttExtent *ext = MqttBuffer_AllocExtent(buf, bytes);
    if(NULL == ext) {
        return MQTTERR_OUTOFMEMORY;
    }

    if(own) {
        ext->payload = ((char*)ext) + sizeof(struct MqttExtent);
        memcpy(ext->payload, payload, size);
    }
    else {
        ext->payload = payload;
        ext->len = size;
    }

    MqttBuffer_AppendExtent(buf, ext);
    return MQTTERR_NOERROR;

}

void MqttBuffer_AppendExtent(struct MqttBuffer *buf, struct MqttExtent *ext)
{
    ext->next = NULL;
    if(NULL != buf->last_ext) {
        buf->last_ext->next = ext;
        buf->last_ext = ext;
    }
    else {
        assert(NULL == buf->first_ext);
        assert(1 <= buf->alloc_count);

        buf->first_ext = ext;
        buf->last_ext = ext;
    }

    buf->buffered_bytes += ext->len;
}
