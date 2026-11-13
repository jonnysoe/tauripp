#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
  #define NOMINMAX
#endif

#include "taopp/native.hpp"

#include <dwmapi.h>
#include <windows.h>

#ifndef TAOPP_HAS_XAML_ISLANDS
  #define TAOPP_HAS_XAML_ISLANDS 0
#endif

#if TAOPP_HAS_XAML_ISLANDS
  #ifdef GetCurrentTime
    #undef GetCurrentTime
  #endif
  #include <unknwn.h>
  #include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
  #include <winrt/Windows.Foundation.Collections.h>
  #include <winrt/Windows.Foundation.h>
  #include <winrt/Windows.UI.Text.h>
  #include <winrt/Windows.UI.ViewManagement.h>
  #include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
  #include <winrt/Windows.UI.Xaml.Controls.h>
  #include <winrt/Windows.UI.Xaml.Hosting.h>
  #include <winrt/Windows.UI.Xaml.Media.h>
  #include <winrt/base.h>
#endif

#include <algorithm>
#include <cstdio>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace taopp {

namespace {

    constexpr UINT kWakeMessage = WM_APP + 1;
    constexpr wchar_t kWindowClassName[] = L"taopp.window.v1";

    std::wstring utf8_to_wide(std::string_view value) {
        if (value.empty()) {
            return {};
        }

        const int required = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0
        );
        if (required <= 0) {
            return {};
        }

        std::wstring result(static_cast<std::size_t>(required), L'\0');
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), required
        );
        return result;
    }

    std::string wide_to_utf8(const wchar_t* value, int length) {
        if (value == nullptr || length <= 0) {
            return {};
        }

        const int required = WideCharToMultiByte(CP_UTF8, 0, value, length, nullptr, 0, nullptr, nullptr);
        if (required <= 0) {
            return {};
        }

        std::string result(static_cast<std::size_t>(required), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, length, result.data(), required, nullptr, nullptr);
        return result;
    }

    std::string last_error_message(const char* operation) {
        const DWORD error = GetLastError();
        char buffer[64] {};
        snprintf(
            buffer, sizeof(buffer), "%s failed with Win32 error %lu", operation, static_cast<unsigned long>(error)
        );
        return buffer;
    }

    int scale_value(int value, float scale_factor) { return static_cast<int>(value * scale_factor + 0.5F); }

    float window_scale(HWND window) {
        const UINT dpi = GetDpiForWindow(window);
        return dpi == 0 ? 1.0F : static_cast<float>(dpi) / 96.0F;
    }

