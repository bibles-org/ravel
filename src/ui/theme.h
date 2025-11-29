#pragma once

#include <cstdlib>
#include <filesystem>
#include <imgui.h>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace theme {

    struct colors {
        static constexpr ImVec4 rosewater = ImVec4(0.96f, 0.87f, 0.86f, 1.0f);
        static constexpr ImVec4 flamingo = ImVec4(0.96f, 0.80f, 0.81f, 1.0f);
        static constexpr ImVec4 pink = ImVec4(0.96f, 0.73f, 0.80f, 1.0f);
        static constexpr ImVec4 mauve = ImVec4(0.91f, 0.71f, 0.87f, 1.0f);
        static constexpr ImVec4 red = ImVec4(0.94f, 0.67f, 0.67f, 1.0f);
        static constexpr ImVec4 maroon = ImVec4(0.93f, 0.68f, 0.73f, 1.0f);
        static constexpr ImVec4 peach = ImVec4(0.96f, 0.75f, 0.67f, 1.0f);
        static constexpr ImVec4 yellow = ImVec4(0.96f, 0.88f, 0.67f, 1.0f);
        static constexpr ImVec4 green = ImVec4(0.67f, 0.89f, 0.67f, 1.0f);
        static constexpr ImVec4 teal = ImVec4(0.65f, 0.89f, 0.76f, 1.0f);
        static constexpr ImVec4 sky = ImVec4(0.64f, 0.87f, 0.93f, 1.0f);
        static constexpr ImVec4 sapphire = ImVec4(0.67f, 0.81f, 0.93f, 1.0f);
        static constexpr ImVec4 blue = ImVec4(0.71f, 0.77f, 0.96f, 1.0f);
        static constexpr ImVec4 lavender = ImVec4(0.72f, 0.73f, 0.96f, 1.0f);
        static constexpr ImVec4 text = ImVec4(0.73f, 0.73f, 0.73f, 1.0f);

        static constexpr ImVec4 base = ImVec4(0.11f, 0.11f, 0.16f, 1.0f);
        static constexpr ImVec4 mantle = ImVec4(0.09f, 0.09f, 0.14f, 1.0f);
        static constexpr ImVec4 crust = ImVec4(0.07f, 0.07f, 0.11f, 1.0f);

        static constexpr ImVec4 surface0 = ImVec4(0.16f, 0.16f, 0.21f, 1.0f);
        static constexpr ImVec4 surface1 = ImVec4(0.20f, 0.20f, 0.25f, 1.0f);
        static constexpr ImVec4 surface2 = ImVec4(0.24f, 0.24f, 0.29f, 1.0f);
        static constexpr ImVec4 overlay0 = ImVec4(0.28f, 0.28f, 0.33f, 1.0f);
        static constexpr ImVec4 overlay1 = ImVec4(0.32f, 0.32f, 0.37f, 1.0f);
        static constexpr ImVec4 overlay2 = ImVec4(0.36f, 0.36f, 0.41f, 1.0f);

        static constexpr ImVec4 transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        static constexpr ImVec4 primary = blue;
        static constexpr ImVec4 secondary = mauve;
    };

    inline ImVec4 with_alpha(const ImVec4& color, float alpha) {
        return ImVec4(color.x, color.y, color.z, alpha);
    }

    inline ImVec4 get_address_color() {
        return colors::red;
    }
    inline ImVec4 get_value_color() {
        return colors::blue;
    }
    inline ImVec4 get_field_color() {
        return colors::text;
    }
    inline ImVec4 get_type_color() {
        return colors::mauve;
    }
    inline ImVec4 get_bytes_color() {
        return colors::overlay0;
    }
    inline ImVec4 get_pointer_color() {
        return colors::sky;
    }

    namespace internal {
        namespace fs = std::filesystem;

        inline std::vector<fs::path> get_system_font_dirs() {
            std::vector<fs::path> paths;

#if defined(_WIN32)
            if (const char* windir = std::getenv("WINDIR")) {
                paths.emplace_back(fs::path(windir) / "Fonts");
            }
            if (const char* local_app_data = std::getenv("LOCALAPPDATA")) {
                paths.emplace_back(fs::path(local_app_data) / "Microsoft\\Windows\\Fonts");
            }
#elif defined(__linux__)
            paths.emplace_back("/usr/share/fonts");
            paths.emplace_back("/usr/local/share/fonts");
            if (const char* home = std::getenv("HOME")) {
                paths.emplace_back(fs::path(home) / ".local/share/fonts");
                paths.emplace_back(fs::path(home) / ".fonts");
            }
#endif
            return paths;
        }

        inline fs::path find_font_file(std::string_view target_filename) {
            auto dirs = get_system_font_dirs();

            for (const auto& dir : dirs) {
                if (!fs::exists(dir))
                    continue;

                try {
                    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                        if (entry.is_regular_file()) {
                            if (entry.path().filename() == target_filename) {
                                return entry.path();
                            }
                        }
                    }
                } catch (...) {
                    continue;
                }
            }
            return {};
        }

        inline void load_fonts(float size_pixels) {
            ImGuiIO& io = ImGui::GetIO();
            io.Fonts->Clear();

            const char* target_font = "JetBrainsMonoNerdFont-Regular.ttf";

            fs::path font_path = find_font_file(target_font);

            if (!font_path.empty()) {
                ImFontConfig config;
                config.PixelSnapH = true;
                config.OversampleH = 2;
                config.OversampleV = 2;
                io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), size_pixels, &config);
            } else {
                io.Fonts->AddFontDefault();
            }

            io.Fonts->Build();
        }
    } // namespace internal

    inline void apply() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* style_colors = style.Colors;

        static std::once_flag font_flag;
        std::call_once(font_flag, []() {
            internal::load_fonts(18.0f);
        });

        ImGui::GetIO().FontGlobalScale = 1.0f;

        style_colors[ImGuiCol_Text] = colors::text;
        style_colors[ImGuiCol_TextDisabled] = colors::overlay0;
        style_colors[ImGuiCol_WindowBg] = colors::base;
        style_colors[ImGuiCol_ChildBg] = colors::mantle;
        style_colors[ImGuiCol_PopupBg] = colors::surface0;
        style_colors[ImGuiCol_Border] = colors::surface2;
        style_colors[ImGuiCol_BorderShadow] = colors::transparent;
        style_colors[ImGuiCol_FrameBg] = colors::surface0;
        style_colors[ImGuiCol_FrameBgHovered] = colors::surface1;
        style_colors[ImGuiCol_FrameBgActive] = colors::surface2;
        style_colors[ImGuiCol_TitleBg] = colors::mantle;
        style_colors[ImGuiCol_TitleBgActive] = colors::crust;
        style_colors[ImGuiCol_TitleBgCollapsed] = colors::transparent;
        style_colors[ImGuiCol_MenuBarBg] = colors::crust;
        style_colors[ImGuiCol_ScrollbarBg] = colors::surface0;
        style_colors[ImGuiCol_ScrollbarGrab] = colors::surface2;
        style_colors[ImGuiCol_ScrollbarGrabHovered] = colors::overlay0;
        style_colors[ImGuiCol_ScrollbarGrabActive] = colors::overlay1;
        style_colors[ImGuiCol_CheckMark] = colors::primary;
        style_colors[ImGuiCol_SliderGrab] = colors::primary;
        style_colors[ImGuiCol_SliderGrabActive] = colors::secondary;
        style_colors[ImGuiCol_Button] = colors::surface0;
        style_colors[ImGuiCol_ButtonHovered] = colors::surface1;
        style_colors[ImGuiCol_ButtonActive] = colors::surface2;
        style_colors[ImGuiCol_Header] = colors::surface0;
        style_colors[ImGuiCol_HeaderHovered] = colors::surface1;
        style_colors[ImGuiCol_HeaderActive] = colors::surface2;
        style_colors[ImGuiCol_Separator] = colors::surface0;
        style_colors[ImGuiCol_SeparatorHovered] = colors::surface1;
        style_colors[ImGuiCol_SeparatorActive] = colors::surface2;
        style_colors[ImGuiCol_ResizeGrip] = colors::surface0;
        style_colors[ImGuiCol_ResizeGripHovered] = colors::surface1;
        style_colors[ImGuiCol_ResizeGripActive] = colors::surface2;
        style_colors[ImGuiCol_Tab] = colors::surface0;
        style_colors[ImGuiCol_TabHovered] = colors::surface1;
        style_colors[ImGuiCol_TabActive] = colors::surface2;
        style_colors[ImGuiCol_TabUnfocused] = colors::mantle;
        style_colors[ImGuiCol_TabUnfocusedActive] = colors::surface0;

        style_colors[ImGuiCol_DockingPreview] = with_alpha(colors::primary, 0.7f);
        style_colors[ImGuiCol_DockingEmptyBg] = colors::base;

        style_colors[ImGuiCol_PlotLines] = colors::primary;
        style_colors[ImGuiCol_PlotLinesHovered] = colors::secondary;
        style_colors[ImGuiCol_PlotHistogram] = colors::primary;
        style_colors[ImGuiCol_PlotHistogramHovered] = colors::secondary;

        style_colors[ImGuiCol_TableHeaderBg] = colors::surface0;
        style_colors[ImGuiCol_TableBorderStrong] = colors::surface2;
        style_colors[ImGuiCol_TableBorderLight] = colors::surface0;
        style_colors[ImGuiCol_TableRowBg] = colors::transparent;
        style_colors[ImGuiCol_TableRowBgAlt] = with_alpha(colors::surface0, 0.5f);

        style_colors[ImGuiCol_TextSelectedBg] = with_alpha(colors::primary, 0.35f);
        style_colors[ImGuiCol_DragDropTarget] = colors::primary;
        style_colors[ImGuiCol_NavHighlight] = colors::primary;
        style_colors[ImGuiCol_NavWindowingHighlight] = colors::primary;
        style_colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.58f);
        style_colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.58f);
    }
} // namespace theme
