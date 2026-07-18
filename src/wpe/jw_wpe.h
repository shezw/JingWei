/*
    JingWei
    src/wpe jw_wpe.h    2026-07-18

     ______     __  __     ______     ______     __     __
    /\  ___\   /\ \_\ \   /\  ___\   /\___  \   /\ \  _ \ \
    \ \___  \  \ \  __ \  \ \  __\   \/_/  /__  \ \ \/ ".\ \
     \/\_____\  \ \_\ \_\  \ \_____\   /\_____\  \ \__/".~\_\
      \/_____/   \/_/\/_/   \/_____/   \/_____/   \/_/   \/_/.com

    @link    : https://github.com/shezw/JingWei
    @author  : shezw
    @email   : hello@shezw.com
*/

#ifndef JINGWEI_WPE_H
#define JINGWEI_WPE_H

#include <jingwei.h>
#include <wpe/wpe-platform.h>

G_BEGIN_DECLS

#define JW_TYPE_WPE_DISPLAY (jw_wpe_display_get_type())

GType jw_wpe_display_get_type(void);

/*
 * The platform display is borrowed and must outlive the returned WPE display.
 * Accepted WPE frames are submitted through its configured JingWei proxy.
 */
WPEDisplay *jw_wpe_display_new(jw_display_t *platform_display);

/* The display emits "first-frame" after its first successful JingWei submit. */
guint64 jw_wpe_display_get_frame_count(WPEDisplay *display);

G_END_DECLS

#endif
