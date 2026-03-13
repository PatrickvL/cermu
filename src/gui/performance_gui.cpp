#ifdef CERMU_HAS_GUI
/**
 * performance_gui.cpp — Performance metrics overlay (Dolphin-style)
 *
 * Renders a semi-transparent overlay in the top-right corner of the
 * screen: scrolling dual-line frame-time graph, VPS/FPS/Speed stats
 * in a three-column Dolphin-style layout, and a max-headroom bar.
 *
 * Toggled from the View menu → "Performance Overlay".
 */

#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "gui/emulator_host.hpp"

// ============================================================================
// Colour helpers
// ============================================================================

static constexpr ImVec4 cyan_text = {0.0f, 0.9f, 1.0f, 1.0f};

/// Speed → colour (green ≥200%, cyan 98–200%, orange 50–98%, red <50%).
static ImVec4 speed_color(double speed_pct) {
    if (speed_pct >= 200.0) return {0.2f, 1.0f, 0.2f, 1.0f};
    if (speed_pct >= 98.0) return cyan_text;
    if (speed_pct >= 50.0)  return {1.0f, 0.6f, 0.2f, 1.0f};
    return {1.0f, 0.3f, 0.2f, 1.0f};
}

// ============================================================================
// Overlay
// ============================================================================

void EmulatorHost::render_performance_window() {
    if (!show_performance_) return;

    auto& pm = perf_metrics_;

    const float bg_alpha = 0.7f;
    const float padding  = 8.0f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();

    // Fixed overlay width; height auto-sizes
    const float overlay_w = 480.0f;

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings;

    // Position: top-right, below the menu bar
    float menu_bar_h = ImGui::GetFrameHeight() + 2.0f;
    ImGui::SetNextWindowPos(
        ImVec2(vp->Pos.x + vp->Size.x - padding,
               vp->Pos.y + menu_bar_h + padding),
        ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(overlay_w, 0.0f), ImVec2(overlay_w, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));

    if (!ImGui::Begin("##PerfOverlay", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(3);
        return;
    }

    // ---- Right-click context menu on overlay --------------------------
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("##PerfCtx");

    if (ImGui::BeginPopup("##PerfCtx")) {
        if (ImGui::MenuItem("Show Performance Graphs", nullptr, show_performance_))
            show_performance_ = !show_performance_;
        ImGui::Separator();
        if (ImGui::MenuItem("Show Frame Times", nullptr, show_perf_frame_time_))
            show_perf_frame_time_ = !show_perf_frame_time_;
        if (ImGui::MenuItem("Show VBlank Times", nullptr, show_perf_vblank_))
            show_perf_vblank_ = !show_perf_vblank_;
        if (ImGui::MenuItem("Show Headroom Bar", nullptr, show_perf_headroom_))
            show_perf_headroom_ = !show_perf_headroom_;
        if (ImGui::MenuItem("Show VPS", nullptr, show_perf_vps_))
            show_perf_vps_ = !show_perf_vps_;
        if (ImGui::MenuItem("Show FPS", nullptr, show_perf_fps_))
            show_perf_fps_ = !show_perf_fps_;
        if (ImGui::MenuItem("Show % Speed", nullptr, show_perf_speed_))
            show_perf_speed_ = !show_perf_speed_;
        ImGui::BeginDisabled(!show_perf_speed_);
        if (ImGui::MenuItem("Show Speed Colors", nullptr, show_perf_colors_))
            show_perf_colors_ = !show_perf_colors_;
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    // ---- Frame Time Graph (overlaid lines + legend) ----------------------
    if (show_perf_frame_time_ || show_perf_vblank_)
    {
        static float interval_vals[2048];  // V-Blank (ms) — blue
        static float frame_vals[2048];     // Frame (ms) — orange
        size_t interval_n = pm.frame_interval.copy_values(interval_vals, 2048);
        size_t frame_n    = pm.frame_time.copy_values(frame_vals, 2048);

        // Auto-scale Y: avg + 2σ, clamped to nice steps
        float y_max = static_cast<float>(
            std::max({pm.frame_time.average() + 2.0 * pm.frame_time.stddev(),
                      pm.frame_interval.average() + 2.0 * pm.frame_interval.stddev(),
                      1.0}));
        if (y_max < 5.0f)       y_max = 5.0f;
        else if (y_max < 10.0f) y_max = 10.0f;
        else if (y_max < 20.0f) y_max = 20.0f;
        else if (y_max < 35.0f) y_max = 35.0f;
        else if (y_max < 50.0f) y_max = 50.0f;
        else                     y_max = std::ceil(y_max / 10.0f) * 10.0f;

        // Reserve left margin for tick labels + notch
        float label_margin = ImGui::CalcTextSize("50.0").x + ImGui::CalcTextSize("W").x + 4.0f;
        float graph_w = ImGui::GetContentRegionAvail().x - label_margin;
        float graph_h = 150.0f;

        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImVec2 graph_origin = ImVec2(cursor_pos.x + label_margin, cursor_pos.y);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Dark graph background + border
        dl->AddRectFilled(graph_origin,
            ImVec2(graph_origin.x + graph_w, graph_origin.y + graph_h),
            IM_COL32(10, 10, 10, 180), 4.0f);
        dl->AddRect(graph_origin,
            ImVec2(graph_origin.x + graph_w, graph_origin.y + graph_h),
            IM_COL32(140, 140, 140, 200), 4.0f);

        // Y-axis tick notches + labels
        struct YTick { float ms; const char* label; };
        YTick ticks[] = {
            {1000.0f / 120.0f, " 8.3"},
            {1000.0f / 59.94f, "16.7"},
            {1000.0f / 50.0f,  "20.0"},
            {1000.0f / 29.97f, "33.4"},
            {1000.0f / 25.0f,  "40.0"},
            {1000.0f / 20.0f,  "50.0"},
        };
        float notch_w = ImGui::CalcTextSize("W").x;  // ~1 character wide
        float label_h = ImGui::GetTextLineHeight();
        ImU32 tick_col = IM_COL32(140, 140, 140, 200);
        for (auto& tick : ticks) {
            if (tick.ms > 0.0f && tick.ms < y_max) {
                float frac  = tick.ms / y_max;
                float y_pos = graph_origin.y + graph_h * (1.0f - frac);
                // Notch extending left from the graph border
                dl->AddLine(
                    ImVec2(graph_origin.x - notch_w, y_pos),
                    ImVec2(graph_origin.x, y_pos),
                    tick_col, 1.0f);
                // Label vertically centred on the notch
                dl->AddText(
                    ImVec2(graph_origin.x - notch_w - ImGui::CalcTextSize(tick.label).x - 2.0f,
                           y_pos - label_h * 0.5f),
                    IM_COL32(220, 220, 220, 220), tick.label);
            }
        }

        // Plot helper
        auto plot_series = [&](const float* vals, size_t n, ImU32 color) {
            if (n < 2) return;
            float x_step = graph_w / static_cast<float>(n - 1);
            for (size_t i = 1; i < n; i++) {
                float x0 = graph_origin.x + x_step * static_cast<float>(i - 1);
                float x1 = graph_origin.x + x_step * static_cast<float>(i);
                float f0 = std::min(vals[i - 1] / y_max, 1.0f);
                float f1 = std::min(vals[i] / y_max, 1.0f);
                dl->AddLine(
                    ImVec2(x0, graph_origin.y + graph_h * (1.0f - f0)),
                    ImVec2(x1, graph_origin.y + graph_h * (1.0f - f1)),
                    color, 1.5f);
            }
        };

        ImU32 col_vblank = IM_COL32(80, 160, 255, 220);
        ImU32 col_frame  = IM_COL32(240, 160, 50, 220);
        if (show_perf_vblank_)
            plot_series(interval_vals, interval_n, col_vblank);
        if (show_perf_frame_time_)
            plot_series(frame_vals, frame_n, col_frame);

        // Legend (bottom-right inside graph)
        {
            float sq = ImGui::GetTextLineHeight() - 2.0f;
            float lh = ImGui::GetTextLineHeightWithSpacing();
            float lp = 5.0f;

            const char* lb1 = "V-Blank (ms)";
            const char* lb2 = "Frame (ms)";
            ImVec2 s1 = ImGui::CalcTextSize(lb1);
            ImVec2 s2 = ImGui::CalcTextSize(lb2);
            float lw = std::max(s1.x, s2.x) + sq + 6.0f + lp * 2.0f;
            float lht = lh * 2.0f + lp * 2.0f;

            float lx = graph_origin.x + graph_w - lw - 3.0f;
            float ly = graph_origin.y + graph_h - lht - 3.0f;
            dl->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + lw, ly + lht),
                              IM_COL32(0, 0, 0, 150), 3.0f);
            dl->AddRect(ImVec2(lx, ly), ImVec2(lx + lw, ly + lht),
                        IM_COL32(140, 140, 140, 200), 3.0f);

            float ey = ly + lp;
            dl->AddRectFilled(ImVec2(lx + lp, ey + 1), ImVec2(lx + lp + sq, ey + 1 + sq), col_vblank);
            dl->AddText(ImVec2(lx + lp + sq + 4, ey), IM_COL32(220, 220, 220, 255), lb1);
            ey += lh;
            dl->AddRectFilled(ImVec2(lx + lp, ey + 1), ImVec2(lx + lp + sq, ey + 1 + sq), col_frame);
            dl->AddText(ImVec2(lx + lp + sq + 4, ey), IM_COL32(220, 220, 220, 255), lb2);
        }

        ImGui::Dummy(ImVec2(label_margin + graph_w, graph_h));
    }

    ImGui::Spacing();

    // ---- Max FPS Headroom Bar --------------------------------------------
    if (show_perf_headroom_)
    {
        double avg_emu_ms = pm.frame_time_long.average();
        double max_fps = (avg_emu_ms > 0.0) ? 1000.0 / avg_emu_ms : 0.0;
        double hw_fps  = pm.target_fps;

        double scale_max = std::max({hw_fps * 1.2, max_fps * 1.05, 10.0});

        float bar_h = 18.0f;
        float avail_w = ImGui::GetContentRegionAvail().x;
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Background
        dl->AddRectFilled(cursor,
            ImVec2(cursor.x + avail_w, cursor.y + bar_h),
            IM_COL32(20, 20, 20, 180), 3.0f);

        // Green / red bar
        float bar_frac = static_cast<float>(max_fps / scale_max);
        if (bar_frac > 1.0f) bar_frac = 1.0f;
        float bar_w = avail_w * bar_frac;
        ImU32 bar_col = (max_fps >= hw_fps)
            ? IM_COL32(60, 200, 80, 200)
            : IM_COL32(220, 80, 50, 200);
        dl->AddRectFilled(cursor,
            ImVec2(cursor.x + bar_w, cursor.y + bar_h),
            bar_col, 3.0f);

        // FPS label
        char fps_label[32];
        snprintf(fps_label, sizeof(fps_label), "%.0f FPS", max_fps);
        ImVec2 tsz = ImGui::CalcTextSize(fps_label);
        float tx = cursor.x + bar_w - tsz.x - 3.0f;
        if (tx < cursor.x + 3.0f) tx = cursor.x + bar_w + 3.0f;
        dl->AddText(ImVec2(tx, cursor.y + (bar_h - tsz.y) * 0.5f),
                    IM_COL32(255, 255, 255, 230), fps_label);

        // Dotted reference line at hardware FPS
        if (hw_fps > 0.0) {
            float ref_x = cursor.x + avail_w * static_cast<float>(hw_fps / scale_max);
            for (float y = cursor.y; y < cursor.y + bar_h; y += 5.0f) {
                float ye = std::min(y + 2.5f, cursor.y + bar_h);
                dl->AddLine(ImVec2(ref_x, y), ImVec2(ref_x, ye),
                            IM_COL32(255, 255, 255, 200), 1.5f);
            }
            char rl[32];
            snprintf(rl, sizeof(rl), "%.0f Hz", hw_fps);
            ImVec2 rs = ImGui::CalcTextSize(rl);
            dl->AddText(ImVec2(ref_x - rs.x * 0.5f, cursor.y + bar_h + 1.0f),
                        IM_COL32(170, 170, 170, 200), rl);
        }

        ImGui::Dummy(ImVec2(avail_w, bar_h + ImGui::GetTextLineHeight() + 2.0f));
    }

    // Capture window bounds before ending, for stat boxes below
    ImVec2 win_pos  = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();

    ImGui::End();
    ImGui::PopStyleVar(3);

    // ---- VPS / FPS / Speed (right-anchored boxes below the overlay) ------
    // Rendered as a separate transparent window so popups layer correctly.
    {
        ImVec4 sc = speed_color(pm.speed_percent);
        double vps = pm.frame_interval.count() > 0 ? pm.frame_interval.hz() : 0.0;
        double fps = pm.frame_time.count() > 0 ? pm.frame_time.hz() : 0.0;

        float gap      = 6.0f;
        float line_h   = ImGui::GetTextLineHeightWithSpacing();
        float box_pad  = 3.0f;
        float box_h    = line_h * 3.0f + box_pad * 2.0f;
        float top_y    = win_pos.y + win_size.y + 2.0f;
        float right_x  = win_pos.x + win_size.x;

        // --- Box text content (3 lines each) ---
        struct BoxCol {
            const char* lines[3];
            ImVec4      colors[3];
        };
        char l_vps[3][32], l_fps[3][32], l_spd[3][32];
        snprintf(l_vps[0], 32, "VPS:%7.2f",        vps);
        snprintf(l_vps[1], 32, "dt:%6.2fms",       pm.frame_interval.average());
        snprintf(l_vps[2], 32, "\xc2\xa0\xc2\xb1:%6.2fms", pm.frame_interval.stddev());
        snprintf(l_fps[0], 32, "FPS:%7.2f",        fps);
        snprintf(l_fps[1], 32, "dt:%6.2fms",       pm.frame_time.average());
        snprintf(l_fps[2], 32, "\xc2\xa0\xc2\xb1:%6.2fms", pm.frame_time.stddev());
        ImVec4 mc = speed_color(pm.max_speed_percent);
        snprintf(l_spd[0], 32, "Speed:%4.0f%%",    pm.speed_percent);
        snprintf(l_spd[1], 32, "Max:%6.0f%%",      pm.max_speed_percent);
        l_spd[2][0] = '\0';

        // Determine which boxes to show
        struct BoxEntry { BoxCol col; bool visible; float width; };
        BoxEntry entries[3] = {
            {{{l_vps[0], l_vps[1], l_vps[2]}, {cyan_text, cyan_text, cyan_text}}, show_perf_vps_, 0.0f},
            {{{l_fps[0], l_fps[1], l_fps[2]}, {cyan_text, cyan_text, cyan_text}}, show_perf_fps_, 0.0f},
            {{{l_spd[0], l_spd[1], l_spd[2]}, {show_perf_colors_ ? sc : cyan_text,
                                                show_perf_colors_ ? mc : cyan_text,
                                                cyan_text}}, show_perf_speed_, 0.0f},
        };

        // Measure each box width from its content
        for (auto& e : entries) {
            if (!e.visible) continue;
            for (int r = 0; r < 3; r++) {
                if (e.col.lines[r][0] == '\0') continue;
                float w = ImGui::CalcTextSize(e.col.lines[r]).x + box_pad * 2.0f;
                if (w > e.width) e.width = w;
            }
        }

        int visible_count = 0;
        float total_w = 0.0f;
        for (auto& e : entries) {
            if (!e.visible) continue;
            if (visible_count > 0) total_w += gap;
            total_w += e.width;
            visible_count++;
        }

        if (visible_count > 0) {
        // Create an invisible window covering the stat boxes area
        ImGui::SetNextWindowPos(ImVec2(right_x - total_w, top_y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(total_w, box_h));
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGuiWindowFlags stats_flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("##PerfStats", nullptr, stats_flags)) {
            // Right-click context menu (same as overlay)
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                ImGui::OpenPopup("##PerfStatsCtx");

            if (ImGui::BeginPopup("##PerfStatsCtx")) {
                if (ImGui::MenuItem("Show Performance Graphs", nullptr, show_performance_))
                    show_performance_ = !show_performance_;
                ImGui::Separator();
                if (ImGui::MenuItem("Show Frame Times", nullptr, show_perf_frame_time_))
                    show_perf_frame_time_ = !show_perf_frame_time_;
                if (ImGui::MenuItem("Show VBlank Times", nullptr, show_perf_vblank_))
                    show_perf_vblank_ = !show_perf_vblank_;
                if (ImGui::MenuItem("Show Headroom Bar", nullptr, show_perf_headroom_))
                    show_perf_headroom_ = !show_perf_headroom_;
                if (ImGui::MenuItem("Show VPS", nullptr, show_perf_vps_))
                    show_perf_vps_ = !show_perf_vps_;
                if (ImGui::MenuItem("Show FPS", nullptr, show_perf_fps_))
                    show_perf_fps_ = !show_perf_fps_;
                if (ImGui::MenuItem("Show % Speed", nullptr, show_perf_speed_))
                    show_perf_speed_ = !show_perf_speed_;
                ImGui::BeginDisabled(!show_perf_speed_);
                if (ImGui::MenuItem("Show Speed Colors", nullptr, show_perf_colors_))
                    show_perf_colors_ = !show_perf_colors_;
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }

            // Draw boxes using window draw list
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 box_bg = IM_COL32(10, 10, 10, 160);

            float bx = right_x;
            for (int b = 2; b >= 0; b--) {
                if (!entries[b].visible) continue;
                auto& bdata = entries[b].col;
                bx -= entries[b].width;
                ImVec2 p0(bx, top_y);
                ImVec2 p1(bx + entries[b].width, top_y + box_h);

                dl->AddRectFilled(p0, p1, box_bg, 3.0f);

                for (int r = 0; r < 3; r++) {
                    if (bdata.lines[r][0] == '\0') continue;
                    ImVec2 ts = ImGui::CalcTextSize(bdata.lines[r]);
                    float tx = bx + entries[b].width - ts.x - box_pad;
                    float ty = top_y + box_pad + line_h * static_cast<float>(r);
                    dl->AddText(ImVec2(tx, ty),
                        ImGui::ColorConvertFloat4ToU32(bdata.colors[r]),
                        bdata.lines[r]);
                }
                bx -= gap;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        } // visible_count > 0
    }
}

#endif // CERMU_HAS_GUI