#if TAOPP_HAS_XAML_ISLANDS
    using XamlCanvas = winrt::Windows::UI::Xaml::Controls::Canvas;
    using XamlFrameworkElement = winrt::Windows::UI::Xaml::FrameworkElement;
    using XamlBorder = winrt::Windows::UI::Xaml::Controls::Border;
    using XamlButton = winrt::Windows::UI::Xaml::Controls::Button;
    using XamlTextBlock = winrt::Windows::UI::Xaml::Controls::TextBlock;
    using XamlTextBox = winrt::Windows::UI::Xaml::Controls::TextBox;
    using XamlToggleSwitch = winrt::Windows::UI::Xaml::Controls::ToggleSwitch;
    using XamlBrush = winrt::Windows::UI::Xaml::Media::SolidColorBrush;

    bool is_windows_11_or_later() {
        // Cannot use GetVersion API
        // https://learn.microsoft.com/en-us/windows/win32/sysinfo/targeting-your-application-at-windows-8-1
        // Do note that application compatibility manifest is required, see cmake/tauripp.manifest
        static const bool result = [] {
            using RtlGetVersionFunction = LONG(WINAPI*)(OSVERSIONINFOW*);
            const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            const auto rtl_get_version =
                (ntdll == nullptr) ? nullptr
                                   : reinterpret_cast<RtlGetVersionFunction>(GetProcAddress(ntdll, "RtlGetVersion"));
            if (rtl_get_version == nullptr) {
                return false;
            }

            OSVERSIONINFOW version {};
            version.dwOSVersionInfoSize = sizeof(version);
            if (rtl_get_version(&version) != 0) {
                return false;
            }
            // Windows 11 has the same major version as Windows 10 (10) but build number is always 22000 or higher.
            // https://learn.microsoft.com/en-us/answers/questions/586619/windows-11-build-ver-is-still-10-0-22000-194
            return version.dwMajorVersion > 10 || (version.dwMajorVersion == 10 && version.dwBuildNumber >= 22000);
        }();
        return result;
    }

    winrt::Windows::UI::Color xaml_color(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
        return { 255, red, green, blue };
    }

    XamlBrush xaml_brush(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
        return XamlBrush(xaml_color(red, green, blue));
    }

    winrt::Windows::UI::Xaml::Thickness xaml_thickness(double value) { return { value, value, value, value }; }

    winrt::Windows::UI::Xaml::CornerRadius xaml_corner_radius(double value) { return { value, value, value, value }; }

    std::string xaml_text(const winrt::hstring& value) { return winrt::to_string(value); }

    struct XamlPalette {
        bool dark = false;
        double card_corner_radius = 0;
        double text_input_corner_radius = 0;
        double button_corner_radius = 0;
        std::uint8_t window_red = 246;
        std::uint8_t window_green = 248;
        std::uint8_t window_blue = 251;
        std::uint8_t card_red = 255;
        std::uint8_t card_green = 255;
        std::uint8_t card_blue = 255;
        std::uint8_t text_red = 31;
        std::uint8_t text_green = 35;
        std::uint8_t text_blue = 40;
        std::uint8_t muted_red = 92;
        std::uint8_t muted_green = 98;
        std::uint8_t muted_blue = 108;
        std::uint8_t surface_red = 247;
        std::uint8_t surface_green = 248;
        std::uint8_t surface_blue = 250;
        std::uint8_t border_red = 216;
        std::uint8_t border_green = 220;
        std::uint8_t border_blue = 226;
        std::uint8_t accent_red = 0;
        std::uint8_t accent_green = 103;
        std::uint8_t accent_blue = 192;
        std::uint8_t accent_hover_red = 0;
        std::uint8_t accent_hover_green = 120;
        std::uint8_t accent_hover_blue = 215;
        std::uint8_t accent_pressed_red = 0;
        std::uint8_t accent_pressed_green = 86;
        std::uint8_t accent_pressed_blue = 160;
        std::uint8_t accent_foreground_red = 255;
        std::uint8_t accent_foreground_green = 255;
        std::uint8_t accent_foreground_blue = 255;
        std::uint8_t toggle_off_fill_red = 247;
        std::uint8_t toggle_off_fill_green = 248;
        std::uint8_t toggle_off_fill_blue = 250;
        std::uint8_t toggle_disabled_red = 128;
        std::uint8_t toggle_disabled_green = 128;
        std::uint8_t toggle_disabled_blue = 128;
        std::uint8_t toggle_on_thumb_red = 255;
        std::uint8_t toggle_on_thumb_green = 255;
        std::uint8_t toggle_on_thumb_blue = 255;
    };

    bool system_prefers_dark_theme() {
        try {
            const auto background = winrt::Windows::UI::ViewManagement::UISettings().GetColorValue(
                winrt::Windows::UI::ViewManagement::UIColorType::Background
            );
            return background.R < 128;
        } catch (...) {
            return false;
        }
    }

    void set_title_bar_theme(HWND window, bool dark) {
        const BOOL use_dark_mode = dark ? TRUE : FALSE;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &use_dark_mode, sizeof(use_dark_mode));
    }

    XamlPalette xaml_palette(bool dark) {
        XamlPalette palette;
        palette.dark = dark;
        if (is_windows_11_or_later()) {
            palette.card_corner_radius = 14;
            palette.text_input_corner_radius = 8;
            palette.button_corner_radius = 4;
        }
        if (dark) {
            palette.window_red = 32;
            palette.window_green = 32;
            palette.window_blue = 32;
            palette.card_red = 44;
            palette.card_green = 44;
            palette.card_blue = 44;
            palette.text_red = 244;
            palette.text_green = 244;
            palette.text_blue = 244;
            palette.muted_red = 190;
            palette.muted_green = 190;
            palette.muted_blue = 190;
            palette.surface_red = 57;
            palette.surface_green = 57;
            palette.surface_blue = 57;
            palette.border_red = 82;
            palette.border_green = 82;
            palette.border_blue = 82;
            palette.toggle_off_fill_red = 57;
            palette.toggle_off_fill_green = 57;
            palette.toggle_off_fill_blue = 57;
            palette.accent_foreground_red = 0;
            palette.accent_foreground_green = 0;
            palette.accent_foreground_blue = 0;
        }

        try {
            const auto settings = winrt::Windows::UI::ViewManagement::UISettings();
            const auto accent = winrt::Windows::UI::ViewManagement::UISettings().GetColorValue(
                winrt::Windows::UI::ViewManagement::UIColorType::Accent
            );
            palette.accent_red = accent.R;
            palette.accent_green = accent.G;
            palette.accent_blue = accent.B;
            const auto button_face =
                settings.UIElementColor(winrt::Windows::UI::ViewManagement::UIElementType::ButtonFace);
            palette.toggle_off_fill_red = button_face.R;
            palette.toggle_off_fill_green = button_face.G;
            palette.toggle_off_fill_blue = button_face.B;
            const auto disabled = settings.UIElementColor(winrt::Windows::UI::ViewManagement::UIElementType::GrayText);
            palette.toggle_disabled_red = disabled.R;
            palette.toggle_disabled_green = disabled.G;
            palette.toggle_disabled_blue = disabled.B;
        } catch (...) { }

        if (dark) {
            palette.toggle_on_thumb_red = 0;
            palette.toggle_on_thumb_green = 0;
            palette.toggle_on_thumb_blue = 0;
        }

        const auto adjust_accent = [](std::uint8_t channel, int amount) {
            return static_cast<std::uint8_t>(std::clamp(static_cast<int>(channel) + amount, 0, 255));
        };
        const int hover_amount = palette.dark ? 24 : 16;
        const int pressed_amount = palette.dark ? -24 : -16;
        palette.accent_hover_red = adjust_accent(palette.accent_red, hover_amount);
        palette.accent_hover_green = adjust_accent(palette.accent_green, hover_amount);
        palette.accent_hover_blue = adjust_accent(palette.accent_blue, hover_amount);
        palette.accent_pressed_red = adjust_accent(palette.accent_red, pressed_amount);
        palette.accent_pressed_green = adjust_accent(palette.accent_green, pressed_amount);
        palette.accent_pressed_blue = adjust_accent(palette.accent_blue, pressed_amount);
        return palette;
    }

    template<typename XamlControl>
    void set_xaml_resource(XamlControl& control, const wchar_t* key, const XamlBrush& value) {
        control.Resources().Insert(winrt::box_value(winrt::hstring(key)), value);
    }

    void set_button_state_resources(XamlButton& control, const XamlPalette& palette, bool accent) {
        const auto normal_background =
            accent ? xaml_brush(palette.accent_red, palette.accent_green, palette.accent_blue)
                   : xaml_brush(palette.surface_red, palette.surface_green, palette.surface_blue);
        const auto hover_background =
            accent ? xaml_brush(palette.accent_hover_red, palette.accent_hover_green, palette.accent_hover_blue)
                   : xaml_brush(
                         palette.card_red - (palette.dark ? 0 : 8),
                         palette.card_green - (palette.dark ? 0 : 8),
                         palette.card_blue - (palette.dark ? 0 : 8)
                     );
        const auto pressed_background =
            accent ? xaml_brush(palette.accent_pressed_red, palette.accent_pressed_green, palette.accent_pressed_blue)
                   : xaml_brush(
                         palette.surface_red - (palette.dark ? 0 : 12),
                         palette.surface_green - (palette.dark ? 0 : 12),
                         palette.surface_blue - (palette.dark ? 0 : 12)
                     );
        const auto foreground =
            accent ? xaml_brush(
                         palette.accent_foreground_red, palette.accent_foreground_green, palette.accent_foreground_blue
                     )
                   : xaml_brush(palette.text_red, palette.text_green, palette.text_blue);
        const auto border =
            accent ? normal_background : xaml_brush(palette.border_red, palette.border_green, palette.border_blue);

        set_xaml_resource(control, L"ButtonBackground", normal_background);
        set_xaml_resource(control, L"ButtonBackgroundPointerOver", hover_background);
        set_xaml_resource(control, L"ButtonBackgroundPressed", pressed_background);
        set_xaml_resource(control, L"ButtonBackgroundDisabled", xaml_brush(90, 90, 90));
        set_xaml_resource(control, L"ButtonForeground", foreground);
        set_xaml_resource(control, L"ButtonForegroundPointerOver", foreground);
        set_xaml_resource(control, L"ButtonForegroundPressed", foreground);
        set_xaml_resource(control, L"ButtonForegroundDisabled", xaml_brush(145, 145, 145));
        set_xaml_resource(control, L"ButtonBorderBrush", border);
        set_xaml_resource(control, L"ButtonBorderBrushPointerOver", border);
        set_xaml_resource(control, L"ButtonBorderBrushPressed", border);
        set_xaml_resource(control, L"ButtonBorderBrushDisabled", xaml_brush(90, 90, 90));
    }

    void set_toggle_state_resources(XamlToggleSwitch& control, const XamlPalette& palette) {
        const auto adjust_channel = [](std::uint8_t channel, int amount) {
            return static_cast<std::uint8_t>(std::clamp(static_cast<int>(channel) + amount, 0, 255));
        };
        const auto track_on = xaml_brush(palette.accent_red, palette.accent_green, palette.accent_blue);
        const auto track_on_hover =
            xaml_brush(palette.accent_hover_red, palette.accent_hover_green, palette.accent_hover_blue);
        const auto track_on_pressed =
            xaml_brush(palette.accent_pressed_red, palette.accent_pressed_green, palette.accent_pressed_blue);
        const auto track_off =
            xaml_brush(palette.toggle_off_fill_red, palette.toggle_off_fill_green, palette.toggle_off_fill_blue);
        const auto track_off_hover = xaml_brush(
            adjust_channel(palette.toggle_off_fill_red, palette.dark ? 0 : -8),
            adjust_channel(palette.toggle_off_fill_green, palette.dark ? 0 : -8),
            adjust_channel(palette.toggle_off_fill_blue, palette.dark ? 0 : -8)
        );
        const auto track_off_pressed = xaml_brush(
            adjust_channel(palette.toggle_off_fill_red, palette.dark ? 0 : -12),
            adjust_channel(palette.toggle_off_fill_green, palette.dark ? 0 : -12),
            adjust_channel(palette.toggle_off_fill_blue, palette.dark ? 0 : -12)
        );
        const auto on_thumb =
            xaml_brush(palette.toggle_on_thumb_red, palette.toggle_on_thumb_green, palette.toggle_on_thumb_blue);
        const auto off_thumb =
            xaml_brush(palette.toggle_disabled_red, palette.toggle_disabled_green, palette.toggle_disabled_blue);
        const auto disabled = off_thumb;
        const auto on_stroke = track_on;
        const auto off_stroke = disabled;

        set_xaml_resource(control, L"ToggleSwitchFillOn", track_on);
        set_xaml_resource(control, L"ToggleSwitchFillOnPointerOver", track_on_hover);
        set_xaml_resource(control, L"ToggleSwitchFillOnPressed", track_on_pressed);
        set_xaml_resource(control, L"ToggleSwitchFillOnDisabled", disabled);
        set_xaml_resource(control, L"ToggleSwitchFillOff", track_off);
        set_xaml_resource(control, L"ToggleSwitchFillOffPointerOver", track_off_hover);
        set_xaml_resource(control, L"ToggleSwitchFillOffPressed", track_off_pressed);
        set_xaml_resource(control, L"ToggleSwitchFillOffDisabled", disabled);

        set_xaml_resource(control, L"ToggleSwitchKnobFillOn", on_thumb);
        set_xaml_resource(control, L"ToggleSwitchKnobFillOnPointerOver", on_thumb);
        set_xaml_resource(control, L"ToggleSwitchKnobFillOnPressed", on_thumb);
        set_xaml_resource(control, L"ToggleSwitchKnobFillOnDisabled", disabled);
        set_xaml_resource(control, L"ToggleSwitchKnobFillOff", off_thumb);
        set_xaml_resource(control, L"ToggleSwitchKnobFillOffPointerOver", off_thumb);
        set_xaml_resource(control, L"ToggleSwitchKnobFillOffPressed", off_thumb);
        set_xaml_resource(control, L"ToggleSwitchKnobFillOffDisabled", disabled);

        set_xaml_resource(control, L"ToggleSwitchStrokeOn", on_stroke);
        set_xaml_resource(control, L"ToggleSwitchStrokeOnPointerOver", on_stroke);
        set_xaml_resource(control, L"ToggleSwitchStrokeOnPressed", on_stroke);
        set_xaml_resource(control, L"ToggleSwitchStrokeOnDisabled", disabled);
        set_xaml_resource(control, L"ToggleSwitchStrokeOff", off_stroke);
        set_xaml_resource(control, L"ToggleSwitchStrokeOffPointerOver", off_stroke);
        set_xaml_resource(control, L"ToggleSwitchStrokeOffPressed", off_stroke);
        set_xaml_resource(control, L"ToggleSwitchStrokeOffDisabled", disabled);

        set_xaml_resource(control, L"ToggleSwitchThumbBackgroundThemeBrush", off_thumb);
        set_xaml_resource(control, L"ToggleSwitchThumbPressedBackgroundThemeBrush", on_thumb);
        set_xaml_resource(control, L"ToggleSwitchThumbPointerOverBackgroundThemeBrush", off_thumb);
        set_xaml_resource(control, L"ToggleSwitchThumbDisabledBackgroundThemeBrush", disabled);
        set_xaml_resource(
            control,
            L"ToggleSwitchHeaderForeground",
            xaml_brush(palette.text_red, palette.text_green, palette.text_blue)
        );
        set_xaml_resource(
            control,
            L"ToggleSwitchHeaderForegroundThemeBrush",
            xaml_brush(palette.text_red, palette.text_green, palette.text_blue)
        );
        set_xaml_resource(control, L"ToggleSwitchHeaderForegroundDisabled", disabled);
        set_xaml_resource(
            control,
            L"ToggleSwitchContentForeground",
            xaml_brush(palette.text_red, palette.text_green, palette.text_blue)
        );
        set_xaml_resource(control, L"ToggleSwitchContentForegroundDisabled", disabled);
    }
