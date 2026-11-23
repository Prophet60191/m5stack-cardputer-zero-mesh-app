// SPDX-License-Identifier: MIT
#pragma once
#include "theme.h"
#include <vector>

namespace view::widgets {
// AUTO is LVGL's overflow-only scrollbar; style it after removing default styles.
inline void style_list_scrollbar(lv_obj_t* list, const ThemePalette& colors) {
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_style_width(list, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list, colors.primary, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(list, 6, 0); // keep text clear of the position marker
}
// Shared list appearance only. Each screen retains its own identity/navigation state.
inline void style_list_row(lv_obj_t *row, const ThemePalette &colors, bool selected) {
    if (!selected) {
        // Fresh rows have no selection style; visited rows release it again.
        // Avoid allocating six unused local properties on every unselected row.
        if (lv_obj_get_style_border_width(row, LV_PART_MAIN) == 0) return;
        for (lv_style_prop_t prop : {LV_STYLE_RADIUS, LV_STYLE_BG_OPA, LV_STYLE_BG_COLOR,
                                     LV_STYLE_BORDER_WIDTH, LV_STYLE_BORDER_SIDE, LV_STYLE_BORDER_COLOR})
            lv_obj_remove_local_style_prop(row, prop, 0);
        return;
    }
    lv_obj_set_style_radius(row, selected ? 6 : 0, 0);
    lv_obj_set_style_bg_opa(row, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(row, colors.button, 0);
    lv_obj_set_style_border_width(row, selected ? 3 : 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(row, colors.primary, 0);
}
inline void select_list_row(const std::vector<lv_obj_t *> &rows, int previous, int next,
                            const ThemePalette &colors) {
    if (previous >= 0 && previous < static_cast<int>(rows.size()))
        style_list_row(rows[previous], colors, false);
    if (next >= 0 && next < static_cast<int>(rows.size())) {
        style_list_row(rows[next], colors, true);
        lv_obj_scroll_to_view(rows[next], LV_ANIM_OFF);
    }
}
} // namespace view::widgets
