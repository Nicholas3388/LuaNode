/*guys, srsly, turn on warnings in the makefile*/
#if defined(__GNUC__)
#pragma GCC diagnostic warning "-Wall"
#pragma GCC diagnostic warning "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

/*-------------------------------------
NEW TIMER API
---------------------------------------

tmr.wdclr() -- not changed
tmr.now()   -- not changed
tmr.time()  -- not changed
tmr.delay() -- not changed
tmr.alarm() -- not changed
tmr.stop()  -- changed, see below. use tmr.unregister for old functionality

tmr.register(id, interval, mode, function)
        bind function with timer and set the interval in ms
        the mode can be:
                tmr.ALARM_SINGLE for a single run alarm
                tmr.ALARM_SEMI for a multiple single run alarm
                tmr.ALARM_AUTO for a repating alarm
        tmr.register does NOT start the timer
        tmr.alarm is a tmr.register & tmr.start macro
tmr.unregister(id)
        stop alarm, unbind function and clean up memory
        not needed for ALARM_SINGLE, as it unregisters itself
tmr.start(id)
        ret: bool
        start a alarm, returns true on success
tmr.stop(id)
        ret: bool
        stops a alarm, returns true on success
        this call dose not free any memory, to do so use tmr.unregister
        stopped alarms can be started with start
tmr.interval(id, interval)
        set alarm interval, running alarm will be restarted
tmr.state(id)
        ret: (bool, int) or nil
        returns alarm status (true=started/false=stopped) and mode
        nil if timer is unregistered
tmr.softwd(int)
        set a negative value to stop the timer
        any other value starts the timer, when the
        countdown reaches zero, the device restarts
        the timer units are seconds
*/

#include "c_types.h"
#include "driver/timer.h"
#include "esp_misc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lauxlib.h"
#include "lrodefs.h"
#include "lrotable.h"
#include "lualib.h"
#include "modules.h"
#include "platform.h"

#define NUM_TMR 7

#define TIMER_MODE_OFF 3
#define TIMER_MODE_SINGLE 0
#define TIMER_MODE_SEMI 2
#define TIMER_MODE_AUTO 1
#define TIMER_IDLE_FLAG (1 << 7)

#define TIMER_DIVIDER 16                              //  Hardware timer clock divider
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

#define TIMER_INTERVAL0_SEC (3.4179)  // sample test interval for the first timer
#define TEST_WITHOUT_RELOAD 0         // testing will be done without auto reload
#define TEST_WITH_RELOAD 1            // testing will be done with auto reload

// well, the following are my assumptions
// why, oh why is there no good documentation
// chinese companies should learn from Atmel
extern void ets_delay_us(uint32_t us);
extern uint32_t system_get_time();
// extern uint32_t system_rtc_clock_cali_proc();
extern uint32_t system_get_rtc_time();
extern void system_soft_wdt_feed();

// in fact lua_State is constant, it's pointless to pass it around
// but hey, whatever, I'll just pass it, still we waste 28B here
typedef struct
{
    // os_timer_t os;
    lua_State* L;
    sint32_t lua_ref;
    uint32_t interval;
    uint8_t mode;
} timer_struct_t;
typedef timer_struct_t* my_timer_t;