#endif

} // namespace

struct NativeApplication::Impl {
    struct WindowRecord {
        WindowId id = 0;
        HWND handle = nullptr;
        std::unordered_map<ControlId, HWND> controls;
        std::unordered_map<HWND, ControlId> controls_by_handle;
#if TAOPP_HAS_XAML_ISLANDS
        bool xaml_enabled = false;
        bool dark_theme = false;
        winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource xaml_source { nullptr };
        HWND xaml_handle = nullptr;
        XamlCanvas xaml_root { nullptr };
        XamlBorder xaml_card { nullptr };
        std::unordered_map<ControlId, XamlFrameworkElement> xaml_controls;
        std::unordered_map<ControlId, ControlType> xaml_control_types;
        std::unordered_map<ControlId, ControlOptions> xaml_options;
        std::unordered_map<ControlId, XamlTextBlock> xaml_toggle_headers;
        std::unordered_map<ControlId, std::string> xaml_texts;
        std::unordered_map<ControlId, winrt::event_token> xaml_event_tokens;
#endif
    };

    explicit Impl(EventLoop& event_loop): event_loop(event_loop), thread_id(GetCurrentThreadId()) { }

    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);

#if TAOPP_HAS_XAML_ISLANDS
    bool ensure_xaml_manager();
    void refresh_xaml_theme();
    void apply_xaml_theme(WindowRecord& window, bool dark);
    void apply_xaml_control_theme(WindowRecord& window, ControlId control_id, const XamlPalette& palette);
    bool initialize_xaml(WindowRecord& window);
    void resize_xaml(WindowRecord& window);
    NativeResult add_xaml_control(WindowRecord& window, const ControlOptions& options);
    void close_xaml(WindowRecord& window);
