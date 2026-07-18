/**
    -----------------------------------------------------------

    Project JingWei
    wpe jw_wpe_input.h    2026/07/18

    @link    : https://github.com/shezw/jingwei
    @author  : shezw
    @email   : hello@shezw.com

    -----------------------------------------------------------
*/

#ifndef JW_WPE_INPUT_H
#define JW_WPE_INPUT_H

#include <glib.h>
#include <wpe/wpe-platform.h>

typedef struct jw_wpe_modifier_state {
    guint pressed_keys;
    WPEModifiers modifiers;
    gboolean caps_lock_key_pressed;
} jw_wpe_modifier_state_t;

void jw_wpe_modifier_state_reset(jw_wpe_modifier_state_t *state);
WPEModifiers jw_wpe_modifier_state_update(
    jw_wpe_modifier_state_t *state,
    guint keysym,
    gboolean pressed);
WPEModifiers jw_wpe_modifier_state_get(
    const jw_wpe_modifier_state_t *state);

#endif