typedef struct
{
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

// everybody just love unions! riiiiight?
static union {
    uint64_t block;
    uint32_t part[2];
} rtc_time;
static sint32_t soft_watchdog = -1;
static timer_struct_t alarm_timers[NUM_TMR];
// static os_timer_t rtc_timer;
static xQueueHandle timer_queue;

static void alarm_timer_common(void* arg)
{
    my_timer_t tmr = &alarm_timers[(uint32_t)arg];
    if (tmr->lua_ref == LUA_NOREF || tmr->L == NULL)
        return;
    lua_rawgeti(tmr->L, LUA_REGISTRYINDEX, tmr->lua_ref);
    // if the timer was set to single run we clean up after it
    if (tmr->mode == TIMER_MODE_SINGLE)
    {
        luaL_unref(tmr->L, LUA_REGISTRYINDEX, tmr->lua_ref);
        tmr->lua_ref = LUA_NOREF;
        tmr->mode = TIMER_MODE_OFF;
    }
    else if (tmr->mode == TIMER_MODE_SEMI)
    {
        tmr->mode |= TIMER_IDLE_FLAG;
    }
    lua_call(tmr->L, 0, 0);
}

void tmr_delay_msec(int ms) { vTaskDelay(ms / portTICK_RATE_MS); }

// Lua: tmr.delay( s )	delay seconds
static int tmr_delay_s(lua_State* L)
{
    uint32_t period = luaL_checkinteger(L, 1);
    vTaskDelay(period * (1000 / portTICK_RATE_MS));
    return 0;
}

// Lua: tmr.delay_ms( ms )
static int tmr_delay_ms(lua_State* L)
{
    uint32_t period = luaL_checkinteger(L, 1);
    tmr_delay_msec(period);
    return 0;
}

// Lua: tmr.delay_us( us )
static int tmr_delay(lua_State* L)
{
    sint32_t us = luaL_checkinteger(L, 1);
    if (us <= 0)
        return luaL_error(L, "wrong arg range");
    while (us >= 1000000)
    {
        us -= 1000000;
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    uint32_t remain = (uint32_t)(us / 1000);
    if (remain > 0)
    {
        us = us % 1000;
        vTaskDelay(remain / portTICK_RATE_MS);
    }
    if (us > 0)
    {
        os_delay_us(us);
    }
    return 0;
}

// Lua: tmr.now() , return system timer in us
static int tmr_now(lua_State* L)
{
    uint32_t now = 0x7FFFFFFF & system_get_time();
    lua_pushinteger(L, now);
    return 1;
}

/*
 * Timer group0 ISR handler
 *
 * Note:
 * We don't call the timer API here because they are not declared with
 * IRAM_ATTR. If we're okay with the timer irq not being serviced while SPI
 * flash cache is disabled, we can allocate this interrupt without the
 * ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR timer_group0_isr(void* para)
{
    int timer_idx = (int)para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;
    uint64_t timer_counter_value =
        ((uint64_t)TIMERG0.hw_timer[timer_idx].cnt_high) << 32 | TIMERG0.hw_timer[timer_idx].cnt_low;

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0)
    {
        evt.type = TEST_WITHOUT_RELOAD;
        TIMERG0.int_clr_timers.t0 = 1;
        timer_counter_value += (uint64_t)(TIMER_INTERVAL0_SEC * TIMER_SCALE);
        TIMERG0.hw_timer[timer_idx].alarm_high = (uint32_t)(timer_counter_value >> 32);
        TIMERG0.hw_timer[timer_idx].alarm_low = (uint32_t)timer_counter_value;
    }
    else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1)
    {
        evt.type = TEST_WITH_RELOAD;
        TIMERG0.int_clr_timers.t1 = 1;
    }
    else
    {
        evt.type = -1;  // not supported even type
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, NULL);
}

static void example_tg0_timer_init(int timer_idx, bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on
       alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr, (void*)timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}

// Lua: tmr.register( id, interval, mode, function )
static int tmr_register(lua_State* L)
{
    uint32_t id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID(tmr, id);
    sint32_t interval = luaL_checkinteger(L, 2);
    uint8_t mode = luaL_checkinteger(L, 3);
    // validate arguments
    uint8_t args_valid = interval <= 0 ||
                         (mode != TIMER_MODE_SINGLE && mode != TIMER_MODE_SEMI && mode != TIMER_MODE_AUTO) ||
                         (lua_type(L, 4) != LUA_TFUNCTION && lua_type(L, 4) != LUA_TLIGHTFUNCTION);
    if (args_valid)
        return luaL_error(L, "wrong arg range");
    // get the lua function reference
    lua_pushvalue(L, 4);
    sint32_t ref = luaL_ref(L, LUA_REGISTRYINDEX);
    my_timer_t tmr = &alarm_timers[id];
    if (!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF)
    {
        // os_timer_disarm(&tmr->os);
    }
    // there was a bug in this part, the second part of the following condition
    // was missing
    if (tmr->lua_ref != LUA_NOREF && tmr->lua_ref != ref)
        luaL_unref(L, LUA_REGISTRYINDEX, tmr->lua_ref);
    tmr->lua_ref = ref;
    tmr->mode = mode | TIMER_IDLE_FLAG;
    tmr->interval = interval;
    tmr->L = L;
    // os_timer_setfn(&tmr->os, alarm_timer_common, (void*)id);
    example_tg0_timer_init(TIMER_0, TEST_WITHOUT_RELOAD, interval);
    return 0;
}

// Lua: tmr.start( id )
static int tmr_start(lua_State* L)
{
    uint8_t id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID(tmr,id);
    my_timer_t tmr = &alarm_timers[id];
    // we return false if the timer is not idle
    if (!(tmr->mode & TIMER_IDLE_FLAG))
    {
        lua_pushboolean(L, 0);
    }
    else
    {
        tmr->mode &= ~TIMER_IDLE_FLAG;
        // os_timer_arm(&tmr->os, tmr->interval, tmr->mode==TIMER_MODE_AUTO);
        lua_pushboolean(L, 1);
    }
    return 1;
}

// Lua: tmr.alarm( id, interval, repeat, function )
static int tmr_alarm(lua_State* L)
{
    tmr_register(L);
    return tmr_start(L);
}

// Lua: tmr.stop( id )
static int tmr_stop(lua_State* L)
{
    uint8_t id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID(tmr,id);
    my_timer_t tmr = &alarm_timers[id];
    // we return false if the timer is idle (of not registered)
    if (!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF)
    {
        tmr->mode |= TIMER_IDLE_FLAG;
        // os_timer_disarm(&tmr->os);
        lua_pushboolean(L, 1);
    }
    else
    {
        lua_pushboolean(L, 0);
    }
    return 1;
}

// Lua: tmr.unregister( id )
static int tmr_unregister(lua_State* L)
{
    uint8_t id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID(tmr,id);
    my_timer_t tmr = &alarm_timers[id];
    if (!(tmr->mode & TIMER_IDLE_FLAG) && tmr->mode != TIMER_MODE_OFF)
    {
        // os_timer_disarm(&tmr->os);
    }
    if (tmr->lua_ref != LUA_NOREF)
        luaL_unref(L, LUA_REGISTRYINDEX, tmr->lua_ref);
    tmr->lua_ref = LUA_NOREF;
    tmr->mode = TIMER_MODE_OFF;
    return 0;
}

// Lua: tmr.interval( id, interval )
static int tmr_interval(lua_State* L)
{
    uint8_t id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID(tmr,id);
    my_timer_t tmr = &alarm_timers[id];
    sint32_t interval = luaL_checkinteger(L, 2);
    if (interval <= 0)
        return luaL_error(L, "wrong arg range");
    if (tmr->mode != TIMER_MODE_OFF)
    {
        tmr->interval = interval;
        if (!(tmr->mode & TIMER_IDLE_FLAG))
        {
            // os_timer_disarm(&tmr->os);
            // os_timer_arm(&tmr->os, tmr->interval,
            // tmr->mode==TIMER_MODE_AUTO);
        }
    }
    return 0;
}

// Lua: tmr.state( id )
static int tmr_state(lua_State* L)
{
    uint8_t id = luaL_checkinteger(L, 1);
    // MOD_CHECK_ID(tmr,id);
    my_timer_t tmr = &alarm_timers[id];
    if (tmr->mode == TIMER_MODE_OFF)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushboolean(L, (tmr->mode & TIMER_IDLE_FLAG) == 0);
    lua_pushinteger(L, tmr->mode & (~TIMER_IDLE_FLAG));
    return 2;
}

/*I left the led comments 'couse I don't know
why they are here*/

// extern void update_key_led();
// Lua: tmr.wdclr()
static int tmr_wdclr(lua_State* L)
{
    // system_soft_wdt_feed ();
    // update_key_led();
    return 0;
}

// system_rtc_clock_cali_proc() returns
// a fixed point value (12 bit fraction part)
// it tells how many rtc clock ticks represent 1us.
// the high 64 bits of the uint64_t multiplication
// are unnedded (I did the math)
static uint32_t rtc2sec(uint64_t rtc)
{
    uint64_t aku = system_get_rtc_time();
    aku *= rtc;
    return (aku >> 12) / 1000000;
}

// the following function workes, I just wrote it and didn't use it.
/*static uint64_t sec2rtc(uint32_t sec){
        uint64_t aku = (1<<20)/system_rtc_clock_cali_proc();
        aku *= sec;
        return (aku>>8)*1000000;
}*/

inline static void rtc_timer_update()
{
    uint32_t current = system_get_rtc_time();
    if (rtc_time.part[0] > current)  // overflow check
        rtc_time.part[1]++;
    rtc_time.part[0] = current;
}

void rtc_callback(void* arg)
{
    rtc_timer_update();
    if (soft_watchdog > 0)
    {
        soft_watchdog--;
        if (soft_watchdog == 0)
            esp_restart();
    }
}

// Lua: tmr.time() , return rtc time in second
static int tmr_time(lua_State* L)
{
    rtc_timer_update();
    lua_pushinteger(L, rtc2sec(rtc_time.block));
    return 1;
}

// Lua: tmr.softwd( value )
static int tmr_softwd(lua_State* L)
{
    soft_watchdog = luaL_checkinteger(L, 1);
    return 0;
}

// Module function map

const LUA_REG_TYPE tmr_map[] = {{LSTRKEY("delay_us"), LFUNCVAL(tmr_delay)},
                                {LSTRKEY("delay_ms"), LFUNCVAL(tmr_delay_ms)},
                                {LSTRKEY("delay"), LFUNCVAL(tmr_delay_s)},
                                {LSTRKEY("now"), LFUNCVAL(tmr_now)},
                                {LSTRKEY("wdclr"), LFUNCVAL(tmr_wdclr)},
                                {LSTRKEY("softwd"), LFUNCVAL(tmr_softwd)},
                                {LSTRKEY("time"), LFUNCVAL(tmr_time)},
                                {LSTRKEY("register"), LFUNCVAL(tmr_register)},
                                {LSTRKEY("alarm"), LFUNCVAL(tmr_alarm)},
                                {LSTRKEY("start"), LFUNCVAL(tmr_start)},
                                {LSTRKEY("stop"), LFUNCVAL(tmr_stop)},
                                {LSTRKEY("unregister"), LFUNCVAL(tmr_unregister)},
                                {LSTRKEY("state"), LFUNCVAL(tmr_state)},
                                {LSTRKEY("interval"), LFUNCVAL(tmr_interval)},
#if LUA_OPTIMIZE_MEMORY > 0
                                {LSTRKEY("ALARM_SINGLE"), LNUMVAL(TIMER_MODE_SINGLE)},
                                {LSTRKEY("ALARM_SEMI"), LNUMVAL(TIMER_MODE_SEMI)},
                                {LSTRKEY("ALARM_AUTO"), LNUMVAL(TIMER_MODE_AUTO)},
#endif
                                {LNILKEY, LNILVAL}};

LUALIB_API int luaopen_tmr(lua_State* L)
{
#if LUA_OPTIMIZE_MEMORY > 0
    return 0;
#else
    luaL_register(L, LUA_TMRLIBNAME, tmr_map);
    return 1;
#endif
}

static void timer_evt_task(void* arg)
{
    while (1)
    {
        timer_event_t evt;
        xQueueReceive(timer_queue, &evt, portMAX_DELAY);

        /* Print information that the timer reported an event */
        if (evt.type == TEST_WITHOUT_RELOAD)
        {
            printf("\n    Example timer without reload\n");
        }
        else if (evt.type == TEST_WITH_RELOAD)
        {
            printf("\n    Example timer with auto reload\n");
        }
        else
        {
            printf("\n    UNKNOWN EVENT TYPE\n");
        }
        printf("Group[%d], timer[%d] alarm event\n", evt.timer_group, evt.timer_idx);

        /* Print the timer values passed by event */
        printf("------- EVENT TIME --------\n");
        // print_timer_counter(evt.timer_counter_value);

        /* Print the timer values as visible by this task */
        printf("-------- TASK TIME --------\n");
        uint64_t task_counter_value;
        timer_get_counter_value(evt.timer_group, evt.timer_idx, &task_counter_value);
        // print_timer_counter(task_counter_value);
    }
}

void tmr_init(void)
{
    // timer task init
    printf("timer task init\r\n");
    timer_queue = xQueueCreate(10, sizeof(timer_event_t));
    xTaskCreate(timer_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);
}