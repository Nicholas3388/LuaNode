
/**
 * mqtt module for Lua
 *
 * Nicholas3388
 * 2017. 02.22
 */
#include "mqtt.h"

#include "lauxlib.h"
#include "lrotable.h"
#include "lua.h"
#include "lualib.h"
#include "platform.h"
#include "user_config.h"

#define MQTT_CMD_TIMEOUT 5000  // 5s
#define MQTT_YIELD_TMIE 1000   // 1s

#define MAX_MQTT_NUM 3
#define TOPIC_BUFF_SIZE 64
#define DATA_BUFF_SIZE 768

#define QOS0 0
#define QOS1 1
#define QOS2 2

//#define mqtt_log(M, ...) printf(M, ##__VA_ARGS__)
#define mqtt_log(M, ...)

typedef struct
{
    mqtt_client *pClient;
    int cb_ref_data;
    int cb_ref_connect;
    int cb_ref_disconnect;
    int cb_ref_publish;
    int cb_ref_reconnect;
    int cb_ref_subscribe;
} mqtt_t;

static mqtt_t handle[MAX_MQTT_NUM];
bool mqtt_thread_is_started = false;

static lua_State *gL = NULL;
static char topic_buf[TOPIC_BUFF_SIZE] = {0};
static char data_buf[DATA_BUFF_SIZE] = {0};

static void messageArrived(/*MessageData* md*/)
{
    /*mqtt_log("messageArrived Called\r\n");
    MQTTMessage* message = md->message;
    mqtt_log("messageArrived: [%.*s]\t [%d][%.*s]\r\n",
                    md->topicName->lenstring.len, md->topicName->lenstring.data,
                    (int)message->payloadlen,
                    (int)message->payloadlen, (char*)message->payload);
    for(int i=0;i<MAX_MQTT_NUM;i++)
    {
      if(pmqtt[i]==NULL || pmqtt[i]->cb_ref_message == LUA_NOREF) continue;
        lua_rawgeti(gL, LUA_REGISTRYINDEX,  pmqtt[i]->cb_ref_message);
        lua_pushlstring(gL,(char
    const*)md->topicName->lenstring.data,md->topicName->lenstring.len);
        lua_pushlstring(gL,(char
    const*)message->payload,(int)message->payloadlen); lua_call(gL, 2, 0);
    }*/
}

static void closeMqtt(int id)
{
    printf("[mqtt:%d]closeMqtt\r\n", id);
    if (handle[id].pClient == NULL)
        return;
}

void connected_cb(void *self, void *params)
{
    int i;
    mqtt_client *client = (mqtt_client *)self;
    for (i = 0; i < MAX_MQTT_NUM; i++)
    {
        if (handle[i].pClient == client)
        {
            if (handle[i].cb_ref_connect != LUA_NOREF)
            {
                lua_rawgeti(gL, LUA_REGISTRYINDEX, handle[i].cb_ref_connect);
                lua_call(gL, 0, 0);
            }
            return;
        }
    }
}

void disconnected_cb(void *self, void *params)
{
    int i;
    mqtt_client *client = (mqtt_client *)self;
    for (i = 0; i < MAX_MQTT_NUM; i++)
    {
        if (handle[i].pClient == client)
        {
            if (handle[i].cb_ref_disconnect != LUA_NOREF)
            {
                lua_rawgeti(gL, LUA_REGISTRYINDEX, handle[i].cb_ref_disconnect);
                lua_call(gL, 0, 0);
            }
            return;
        }
    }
}
void reconnect_cb(void *self, void *params)
{
    int i;
    mqtt_client *client = (mqtt_client *)self;
    for (i = 0; i < MAX_MQTT_NUM; i++)
    {
        if (handle[i].pClient == client)
        {
            if (handle[i].cb_ref_reconnect != LUA_NOREF)
            {
                lua_rawgeti(gL, LUA_REGISTRYINDEX, handle[i].cb_ref_reconnect);
                lua_call(gL, 0, 0);
            }
            return;
        }
    }
}

void subscribe_cb(void *self, void *params)
{
    int i;
    mqtt_client *client = (mqtt_client *)self;
    for (i = 0; i < MAX_MQTT_NUM; i++)
    {
        if (handle[i].pClient == client)
        {
            if (handle[i].cb_ref_subscribe != LUA_NOREF)
            {
                lua_rawgeti(gL, LUA_REGISTRYINDEX, handle[i].cb_ref_subscribe);
                lua_call(gL, 0, 0);
            }
            return;
        }
    }
}