#endif

    bool ensure_window_class() {
        if (window_class != 0) {
            return true;
        }

        WNDCLASSEXW window_class_info {};
        window_class_info.cbSize = sizeof(window_class_info);
        window_class_info.hInstance = GetModuleHandleW(nullptr);
        window_class_info.lpfnWndProc = &Impl::window_proc;
        window_class_info.lpszClassName = kWindowClassName;
        window_class_info.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class_info.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        window_class = RegisterClassExW(&window_class_info);
        if (window_class == 0 && GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            window_class = 1;
        }
        return window_class != 0;
    }

    WindowRecord* find_window(WindowId id) const {
        const auto found = windows.find(id);
        return found == windows.end() ? nullptr : found->second.get();
    }

    WindowRecord* find_window(HWND handle) const {
        const auto found = windows_by_handle.find(handle);
        return found == windows_by_handle.end() ? nullptr : found->second;
    }

#if TAOPP_HAS_XAML_ISLANDS
    WindowRecord* find_window_by_id(WindowId id) const { return find_window(id); }
#endif

    EventLoop& event_loop;
    std::unordered_map<WindowId, std::unique_ptr<WindowRecord>> windows;
    std::unordered_map<HWND, WindowRecord*> windows_by_handle;
    ActionCallback action_callback;
    CloseRequestedCallback close_requested_callback;
    ATOM window_class = 0;
    DWORD thread_id = 0;
#if TAOPP_HAS_XAML_ISLANDS
    winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager xaml_manager { nullptr };
    winrt::Windows::UI::ViewManagement::UISettings xaml_settings { nullptr };
    winrt::event_token xaml_theme_token {};
    bool xaml_theme_listener_registered = false;
    bool xaml_apartment_initialized = false;
    std::string xaml_error;
    std::shared_ptr<int> xaml_theme_lifetime = std::make_shared<int>(0);
#endif
};

#if TAOPP_HAS_XAML_ISLANDS

bool NativeApplication::Impl::ensure_xaml_manager() {
    if (xaml_manager != nullptr) {
        return true;
    }

    try {
        if (!xaml_apartment_initialized) {
            winrt::init_apartment(winrt::apartment_type::single_threaded);
            xaml_apartment_initialized = true;
        }
        xaml_manager = winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        xaml_settings = winrt::Windows::UI::ViewManagement::UISettings();
        const EventLoopProxy theme_proxy = event_loop.proxy();
        const std::weak_ptr<int> theme_lifetime = xaml_theme_lifetime;
        xaml_theme_token = xaml_settings.ColorValuesChanged([this, theme_proxy, theme_lifetime](
                                                                winrt::Windows::UI::ViewManagement::UISettings const&,
                                                                winrt::Windows::Foundation::IInspectable const&
                                                            ) {
            theme_proxy.post([this, theme_lifetime] {
                if (!theme_lifetime.expired()) {
                    refresh_xaml_theme();
                }
            });
        });
        xaml_theme_listener_registered = true;
        return xaml_manager != nullptr;
    } catch (const winrt::hresult_error& error) {
        xaml_error = "WindowsXamlManager initialization failed (HRESULT " +
                     std::to_string(static_cast<std::int32_t>(error.code().value)) +
                     "): " +
                     winrt::to_string(error.message());
        return false;
    } catch (...) {
        xaml_error = "WindowsXamlManager initialization failed with an unknown error";
        return false;
    }
}

void NativeApplication::Impl::refresh_xaml_theme() {
    const bool dark = system_prefers_dark_theme();
    for (auto& entry : windows) {
        if (entry.second->xaml_enabled) {
            apply_xaml_theme(*entry.second, dark);
        }
    }
}

void NativeApplication::Impl::apply_xaml_control_theme(
    WindowRecord& window,
    ControlId control_id,
    const XamlPalette& palette
) {
    const auto element_found = window.xaml_controls.find(control_id);
    const auto options_found = window.xaml_options.find(control_id);
    if (element_found == window.xaml_controls.end() || options_found == window.xaml_options.end()) {
        return;
    }

    const auto dark_brush = xaml_brush(palette.text_red, palette.text_green, palette.text_blue);
    const auto muted_brush = xaml_brush(palette.muted_red, palette.muted_green, palette.muted_blue);
    const auto accent_brush = xaml_brush(palette.accent_red, palette.accent_green, palette.accent_blue);
    const auto surface_brush = xaml_brush(palette.surface_red, palette.surface_green, palette.surface_blue);
    const auto border_brush = xaml_brush(palette.border_red, palette.border_green, palette.border_blue);
    const auto& options = options_found->second;

    try {
        switch (options.type) {
        case ControlType::Label: {
            auto control = element_found->second.as<XamlTextBlock>();
            control.Foreground(options.emphasized ? dark_brush : muted_brush);
            break;
        }
        case ControlType::TextInput: {
            auto control = element_found->second.as<XamlTextBox>();
            control.Foreground(dark_brush);
            control.Background(surface_brush);
            control.BorderBrush(border_brush);
            break;
        }
        case ControlType::Button: {
            auto control = element_found->second.as<XamlButton>();
            control.Background(options.accent ? accent_brush : surface_brush);
            control.Foreground(
                options.accent
                    ? xaml_brush(
                          palette.accent_foreground_red, palette.accent_foreground_green, palette.accent_foreground_blue
                      )
                    : dark_brush
            );
            control.BorderBrush(options.accent ? accent_brush : border_brush);
            set_button_state_resources(control, palette, options.accent);
            break;
        }
        case ControlType::Toggle: {
            auto control = element_found->second.as<XamlToggleSwitch>();
            control.Foreground(dark_brush);
            const auto header_found = window.xaml_toggle_headers.find(control_id);
            if (header_found != window.xaml_toggle_headers.end()) {
                header_found->second.Foreground(dark_brush);
            }
            set_toggle_state_resources(control, palette);
            break;
        }
        }
    } catch (...) { }
}

