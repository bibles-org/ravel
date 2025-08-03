#pragma once

#include <imgui.h>

namespace theme {
    struct colors {
        static constexpr ImVec4 BG_DARK = ImVec4(0.086f, 0.094f, 0.149f, 1.0f);
        static constexpr ImVec4 BG = ImVec4(0.102f, 0.106f, 0.149f, 1.0f);
        static constexpr ImVec4 BG_HIGHLIGHT = ImVec4(0.161f, 0.180f, 0.259f, 1.0f);
        static constexpr ImVec4 BG_VISUAL = ImVec4(0.212f, 0.290f, 0.510f, 1.0f);

        static constexpr ImVec4 FG = ImVec4(0.753f, 0.792f, 0.961f, 1.0f);
        static constexpr ImVec4 FG_DARK = ImVec4(0.337f, 0.373f, 0.537f, 1.0f);
        static constexpr ImVec4 FG_GUTTER = ImVec4(0.227f, 0.247f, 0.365f, 1.0f);

        static constexpr ImVec4 BLUE = ImVec4(0.478f, 0.635f, 0.969f, 1.0f);
        static constexpr ImVec4 CYAN = ImVec4(0.490f, 0.812f, 1.000f, 1.0f);
        static constexpr ImVec4 PURPLE = ImVec4(0.733f, 0.604f, 0.969f, 1.0f);
        static constexpr ImVec4 GREEN = ImVec4(0.620f, 0.808f, 0.416f, 1.0f);
        static constexpr ImVec4 ORANGE = ImVec4(1.000f, 0.620f, 0.392f, 1.0f);
        static constexpr ImVec4 RED = ImVec4(0.969f, 0.467f, 0.557f, 1.0f);
        static constexpr ImVec4 YELLOW = ImVec4(0.878f, 0.686f, 0.408f, 1.0f);
        static constexpr ImVec4 MAGENTA = ImVec4(0.733f, 0.604f, 0.969f, 1.0f);

        static constexpr ImVec4 TERMINAL_BLACK = ImVec4(0.086f, 0.094f, 0.149f, 1.0f);
        static constexpr ImVec4 TERMINAL_RED = ImVec4(0.969f, 0.467f, 0.557f, 1.0f);
        static constexpr ImVec4 TERMINAL_GREEN = ImVec4(0.620f, 0.808f, 0.416f, 1.0f);
        static constexpr ImVec4 TERMINAL_YELLOW = ImVec4(0.878f, 0.686f, 0.408f, 1.0f);
        static constexpr ImVec4 TERMINAL_BLUE = ImVec4(0.478f, 0.635f, 0.969f, 1.0f);
        static constexpr ImVec4 TERMINAL_MAGENTA = ImVec4(0.733f, 0.604f, 0.969f, 1.0f);
        static constexpr ImVec4 TERMINAL_CYAN = ImVec4(0.490f, 0.812f, 1.000f, 1.0f);
        static constexpr ImVec4 TERMINAL_WHITE = ImVec4(0.753f, 0.792f, 0.961f, 1.0f);
    };

    inline ImVec4 with_alpha(const ImVec4& color, float alpha) {
        return ImVec4(color.x, color.y, color.z, alpha);
    }

    inline ImU32 color_u32(const ImVec4& color) {
        return ImGui::ColorConvertFloat4ToU32(color);
    }

    inline ImVec4 get_address_color() {
        return colors::TERMINAL_RED;
    }
    inline ImVec4 get_value_color() {
        return colors::BLUE;
    }
    inline ImVec4 get_field_color() {
        return colors::FG;
    }
    inline ImVec4 get_type_color() {
        return colors::PURPLE;
    }
    inline ImVec4 get_bytes_color() {
        return colors::FG_DARK;
    }
    inline ImVec4 get_pointer_color() {
        return colors::CYAN;
    }

    inline void apply() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        ImGui::GetIO().FontGlobalScale = 1.2f;