void publish_cb(void *self, void *params)
{
    mqtt_client *client = (mqtt_client *)self;
    mqtt_event_data_t *event_data = (mqtt_event_data_t *)params;

    if (event_data->data_offset == 0)
    {
        memcpy(topic_buf, event_data->topic, event_data->topic_length);
        topic_buf[event_data->topic_length] = 0;
    }

    memcpy(data_buf, event_data->data, event_data->data_length);
    data_buf[event_data->data_length] = 0;

    int i;
    for (i = 0; i < MAX_MQTT_NUM; i++)
    {
        if (handle[i].pClient == client)
        {
            if (handle[i].cb_ref_publish != LUA_NOREF)
            {
                lua_rawgeti(gL, LUA_REGISTRYINDEX, handle[i].cb_ref_publish);
                lua_pushlstring(gL, (char const *)topic_buf, (int)(event_data->topic_length));
                lua_pushlstring(gL, (char const *)data_buf, (int)(event_data->data_length));
                lua_call(gL, 2, 0);
            }
            return;
        }
    }
}

void data_cb(void *self, void *params)
{
    mqtt_client *client = (mqtt_client *)self;
    mqtt_event_data_t *event_data = (mqtt_event_data_t *)params;

    if (event_data->data_offset == 0)
    {
        memcpy(topic_buf, event_data->topic, event_data->topic_length);
        topic_buf[event_data->topic_length] = 0;
        printf("[APP] Publish topic: %s\n", topic_buf);
    }

    memcpy(data_buf, event_data->data, event_data->data_length);
    data_buf[event_data->data_length] = 0;
    printf("[APP] Publish data[%d/%d bytes], content: %s\n", event_data->data_length + event_data->data_offset,
           event_data->data_total_length, data_buf);

    int i;
    for (i = 0; i < MAX_MQTT_NUM; i++)
    {
        if (handle[i].pClient == client)
        {
            if (handle[i].cb_ref_data != LUA_NOREF)
            {
                lua_rawgeti(gL, LUA_REGISTRYINDEX, handle[i].cb_ref_data);
                lua_pushlstring(gL, (char const *)topic_buf, (int)(event_data->topic_length));
                lua_pushlstring(gL, (char const *)data_buf, (int)(event_data->data_length));
                lua_call(gL, 2, 0);
            }
            return;
        }
    }
}

// mqttClt = mqtt.new(host,client_id,keepalive,connect_cb,[user],[pass])
static int lmqtt_new(lua_State *L)
{
    int i = 0, idx = 0;
    char *host = NULL;
    char *user = NULL;
    char *pass = NULL;
    size_t sl = 0;
    host = luaL_checklstring(L, 1, &sl);
    if (host == NULL)
        return luaL_error(L, "wrong arg 1 type");

    char const *clientid = luaL_checklstring(L, 2, &sl);
    if (clientid == NULL)
        return luaL_error(L, "wrong arg 2 type");

    unsigned keepalive;
    keepalive = luaL_checkinteger(L, 3);

    for (i = 0; i < MAX_MQTT_NUM; i++)
    {
        if (handle[i].pClient == NULL)
        {
            idx = i;
            break;
        }
    }
    if (i >= MAX_MQTT_NUM)
    {
        return luaL_error(L, "Max MQTT Number is reached");
    }

    handle[idx].cb_ref_data = LUA_NOREF;
    handle[idx].cb_ref_connect = LUA_NOREF;
    handle[idx].cb_ref_disconnect = LUA_NOREF;
    handle[idx].cb_ref_publish = LUA_NOREF;
    handle[idx].cb_ref_reconnect = LUA_NOREF;
    handle[idx].cb_ref_subscribe = LUA_NOREF;

    // get connect callback
    if (lua_type(L, 4) == LUA_TFUNCTION || lua_type(L, 4) == LUA_TLIGHTFUNCTION)
    {
        lua_pushvalue(L, 4);
        if (handle[idx].cb_ref_connect != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[idx].cb_ref_connect);
        }
        handle[idx].cb_ref_connect = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        handle[idx].cb_ref_connect = LUA_NOREF;
        // return luaL_error( L, "callback function needed" );
    }

    if (lua_gettop(L) > 4)
    {
        user = luaL_checklstring(L, 5, &sl);
        pass = luaL_checklstring(L, 6, &sl);
    }
    int k = 0;
    for (k = 0; k < MAX_MQTT_NUM; k++)
    {
        if (handle[k].pClient == NULL)
            break;
    }
    if (k == MAX_MQTT_NUM)
        return luaL_error(L, "Max MQTT Number is reached");
    mqtt_settings *settings = (mqtt_settings *)malloc(sizeof(mqtt_settings));
    memset(settings, 0, sizeof(mqtt_settings));