void NativeApplication::Impl::apply_xaml_theme(WindowRecord& window, bool dark) {
    window.dark_theme = dark;
    set_title_bar_theme(window.handle, dark);
    const XamlPalette palette = xaml_palette(dark);

    try {
        window.xaml_root.RequestedTheme(
            dark ? winrt::Windows::UI::Xaml::ElementTheme::Dark : winrt::Windows::UI::Xaml::ElementTheme::Light
        );
        window.xaml_root.Background(xaml_brush(palette.window_red, palette.window_green, palette.window_blue));
        window.xaml_card.Background(xaml_brush(palette.card_red, palette.card_green, palette.card_blue));
        window.xaml_card.BorderBrush(xaml_brush(palette.border_red, palette.border_green, palette.border_blue));
    } catch (...) { }

    for (const auto& entry : window.xaml_options) {
        apply_xaml_control_theme(window, entry.first, palette);
    }
}

bool NativeApplication::Impl::initialize_xaml(WindowRecord& window) {
    if (!ensure_xaml_manager()) {
        return false;
    }

    try {
        window.xaml_source = winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource();
        auto native_source = window.xaml_source.as<IDesktopWindowXamlSourceNative>();
        winrt::check_hresult(native_source->AttachToWindow(window.handle));
        winrt::check_hresult(native_source->get_WindowHandle(&window.xaml_handle));

        window.dark_theme = system_prefers_dark_theme();
        set_title_bar_theme(window.handle, window.dark_theme);
        const XamlPalette palette = xaml_palette(window.dark_theme);
        window.xaml_root = XamlCanvas();
        window.xaml_root.RequestedTheme(
            window.dark_theme ? winrt::Windows::UI::Xaml::ElementTheme::Dark
                              : winrt::Windows::UI::Xaml::ElementTheme::Light
        );
        window.xaml_root.Background(xaml_brush(palette.window_red, palette.window_green, palette.window_blue));
        window.xaml_root.HorizontalAlignment(winrt::Windows::UI::Xaml::HorizontalAlignment::Stretch);
        window.xaml_root.VerticalAlignment(winrt::Windows::UI::Xaml::VerticalAlignment::Stretch);

        XamlBorder card;
        card.Background(xaml_brush(palette.card_red, palette.card_green, palette.card_blue));
        card.BorderBrush(xaml_brush(palette.border_red, palette.border_green, palette.border_blue));
        card.BorderThickness(xaml_thickness(1));
        card.CornerRadius(xaml_corner_radius(palette.card_corner_radius));
        card.Width(720);
        card.Height(470);
        XamlCanvas::SetLeft(card, 40);
        XamlCanvas::SetTop(card, 30);
        XamlCanvas::SetZIndex(card, 0);
        window.xaml_card = card;
        window.xaml_root.Children().Append(card);

        window.xaml_source.Content(window.xaml_root);
        window.xaml_enabled = true;
        resize_xaml(window);
        return true;
    } catch (const winrt::hresult_error& error) {
        xaml_error = "DesktopWindowXamlSource initialization failed (HRESULT " +
                     std::to_string(static_cast<std::int32_t>(error.code().value)) +
                     "): " +
                     winrt::to_string(error.message());
        close_xaml(window);
        return false;
    } catch (...) {
        xaml_error = "DesktopWindowXamlSource initialization failed with an unknown error";
        close_xaml(window);
        return false;
    }
}

void NativeApplication::Impl::resize_xaml(WindowRecord& window) {
    if (!window.xaml_enabled || window.xaml_handle == nullptr) {
        return;
    }

    RECT client {};
    GetClientRect(window.handle, &client);
    SetWindowPos(
        window.xaml_handle,
        nullptr,
        0,
        0,
        std::max(0L, client.right - client.left),
        std::max(0L, client.bottom - client.top),
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW
    );
}

