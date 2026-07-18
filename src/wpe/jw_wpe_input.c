/*
    JingWei
    src/wpe jw_wpe_input.c    2026-07-18

     ______     __  __     ______     ______     __     __
    /\  ___\   /\ \_\ \   /\  ___\   /\___  \   /\ \  _ \ \
    \ \___  \  \ \  __ \  \ \  __\   \/_/  /__  \ \ \/ ".\ \
     \/\_____\  \ \_\ \_\  \ \_____\   /\_____\  \ \__/".~\_\
      \/_____/   \/_/\/_/   \/_____/   \/_____/   \/_/   \/_/.com

    @link    : https://github.com/shezw/JingWei
    @author  : shezw
    @email   : hello@shezw.com
*/

#include "jw_wpe_input.h"

enum jw_wpe_modifier_key {
    JW_WPE_CONTROL_LEFT = 1U << 0,
    JW_WPE_CONTROL_RIGHT = 1U << 1,
    JW_WPE_SHIFT_LEFT = 1U << 2,
    JW_WPE_SHIFT_RIGHT = 1U << 3,
    JW_WPE_ALT_LEFT = 1U << 4,
    JW_WPE_ALT_RIGHT = 1U << 5,
    JW_WPE_META_LEFT = 1U << 6,
    JW_WPE_META_RIGHT = 1U << 7
};

static guint jw_wpe_modifier_key_bit(guint keysym)
{
    switch (keysym) {
    case WPE_KEY_Control_L:
        return JW_WPE_CONTROL_LEFT;
    case WPE_KEY_Control_R:
        return JW_WPE_CONTROL_RIGHT;
    case WPE_KEY_Shift_L:
        return JW_WPE_SHIFT_LEFT;
    case WPE_KEY_Shift_R:
        return JW_WPE_SHIFT_RIGHT;
    case WPE_KEY_Alt_L:
        return JW_WPE_ALT_LEFT;
    case WPE_KEY_Alt_R:
        return JW_WPE_ALT_RIGHT;
    case WPE_KEY_Meta_L:
        return JW_WPE_META_LEFT;
    case WPE_KEY_Meta_R:
        return JW_WPE_META_RIGHT;
    default:
        return 0;
    }
}

static void jw_wpe_modifier_state_recompute(jw_wpe_modifier_state_t *state)
{
    guint modifiers = state->modifiers & WPE_MODIFIER_KEYBOARD_CAPS_LOCK;

    if ((state->pressed_keys &
            (JW_WPE_CONTROL_LEFT | JW_WPE_CONTROL_RIGHT)) != 0) {
        modifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;
    }
    if ((state->pressed_keys &
            (JW_WPE_SHIFT_LEFT | JW_WPE_SHIFT_RIGHT)) != 0) {
        modifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
    }
    if ((state->pressed_keys &
            (JW_WPE_ALT_LEFT | JW_WPE_ALT_RIGHT)) != 0) {
        modifiers |= WPE_MODIFIER_KEYBOARD_ALT;
    }
    if ((state->pressed_keys &
            (JW_WPE_META_LEFT | JW_WPE_META_RIGHT)) != 0) {
        modifiers |= WPE_MODIFIER_KEYBOARD_META;
    }
    state->modifiers = (WPEModifiers)modifiers;
}

void jw_wpe_modifier_state_reset(jw_wpe_modifier_state_t *state)
{
    g_return_if_fail(state != NULL);

    state->pressed_keys = 0;
    state->modifiers = (WPEModifiers)0;
    state->caps_lock_key_pressed = FALSE;
}

WPEModifiers jw_wpe_modifier_state_update(
    jw_wpe_modifier_state_t *state,
    guint keysym,
    gboolean pressed)
{
    guint key_bit;

    g_return_val_if_fail(state != NULL, (WPEModifiers)0);

    if (keysym == WPE_KEY_Caps_Lock) {
        if (pressed && !state->caps_lock_key_pressed) {
            state->modifiers = (WPEModifiers)(
                state->modifiers ^ WPE_MODIFIER_KEYBOARD_CAPS_LOCK);
        }
        state->caps_lock_key_pressed = pressed;
        return state->modifiers;
    }

    key_bit = jw_wpe_modifier_key_bit(keysym);
    if (key_bit == 0) {
        return state->modifiers;
    }
    if (pressed) {
        state->pressed_keys |= key_bit;
    } else {
        state->pressed_keys &= ~key_bit;
    }
    jw_wpe_modifier_state_recompute(state);
    return state->modifiers;
}

WPEModifiers jw_wpe_modifier_state_get(
    const jw_wpe_modifier_state_t *state)
{
    g_return_val_if_fail(state != NULL, (WPEModifiers)0);
    return state->modifiers;
}