#if defined(CONFIG_MQTT_SECURITY_ON)
    settings->port = 8883,  // encrypted
#else
    settings->port = 1883,  // unencrypted
#endif
        settings->clean_session = 0;
    settings->keepalive = keepalive;
    settings->lwt_qos = QOS0;
    settings->lwt_retain = 0;
    settings->connected_cb = connected_cb;
    settings->disconnected_cb = disconnected_cb;
    settings->reconnect_cb = reconnect_cb;
    settings->subscribe_cb = subscribe_cb;
    settings->publish_cb = publish_cb;
    settings->data_cb = data_cb;

    if (strlen(host) <= CONFIG_MQTT_MAX_HOST_LEN)
    {
        memcpy(settings->host, host, CONFIG_MQTT_MAX_HOST_LEN);
    }
    else
    {
        printf("host too long\n");
    }
    if (clientid != NULL && strlen(clientid) <= CONFIG_MQTT_MAX_CLIENT_LEN)
    {
        memcpy(settings->client_id, clientid, strlen(clientid));
    }
    else
    {
    }
    if (user != NULL && strlen(user) <= CONFIG_MQTT_MAX_USERNAME_LEN)
    {
        memcpy(settings->username, user, strlen(user));
    }
    else
    {
        printf("user name too long\n");
    }
    if (pass != NULL && strlen(pass) <= CONFIG_MQTT_MAX_PASSWORD_LEN)
    {
        memcpy(settings->password, pass, strlen(pass));
    }
    else
    {
        printf("password too long\n");
    }

    gL = L;
    handle[idx].pClient = mqtt_start(settings);

    lua_pushinteger(L, idx);
    return 1;
}

// mqtt.close(mqttClt)
static int lmqtt_close(lua_State *L)
{
    unsigned id = luaL_checkinteger(L, 1);
    if (id >= MAX_MQTT_NUM)
        return luaL_error(L, "mqttClt arg is wrong!");
    if (handle[id].pClient != NULL)
    {
        // pmqtt[mqttClt]->reqClose=true;
        if (handle[id].cb_ref_data != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_data);
            handle[id].cb_ref_data = LUA_NOREF;
        }
        if (handle[id].cb_ref_disconnect != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_disconnect);
            handle[id].cb_ref_disconnect = LUA_NOREF;
        }
        if (handle[id].cb_ref_reconnect != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_reconnect);
            handle[id].cb_ref_reconnect = LUA_NOREF;
        }
        if (handle[id].cb_ref_publish != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_publish);
            handle[id].cb_ref_publish = LUA_NOREF;
        }
        if (handle[id].cb_ref_connect != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_connect);
            handle[id].cb_ref_connect = LUA_NOREF;
        }
        if (handle[id].cb_ref_subscribe != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_subscribe);
            handle[id].cb_ref_subscribe = LUA_NOREF;
        }
        free((handle[id].pClient)->settings);
        mqtt_destroy(handle[id].pClient);
        closeMqtt(id);
    }
    return 0;
}

// mqtt.subscribe(id, topic, QoS)
static int lmqtt_subscribe(lua_State *L)
{
    unsigned id = luaL_checkinteger(L, 1);
    if (handle[id].pClient == NULL || id >= MAX_MQTT_NUM)
        return luaL_error(L, "mqttClt arg is wrong!");
    size_t sl = 0;
    char const *topic = luaL_checklstring(L, 2, &sl);
    if (topic == NULL)
        return luaL_error(L, "wrong arg type");

    unsigned qos = luaL_checkinteger(L, 3);
    if (!(qos == QOS0 || qos == QOS1 || qos == QOS2))
        return luaL_error(L, "QoS wrong arg type");

    mqtt_subscribe(handle[id].pClient, topic, qos);

    return 0;
}

// mqtt.unsubscribe(id,topic)
static int lmqtt_unsubscribe(lua_State *L)
{
    unsigned id = luaL_checkinteger(L, 1);
    if (handle[id].pClient == NULL || id >= MAX_MQTT_NUM)
        return luaL_error(L, "mqttClt arg is wrong!");

    size_t sl = 0;
    char const *topic = luaL_checklstring(L, 2, &sl);
    if (topic == NULL)
        return luaL_error(L, "wrong arg type");

    mqtt_unsubscribe(handle[id].pClient, topic);
    return 0;
}