NativeResult NativeApplication::Impl::add_xaml_control(WindowRecord& window, const ControlOptions& options) {
    if (options.id == 0) {
        return NativeResult::invalid("control id must be non-zero");
    }
    if (window.xaml_controls.find(options.id) != window.xaml_controls.end()) {
        return NativeResult::invalid("control id is already registered");
    }

    XamlFrameworkElement element { nullptr };
    const XamlPalette palette = xaml_palette(window.dark_theme);
    const auto dark_brush = xaml_brush(palette.text_red, palette.text_green, palette.text_blue);
    const auto muted_brush = xaml_brush(palette.muted_red, palette.muted_green, palette.muted_blue);
    const auto accent_brush = xaml_brush(palette.accent_red, palette.accent_green, palette.accent_blue);
    const auto light_surface_brush = xaml_brush(palette.surface_red, palette.surface_green, palette.surface_blue);
    const auto border_brush = xaml_brush(palette.border_red, palette.border_green, palette.border_blue);
    constexpr auto left = winrt::Windows::UI::Xaml::HorizontalAlignment::Left;
    constexpr auto top = winrt::Windows::UI::Xaml::VerticalAlignment::Top;

    switch (options.type) {
    case ControlType::Label: {
        XamlTextBlock control;
        control.Text(winrt::to_hstring(options.text));
        control.FontSize(options.font_size);
        control.FontWeight(
            options.emphasized ? winrt::Windows::UI::Text::FontWeights::SemiBold()
                               : winrt::Windows::UI::Text::FontWeights::Normal()
        );
        control.Foreground(options.emphasized ? dark_brush : muted_brush);
        control.TextWrapping(winrt::Windows::UI::Xaml::TextWrapping::Wrap);
        element = control.as<XamlFrameworkElement>();
        break;
    }
    case ControlType::TextInput: {
        XamlTextBox control;
        control.Text(winrt::to_hstring(options.text));
        control.PlaceholderText(L"Type your name");
        control.FontSize(options.font_size);
        control.Foreground(dark_brush);
        control.Background(light_surface_brush);
        control.BorderBrush(border_brush);
        control.BorderThickness(xaml_thickness(1));
        control.CornerRadius(xaml_corner_radius(palette.text_input_corner_radius));
        control.Padding({ 14, 7, 14, 7 });
        control.VerticalContentAlignment(winrt::Windows::UI::Xaml::VerticalAlignment::Center);
        control.IsEnabled(options.enabled);
        element = control.as<XamlFrameworkElement>();
        break;
    }
    case ControlType::Button: {
        XamlButton control;
        control.Content(winrt::box_value(winrt::to_hstring(options.text)));
        control.FontSize(options.font_size);
        control.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        control.Background(options.accent ? accent_brush : light_surface_brush);
        control.Foreground(
            options.accent
                ? xaml_brush(
                      palette.accent_foreground_red, palette.accent_foreground_green, palette.accent_foreground_blue
                  )
                : dark_brush
        );
        control.BorderBrush(options.accent ? accent_brush : border_brush);
        control.BorderThickness(xaml_thickness(options.accent ? 0 : 1));
        control.CornerRadius(xaml_corner_radius(palette.button_corner_radius));
        control.Padding({ 14, 5, 14, 5 });
        control.HorizontalContentAlignment(winrt::Windows::UI::Xaml::HorizontalAlignment::Center);
        control.VerticalContentAlignment(winrt::Windows::UI::Xaml::VerticalAlignment::Center);
        control.IsTabStop(true);
        control.IsEnabled(options.enabled);
        set_button_state_resources(control, palette, options.accent);
        const WindowId window_id = window.id;
        const ControlId control_id = options.id;
        window.xaml_event_tokens.emplace(
            control_id, control.Click([this, window_id, control_id](auto const&, auto const&) {
                auto* current_window = find_window_by_id(window_id);
                if (current_window == nullptr || !action_callback) {
                    return;
                }
                try {
                    action_callback(window_id, control_id, {});
                } catch (...) {
                    // Native callbacks cannot unwind through XAML event dispatch.
                }
            })
        );
        element = control.as<XamlFrameworkElement>();
        break;
    }
    case ControlType::Toggle: {
        XamlToggleSwitch control;
        XamlTextBlock header;
        header.Text(winrt::to_hstring(options.text));
        header.FontSize(options.font_size);
        header.Foreground(dark_brush);
        header.VerticalAlignment(winrt::Windows::UI::Xaml::VerticalAlignment::Center);
        control.Header(header);
        control.OnContent(winrt::box_value(winrt::to_hstring("On")));
        control.OffContent(winrt::box_value(winrt::to_hstring("Off")));
        control.IsOn(options.checked);
        control.FontSize(options.font_size);
        control.Foreground(dark_brush);
        control.HorizontalContentAlignment(left);
        control.VerticalContentAlignment(winrt::Windows::UI::Xaml::VerticalAlignment::Center);
        control.IsEnabled(options.enabled);
        control.MinHeight(64.0);
        set_toggle_state_resources(control, palette);
        const WindowId window_id = window.id;
        const ControlId control_id = options.id;
        window.xaml_event_tokens.emplace(
            control_id, control.Toggled([this, window_id, control_id](auto const& sender, auto const&) {
                auto* current_window = find_window_by_id(window_id);
                if (current_window == nullptr || !action_callback) {
                    return;
                }
                try {
                    const auto toggle = sender.template as<XamlToggleSwitch>();
                    action_callback(window_id, control_id, toggle.IsOn() ? "on" : "off");
                } catch (...) {
                    // Native callbacks cannot unwind through XAML event dispatch.
                }
            })
        );
        window.xaml_toggle_headers.emplace(options.id, header);
        element = control.as<XamlFrameworkElement>();
        break;
    }
    }

    element.Width(std::max(1.0, static_cast<double>(options.logical_bounds.width)));
    const double element_height = options.type == ControlType::Toggle
                                      ? std::max(64.0, static_cast<double>(options.logical_bounds.height))
                                      : std::max(1.0, static_cast<double>(options.logical_bounds.height));
    element.Height(element_height);
    element.HorizontalAlignment(left);
    element.VerticalAlignment(top);
    XamlCanvas::SetLeft(element, options.logical_bounds.x);
    XamlCanvas::SetTop(element, options.logical_bounds.y);
    XamlCanvas::SetZIndex(element, 1);
    window.xaml_root.Children().Append(element);
    window.xaml_controls.emplace(options.id, element);
    window.xaml_control_types.emplace(options.id, options.type);
    window.xaml_options.emplace(options.id, options);
    window.xaml_texts.emplace(options.id, options.text);
    apply_xaml_control_theme(window, options.id, palette);
    return NativeResult::success();
}

void NativeApplication::Impl::close_xaml(WindowRecord& window) {
    if (!window.xaml_enabled && window.xaml_source == nullptr) {
        return;
    }
    window.xaml_event_tokens.clear();
    window.xaml_controls.clear();
    window.xaml_control_types.clear();
    window.xaml_options.clear();
    window.xaml_toggle_headers.clear();
    window.xaml_texts.clear();
    if (window.xaml_source != nullptr) {
        try {
            window.xaml_source.Content(nullptr);
            window.xaml_source.Close();
        } catch (...) { }
    }
    window.xaml_source = nullptr;
    window.xaml_root = nullptr;
    window.xaml_card = nullptr;
    window.xaml_handle = nullptr;
    window.xaml_enabled = false;
}

#endif

NativeApplication::NativeApplication(EventLoop& event_loop):
    event_loop_(event_loop),
    impl_(std::make_unique<Impl>(event_loop)) { }

NativeApplication::~NativeApplication() {
    event_loop_.set_wake_callback({});
#if TAOPP_HAS_XAML_ISLANDS
    if (impl_->xaml_theme_listener_registered) {
        impl_->xaml_settings.ColorValuesChanged(impl_->xaml_theme_token);
        impl_->xaml_theme_listener_registered = false;
    }
    impl_->xaml_theme_lifetime.reset();
#endif
    while (!impl_->windows.empty()) {
        const auto entry = impl_->windows.begin();
        if (entry->second->handle != nullptr) {
            DestroyWindow(entry->second->handle);
        } else {
            impl_->windows.erase(entry);
        }
    }
    impl_->windows_by_handle.clear();
#if TAOPP_HAS_XAML_ISLANDS
    impl_->xaml_settings = nullptr;
    impl_->xaml_manager = nullptr;
    if (impl_->xaml_apartment_initialized) {
        winrt::uninit_apartment();
    }
#endif
}