        colors[ImGuiCol_Text] = colors::FG;
        colors[ImGuiCol_TextDisabled] = colors::FG_DARK;
        colors[ImGuiCol_WindowBg] = colors::BG_DARK;
        colors[ImGuiCol_ChildBg] = colors::BG;
        colors[ImGuiCol_PopupBg] = colors::BG;
        colors[ImGuiCol_Border] = colors::BG_HIGHLIGHT;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = colors::BG_HIGHLIGHT;
        colors[ImGuiCol_FrameBgHovered] = colors::BG_VISUAL;
        colors[ImGuiCol_FrameBgActive] = colors::BLUE;
        colors[ImGuiCol_TitleBg] = colors::BG_DARK;
        colors[ImGuiCol_TitleBgActive] = colors::BG;
        colors[ImGuiCol_TitleBgCollapsed] = colors::BG_DARK;
        colors[ImGuiCol_MenuBarBg] = colors::BG;
        colors[ImGuiCol_ScrollbarBg] = colors::BG_DARK;
        colors[ImGuiCol_ScrollbarGrab] = colors::BG_HIGHLIGHT;
        colors[ImGuiCol_ScrollbarGrabHovered] = colors::BG_VISUAL;
        colors[ImGuiCol_ScrollbarGrabActive] = colors::BLUE;
        colors[ImGuiCol_CheckMark] = colors::GREEN;
        colors[ImGuiCol_SliderGrab] = colors::BLUE;
        colors[ImGuiCol_SliderGrabActive] = colors::CYAN;
        colors[ImGuiCol_Button] = colors::BG_HIGHLIGHT;
        colors[ImGuiCol_ButtonHovered] = colors::BG_VISUAL;
        colors[ImGuiCol_ButtonActive] = colors::BLUE;
        colors[ImGuiCol_Header] = colors::BG_HIGHLIGHT;
        colors[ImGuiCol_HeaderHovered] = colors::BG_VISUAL;
        colors[ImGuiCol_HeaderActive] = colors::BLUE;
        colors[ImGuiCol_Separator] = colors::FG_GUTTER;
        colors[ImGuiCol_SeparatorHovered] = colors::BLUE;
        colors[ImGuiCol_SeparatorActive] = colors::CYAN;
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ResizeGripHovered] = colors::BLUE;
        colors[ImGuiCol_ResizeGripActive] = colors::CYAN;
        colors[ImGuiCol_Tab] = colors::BG_DARK;
        colors[ImGuiCol_TabHovered] = colors::BG_VISUAL;
        colors[ImGuiCol_TabActive] = colors::BLUE;
        colors[ImGuiCol_TabUnfocused] = colors::BG_DARK;
        colors[ImGuiCol_TabUnfocusedActive] = colors::BG_HIGHLIGHT;
        colors[ImGuiCol_DockingPreview] = with_alpha(colors::BLUE, 0.5f);
        colors[ImGuiCol_DockingEmptyBg] = colors::BG_DARK;
        colors[ImGuiCol_PlotLines] = colors::BLUE;
        colors[ImGuiCol_PlotLinesHovered] = colors::CYAN;
        colors[ImGuiCol_PlotHistogram] = colors::GREEN;
        colors[ImGuiCol_PlotHistogramHovered] = colors::YELLOW;
        colors[ImGuiCol_TableHeaderBg] = colors::BG;
        colors[ImGuiCol_TableBorderStrong] = colors::FG_GUTTER;
        colors[ImGuiCol_TableBorderLight] = colors::FG_GUTTER;
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = with_alpha(colors::BG_HIGHLIGHT, 0.3f);
        colors[ImGuiCol_TextSelectedBg] = colors::BG_VISUAL;
        colors[ImGuiCol_DragDropTarget] = colors::YELLOW;
        colors[ImGuiCol_NavHighlight] = colors::BLUE;
        colors[ImGuiCol_NavWindowingHighlight] = colors::CYAN;
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.2f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.35f);

        style.WindowPadding = ImVec2(6, 6);
        style.FramePadding = ImVec2(6, 4);
        style.ItemSpacing = ImVec2(6, 4);
        style.ItemInnerSpacing = ImVec2(4, 4);
        style.IndentSpacing = 20.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 10.0f;

        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.TabRounding = 0.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;
    }
} // namespace theme