// mqtt.publish(id,topic, data)
static int lmqtt_publish(lua_State *L)
{
    unsigned id = luaL_checkinteger(L, 1);
    if (handle[id].pClient == NULL || id >= MAX_MQTT_NUM)
        return luaL_error(L, "mqttClt arg is wrong!");
    size_t sl = 0;
    char const *topic = luaL_checklstring(L, 2, &sl);
    if (topic == NULL)
        return luaL_error(L, "wrong arg type");

    char const *data = luaL_checklstring(L, 3, &sl);
    if (data == NULL)
        return luaL_error(L, "wrong arg type");

    mqtt_publish(handle[id].pClient, topic, data, strlen(data), 0, 0);
    return 0;
}

// mqtt.on(handle,'connect',function())
// mqtt.on(handle,'disconnect',function())
// mqtt.on(handle,'data',function(topic, data))     // the callback is different
// from the others mqtt.on(handle,'reconnect',function())
// mqtt.on(handle,'subscribe',function())
// mqtt.on(handle,'publish',function())
//  different topics with same callback
static int lmqtt_on(lua_State *L)
{
    unsigned id = luaL_checkinteger(L, 1);
    if (handle[id].pClient == NULL || id >= MAX_MQTT_NUM)
        return luaL_error(L, "mqttClt arg is wrong!");
    size_t sl = 0;
    char const *method = luaL_checklstring(L, 2, &sl);
    if (method == NULL)
        return luaL_error(L, "wrong arg type");

    if (lua_type(L, 3) == LUA_TFUNCTION || lua_type(L, 3) == LUA_TLIGHTFUNCTION)
    {
        lua_pushvalue(L, 3);
    }
    else
    {
        return luaL_error(L, "callback function needed");
    }

    if (strcmp(method, "data") == 0 && sl == strlen("data"))
    {
        if (handle[id].cb_ref_data != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_data);
        }
        handle[id].cb_ref_data = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else if (strcmp(method, "disconnect") == 0 && sl == strlen("disconnect"))
    {
        if (handle[id].cb_ref_disconnect != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_disconnect);
        }
        handle[id].cb_ref_disconnect = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else if (strcmp(method, "publish") == 0 && sl == strlen("publish"))
    {
        if (handle[id].cb_ref_publish != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_publish);
        }
        handle[id].cb_ref_publish = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else if (strcmp(method, "reconnect") == 0 && sl == strlen("reconnect"))
    {
        if (handle[id].cb_ref_reconnect != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_reconnect);
        }
        handle[id].cb_ref_reconnect = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else if (strcmp(method, "connect") == 0 && sl == strlen("connect"))
    {
        if (handle[id].cb_ref_connect != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_connect);
        }
        handle[id].cb_ref_connect = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else if (strcmp(method, "subscribe") == 0 && sl == strlen("subscribe"))
    {
        if (handle[id].cb_ref_subscribe != LUA_NOREF)
        {
            luaL_unref(L, LUA_REGISTRYINDEX, handle[id].cb_ref_subscribe);
        }
        handle[id].cb_ref_subscribe = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        return luaL_error(L, "wrong method");
    }
    return 0;
}
/*
mqttClt = mqtt.new(clientid,keepalive,user,pass)
mqtt.start(mqttClt, server, port)
mqtt.on(mqttClt,'connect',function())
mqtt.on(mqttClt,'offline',function())
//different topics with same callback
mqtt.on(mqttClt,'message',cb_messagearrived(topic,message))
mqtt.close(mqttClt)
mqtt.publish(mqttClt,topic,QoS, data)
mqtt.subscribe(mqttClt,topic,QoS,cb_messagearrived(topic,message))
mqtt.unsubscribe(mqttClt,topic)
*/

#include "lrodefs.h"
const LUA_REG_TYPE mqtt_map[] = {{LSTRKEY("new"), LFUNCVAL(lmqtt_new)},
                                 {LSTRKEY("close"), LFUNCVAL(lmqtt_close)},
                                 {LSTRKEY("subscribe"), LFUNCVAL(lmqtt_subscribe)},
                                 {LSTRKEY("unsubscribe"), LFUNCVAL(lmqtt_unsubscribe)},
                                 {LSTRKEY("publish"), LFUNCVAL(lmqtt_publish)},
                                 {LSTRKEY("on"), LFUNCVAL(lmqtt_on)},
#if LUA_OPTIMIZE_MEMORY > 0
                                 {LSTRKEY("QOS0"), LNUMVAL(QOS0)},
                                 {LSTRKEY("QOS1"), LNUMVAL(QOS1)},
                                 {LSTRKEY("QOS2"), LNUMVAL(QOS2)},
#endif
                                 {LNILKEY, LNILVAL}};

int luaopen_mqtt(lua_State *L) { return 0; }