NativeResult NativeApplication::create_window(const WindowOptions& options, WindowId& id) {
    if (options.logical_size.width <= 0 || options.logical_size.height <= 0) {
        return NativeResult::invalid("window dimensions must be positive");
    }
    if (!impl_->ensure_window_class()) {
        return NativeResult::system_error(last_error_message("RegisterClassExW"));
    }

    id = event_loop_.create_window(options);
    auto record = std::make_unique<Impl::WindowRecord>();
    record->id = id;

    DWORD style = options.decorated ? WS_OVERLAPPEDWINDOW : WS_POPUP;
    if (!options.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    RECT bounds { 0, 0, options.logical_size.width, options.logical_size.height };
    AdjustWindowRectEx(&bounds, style, FALSE, 0);
    const std::wstring title = utf8_to_wide(options.title);
    const int x = options.has_position ? options.logical_position.x : CW_USEDEFAULT;
    const int y = options.has_position ? options.logical_position.y : CW_USEDEFAULT;
    HWND handle = CreateWindowExW(
        0,
        kWindowClassName,
        title.empty() ? L"taopp" : title.c_str(),
        style,
        x,
        y,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        impl_.get()
    );
    if (handle == nullptr) {
        event_loop_.destroy_window(id);
        return NativeResult::system_error(last_error_message("CreateWindowExW"));
    }

    record->handle = handle;
    impl_->windows_by_handle.emplace(handle, record.get());
    impl_->windows.emplace(id, std::move(record));
#if TAOPP_HAS_XAML_ISLANDS
    auto* created_window = impl_->find_window(id);
    if (!impl_->initialize_xaml(*created_window)) {
        const std::string error =
            impl_->xaml_error.empty() ? "Windows XAML Islands could not be activated" : impl_->xaml_error;
        DestroyWindow(handle);
        return NativeResult::system_error(error);
    }
#endif

    if (options.visible) {
        ShowWindow(handle, options.focused ? SW_SHOW : SW_SHOWNOACTIVATE);
        UpdateWindow(handle);
    }
    return NativeResult::success();
}

NativeResult NativeApplication::destroy_window(WindowId id) {
    auto* window = impl_->find_window(id);
    if (window == nullptr) {
        return NativeResult::invalid("unknown window id");
    }
    if (!DestroyWindow(window->handle)) {
        return NativeResult::system_error(last_error_message("DestroyWindow"));
    }
    return NativeResult::success();
}

NativeResult NativeApplication::add_control(WindowId window_id, const ControlOptions& options) {
    auto* window = impl_->find_window(window_id);
    if (window == nullptr || options.id == 0) {
        return NativeResult::invalid("unknown window or invalid control id");
    }
#if TAOPP_HAS_XAML_ISLANDS
    if (window->xaml_enabled) {
        return impl_->add_xaml_control(*window, options);
    }
#endif
    if (window->controls.find(options.id) != window->controls.end()) {
        return NativeResult::invalid("control id is already registered");
    }
    if (options.id > std::numeric_limits<WORD>::max()) {
        return NativeResult::invalid("Windows controls require ids no larger than 65535");
    }

    const wchar_t* class_name = L"STATIC";
    DWORD style = WS_CHILD | WS_VISIBLE;
    DWORD extended_style = 0;
    switch (options.type) {
    case ControlType::Label:
        style |= SS_LEFT;
        break;
    case ControlType::TextInput:
        class_name = L"EDIT";
        extended_style = WS_EX_CLIENTEDGE;
        style |= ES_AUTOHSCROLL | WS_TABSTOP;
        break;
    case ControlType::Button:
        class_name = L"BUTTON";
        style |= BS_PUSHBUTTON | WS_TABSTOP;
        break;
    case ControlType::Toggle:
        class_name = L"BUTTON";
        style |= BS_AUTOCHECKBOX | WS_TABSTOP;
        break;
    }

    const float scale_factor = window_scale(window->handle);
    const int x = scale_value(options.logical_bounds.x, scale_factor);
    const int y = scale_value(options.logical_bounds.y, scale_factor);
    const int width = std::max(1, scale_value(options.logical_bounds.width, scale_factor));
    const int height = std::max(1, scale_value(options.logical_bounds.height, scale_factor));
    const std::wstring text = utf8_to_wide(options.text);
    HWND control = CreateWindowExW(
        extended_style,
        class_name,
        text.c_str(),
        style,
        x,
        y,
        width,
        height,
        window->handle,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(options.id)),
        GetModuleHandleW(nullptr),
        nullptr
    );
    if (control == nullptr) {
        return NativeResult::system_error(last_error_message("CreateWindowExW control"));
    }

    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    if (options.type == ControlType::Toggle && options.checked) {
        SendMessageW(control, BM_SETCHECK, BST_CHECKED, 0);
    }
    EnableWindow(control, options.enabled ? TRUE : FALSE);
    window->controls.emplace(options.id, control);
    window->controls_by_handle.emplace(control, options.id);
    return NativeResult::success();
}

NativeResult NativeApplication::set_control_text(WindowId window_id, ControlId control_id, const std::string& text) {
    auto* window = impl_->find_window(window_id);
    if (window == nullptr) {
        return NativeResult::invalid("unknown window id");
    }
#if TAOPP_HAS_XAML_ISLANDS
    if (window->xaml_enabled) {
        const auto found = window->xaml_controls.find(control_id);
        if (found == window->xaml_controls.end()) {
            return NativeResult::invalid("unknown control id");
        }
        try {
            switch (window->xaml_control_types.at(control_id)) {
            case ControlType::Label:
                found->second.as<XamlTextBlock>().Text(winrt::to_hstring(text));
                break;
            case ControlType::TextInput:
                found->second.as<XamlTextBox>().Text(winrt::to_hstring(text));
                break;
            case ControlType::Button:
                found->second.as<XamlButton>().Content(winrt::box_value(winrt::to_hstring(text)));
                break;
            case ControlType::Toggle: {
                const auto header_found = window->xaml_toggle_headers.find(control_id);
                if (header_found != window->xaml_toggle_headers.end()) {
                    header_found->second.Text(winrt::to_hstring(text));
                } else {
                    found->second.as<XamlToggleSwitch>().Header(winrt::box_value(winrt::to_hstring(text)));
                }
                break;
            }
            }
            window->xaml_options.at(control_id).text = text;
            window->xaml_texts[control_id] = text;
            return NativeResult::success();
        } catch (...) {
            return NativeResult::system_error("XAML control text update failed");
        }
    }
#endif
    const auto found = window->controls.find(control_id);
    if (found == window->controls.end()) {
        return NativeResult::invalid("unknown control id");
    }
    const std::wstring wide_text = utf8_to_wide(text);
    if (!SetWindowTextW(found->second, wide_text.c_str())) {
        return NativeResult::system_error(last_error_message("SetWindowTextW"));
    }
    return NativeResult::success();
}

