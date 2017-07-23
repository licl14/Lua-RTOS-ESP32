/*
 * Lua RTOS, timer MODULE
 *
 * Copyright (C) 2015 - 2017
 * IBEROXARXA SERVICIOS INTEGRALES, S.L.
 * 
 * Author: Jaume Olivé (jolive@iberoxarxa.com / jolive@whitecatboard.org)
 * 
 * All rights reserved.  
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and this
 * permission notice and warranty disclaimer appear in supporting
 * documentation, and that the name of the author not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * The author disclaim all warranties with regard to this
 * software, including all implied warranties of merchantability
 * and fitness.  In no event shall the author be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether
 * in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of
 * this software.
 */

#include "luartos.h"

#if CONFIG_LUA_RTOS_LUA_USE_TMR

#include "freertos/FreeRTOS.h"
#include "freertos/adds.h"

#include "esp_attr.h"

#include "lua.h"
#include "lauxlib.h"
#include "modules.h"
#include "error.h"
#include "tmr.h"

#include <string.h>
#include <unistd.h>

#include <sys/delay.h>

#include <drivers/timer.h>
#include <drivers/cpu.h>

typedef struct {
	int callback;
	lua_State *L;
} tmr_callback_t;

static tmr_callback_t callbacks[CPU_LAST_TIMER + 1];

static void callback_func(void *arg) {
	int unit = (int)arg;
	lua_State *TL;
	lua_State *L;
	int tref;

	if (callbacks[unit].callback != LUA_NOREF) {
	    L = pvGetLuaState();
	    TL = lua_newthread(L);

	    tref = luaL_ref(L, LUA_REGISTRYINDEX);

	    lua_rawgeti(L, LUA_REGISTRYINDEX, callbacks[unit].callback);
	    lua_xmove(L, TL, 1);
	    lua_pcall(TL, 0, 0, 0);
        luaL_unref(TL, LUA_REGISTRYINDEX, tref);
    }

}

static int ltmr_delay( lua_State* L ) {
    unsigned long long period;

    period = luaL_checkinteger( L, 1 );
    
    delay(period * 1000);
    
    return 0;
}

static int ltmr_delay_ms( lua_State* L ) {
    unsigned long long period;

    period = luaL_checkinteger( L, 1 );

    delay(period);
        
    return 0;
}

static int ltmr_delay_us( lua_State* L ) {
    unsigned long long period;

    period = luaL_checkinteger( L, 1 );
    udelay(period);
    
    return 0;
}

static int ltmr_sleep( lua_State* L ) {
    unsigned long long period;

    period = luaL_checkinteger( L, 1 );
    sleep(period);
    return 0;
}

static int ltmr_sleep_ms( lua_State* L ) {
    unsigned long long period;

    period = luaL_checkinteger( L, 1 );
    usleep(period * 1000);
    return 0;
}

static int ltmr_sleep_us( lua_State* L ) {
    unsigned long long period;

    period = luaL_checkinteger( L, 1 );
    usleep(period);
    
    return 0;
}

static int ltmr_hw_attach( lua_State* L ) {
	driver_error_t *error;

	int id = luaL_checkinteger(L, 1);
	uint32_t micros = luaL_checkinteger(L, 2);
	if (micros < 500) {
		return luaL_exception(L, TIMER_ERR_INVALID_PERIOD);
	}

	luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);

    int callback = luaL_ref(L, LUA_REGISTRYINDEX);

	callbacks[id].callback = callback;
	callbacks[id].L = L;

    tmr_userdata *tmr = (tmr_userdata *)lua_newuserdata(L, sizeof(tmr_userdata));
    if (!tmr) {
       	return luaL_exception(L, TIMER_ERR_NOT_ENOUGH_MEMORY);
    }

    tmr->type = TmrHW;

    if ((error = tmr_setup(id, micros, callback_func, 1))) {
    	return luaL_driver_error(L, error);
    }

    tmr->unit = id;

    luaL_getmetatable(L, "tmr.timer");
    lua_setmetatable(L, -2);

    return 1;
}

static int ltmr_sw_attach( lua_State* L ) {
	tmr_userdata *tmr = (tmr_userdata *)lua_newuserdata(L, sizeof(tmr_userdata));
    if (!tmr) {
       	return luaL_exception(L, TIMER_ERR_NOT_ENOUGH_MEMORY);
    }

    tmr->type = TmrSW;

    luaL_getmetatable(L, "tmr.timer");
    lua_setmetatable(L, -2);

    return 1;
}

static int ltmr_attach( lua_State* L ) {
	if ((lua_gettop(L) == 3)) {
		return ltmr_hw_attach(L);
	} else {
		return ltmr_sw_attach(L);
	}
}

static int ltmr_start( lua_State* L ) {
	driver_error_t *error;
	tmr_userdata *tmr = NULL;

    tmr = (tmr_userdata *)luaL_checkudata(L, 1, "tmr.timer");
    luaL_argcheck(L, tmr, 1, "tmr expected");

    if (tmr->type == TmrHW) {
    	if ((error = tmr_start(tmr->unit))) {
    		return luaL_driver_error(L, error);
    	}
    }

    return 0;
}

static int ltmr_stop( lua_State* L ) {
	driver_error_t *error;
	tmr_userdata *tmr = NULL;

    tmr = (tmr_userdata *)luaL_checkudata(L, 1, "tmr.timer");
    luaL_argcheck(L, tmr, 1, "tmr expected");

    if (tmr->type == TmrHW) {
    	if ((error = tmr_stop(tmr->unit))) {
    		return luaL_driver_error(L, error);
    	}
    }

    return 0;
}

static const LUA_REG_TYPE tmr_timer_map[] = {
	{ LSTRKEY( "start" ),			LFUNCVAL( ltmr_start    ) },
	{ LSTRKEY( "stop" ),			LFUNCVAL( ltmr_stop     ) },
    { LSTRKEY( "__metatable" ),	    LROVAL  ( tmr_timer_map ) },
	{ LSTRKEY( "__index"     ),     LROVAL  ( tmr_timer_map ) },
    { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE tmr_map[] = {
	{ LSTRKEY( "attach" ),			LFUNCVAL( ltmr_attach ) },
    { LSTRKEY( "delay" ),			LFUNCVAL( ltmr_delay ) },
    { LSTRKEY( "delayms" ),			LFUNCVAL( ltmr_delay_ms ) },
    { LSTRKEY( "delayus" ),			LFUNCVAL( ltmr_delay_us ) },
    { LSTRKEY( "sleep" ),			LFUNCVAL( ltmr_sleep ) },
    { LSTRKEY( "sleepms" ),			LFUNCVAL( ltmr_sleep_ms ) },
    { LSTRKEY( "sleepus" ),			LFUNCVAL( ltmr_sleep_us ) },
	TMR_TMR0
	TMR_TMR1
	TMR_TMR2
	TMR_TMR3
    { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_tmr( lua_State *L ) {
	memset(callbacks, 0, sizeof(callbacks));

    luaL_newmetarotable(L,"tmr.timer", (void *)tmr_timer_map);
	return 0;
}

MODULE_REGISTER_MAPPED(TMR, tmr, tmr_map, luaopen_tmr);

/*

function blink()
	if (led_on) then
		pio.pin.sethigh(pio.GPIO26)
		led_on = false
	else
		pio.pin.setlow(pio.GPIO26)
		led_on = true
	end
end

led_on = false
pio.pin.setdir(pio.OUTPUT, pio.GPIO26)

t0 = tmr.attach(tmr.TMR0, 50000, blink)
t0:start()
t0:stop()

 */
#endif
