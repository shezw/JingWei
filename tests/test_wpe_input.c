/*
    JingWei
    tests test_wpe_input.c    2026-07-18

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

#include <stdio.h>

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "expectation failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

static gboolean has_modifier(WPEModifiers modifiers, WPEModifiers expected)
{
    return (modifiers & expected) != 0;
}

int main(void)
{
    jw_wpe_modifier_state_t state;
    WPEModifiers modifiers;

    jw_wpe_modifier_state_reset(&state);

    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Shift_L, TRUE);
    EXPECT_TRUE(has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_SHIFT));
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Shift_R, TRUE);
    EXPECT_TRUE(has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_SHIFT));
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Shift_L, FALSE);
    EXPECT_TRUE(has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_SHIFT));
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Shift_R, FALSE);
    EXPECT_TRUE(!has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_SHIFT));

    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Control_L, TRUE);
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Control_R, TRUE);
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Control_L, FALSE);
    EXPECT_TRUE(has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_CONTROL));
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Control_R, FALSE);
    EXPECT_TRUE(!has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_CONTROL));

    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Alt_L, TRUE);
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Alt_R, TRUE);
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Alt_L, FALSE);
    EXPECT_TRUE(has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_ALT));
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Alt_R, FALSE);
    EXPECT_TRUE(!has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_ALT));

    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Meta_L, TRUE);
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Meta_R, TRUE);
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Meta_L, FALSE);
    EXPECT_TRUE(has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_META));
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Meta_R, FALSE);
    EXPECT_TRUE(!has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_META));

    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_a, TRUE);
    EXPECT_TRUE(modifiers == 0);
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_a, FALSE);
    EXPECT_TRUE(modifiers == 0);

    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Caps_Lock, TRUE);
    EXPECT_TRUE(has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_CAPS_LOCK));
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Caps_Lock, TRUE);
    EXPECT_TRUE(has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_CAPS_LOCK));
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Caps_Lock, FALSE);
    EXPECT_TRUE(has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_CAPS_LOCK));
    modifiers = jw_wpe_modifier_state_update(&state, WPE_KEY_Caps_Lock, TRUE);
    EXPECT_TRUE(!has_modifier(modifiers, WPE_MODIFIER_KEYBOARD_CAPS_LOCK));

    jw_wpe_modifier_state_reset(&state);
    EXPECT_TRUE(jw_wpe_modifier_state_get(&state) == 0);
    return 0;
}