std::optional<std::string> NativeApplication::control_text(WindowId window_id, ControlId control_id) const {
    const auto* window = impl_->find_window(window_id);
    if (window == nullptr) {
        return std::nullopt;
    }
#if TAOPP_HAS_XAML_ISLANDS
    if (window->xaml_enabled) {
        const auto found = window->xaml_controls.find(control_id);
        if (found == window->xaml_controls.end()) {
            return std::nullopt;
        }
        try {
            switch (window->xaml_control_types.at(control_id)) {
            case ControlType::Label:
                return xaml_text(found->second.as<XamlTextBlock>().Text());
            case ControlType::TextInput:
                return xaml_text(found->second.as<XamlTextBox>().Text());
            case ControlType::Toggle:
                return found->second.as<XamlToggleSwitch>().IsOn() ? "on" : "off";
            case ControlType::Button:
                return window->xaml_texts.at(control_id);
            }
        } catch (...) {
            return std::nullopt;
        }
    }
#endif
    const auto found = window->controls.find(control_id);
    if (found == window->controls.end()) {
        return std::nullopt;
    }

    const int length = GetWindowTextLengthW(found->second);
    std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(found->second, buffer.data(), length + 1);
    return wide_to_utf8(buffer.data(), length);
}

std::optional<NativeWindowHandle> NativeApplication::native_window(WindowId window_id) const {
    const auto* window = impl_->find_window(window_id);
    if (window == nullptr) {
        return std::nullopt;
    }
    return NativeWindowHandle { NativeWindowHandleKind::Win32, reinterpret_cast<std::uintptr_t>(window->handle) };
}

void NativeApplication::set_action_callback(ActionCallback callback) { impl_->action_callback = std::move(callback); }

void NativeApplication::set_close_requested_callback(CloseRequestedCallback callback) {
    impl_->close_requested_callback = std::move(callback);
}

int NativeApplication::run() {
    PeekMessageW(nullptr, nullptr, 0, 0, PM_NOREMOVE);
    event_loop_.set_wake_callback([this] { PostThreadMessageW(impl_->thread_id, kWakeMessage, 0, 0); });

    while (!event_loop_.exit_requested()) {
        event_loop_.process_pending();

        MSG message {};
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == 0) {
            event_loop_.exit(0);
            break;
        }
        if (result == -1) {
            event_loop_.exit(static_cast<int>(GetLastError()));
            break;
        }
        if (message.message == kWakeMessage) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    event_loop_.set_wake_callback({});
    return event_loop_.run(RunMode::Poll);
}

void NativeApplication::exit(int code) {
    event_loop_.exit(code);
    PostThreadMessageW(impl_->thread_id, kWakeMessage, 0, 0);
}

LRESULT CALLBACK NativeApplication::Impl::window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    auto* application = static_cast<Impl*>(GetPropW(window, L"taopp.impl"));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        application = static_cast<Impl*>(create->lpCreateParams);
        SetPropW(window, L"taopp.impl", application);
    }
    if (application == nullptr) {
        return DefWindowProcW(window, message, w_param, l_param);
    }

    auto* record = application->find_window(window);
    if (record == nullptr) {
        return DefWindowProcW(window, message, w_param, l_param);
    }

    switch (message) {
    case WM_CLOSE: {
        bool allow_close = true;
        if (application->close_requested_callback) {
            try {
                allow_close = application->close_requested_callback(record->id);
            } catch (...) {
                allow_close = false;
            }
        }
        if (allow_close) {
            DestroyWindow(window);
        }
        return 0;
    }
    case WM_SIZE:
        application->event_loop.emit_window_event(
            { WindowEventType::Resized,
              record->id,
              { static_cast<std::int32_t>(LOWORD(l_param)), static_cast<std::int32_t>(HIWORD(l_param)) } }
        );
#if TAOPP_HAS_XAML_ISLANDS
        application->resize_xaml(*record);
#endif
        break;
    case WM_MOVE:
        application->event_loop.emit_window_event(
            { WindowEventType::Moved,
              record->id,
              {},
              { static_cast<std::int32_t>(static_cast<short>(LOWORD(l_param))),
                static_cast<std::int32_t>(static_cast<short>(HIWORD(l_param))) } }
        );
        break;
    case WM_SETFOCUS:
        application->event_loop.emit_window_event({ WindowEventType::Focused, record->id });
        break;
    case WM_KILLFOCUS:
        application->event_loop.emit_window_event({ WindowEventType::Unfocused, record->id });
        break;
    case WM_DPICHANGED: {
        const float scale_factor = static_cast<float>(LOWORD(w_param)) / 96.0F;
        application->event_loop.emit_window_event(
            { WindowEventType::ScaleFactorChanged, record->id, {}, {}, scale_factor }
        );
        const auto* suggested = reinterpret_cast<const RECT*>(l_param);
        if (suggested != nullptr) {
            SetWindowPos(
                window,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOACTIVATE | SWP_NOZORDER
            );
        }
        break;
    }
    case WM_COMMAND:
        if (HIWORD(w_param) == BN_CLICKED && application->action_callback) {
            const auto found = record->controls_by_handle.find(reinterpret_cast<HWND>(l_param));
            if (found != record->controls_by_handle.end()) {
                std::string text;
                const auto text_length = GetWindowTextLengthW(reinterpret_cast<HWND>(l_param));
                std::vector<wchar_t> buffer(static_cast<std::size_t>(text_length) + 1, L'\0');
                GetWindowTextW(reinterpret_cast<HWND>(l_param), buffer.data(), text_length + 1);
                text = wide_to_utf8(buffer.data(), text_length);
                try {
                    application->action_callback(record->id, found->second, std::move(text));
                } catch (...) {
                    // Native callbacks are application code and cannot unwind WndProc.
                }
            }
        }
        break;
    case WM_DESTROY:
#if TAOPP_HAS_XAML_ISLANDS
        application->close_xaml(*record);
#endif
        RemovePropW(window, L"taopp.impl");
        application->windows_by_handle.erase(window);
        application->event_loop.destroy_window(record->id);
        application->windows.erase(record->id);
        if (application->windows.empty()) {
            application->event_loop.exit(0);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, w_param, l_param);
}

} // namespace taopp