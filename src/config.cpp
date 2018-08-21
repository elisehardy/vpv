#include <string>
#include <cassert>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "lua.hpp"
#include "kaguya/kaguya.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "Window.hpp"
#include "Sequence.hpp"
#include "Player.hpp"
#include "Image.hpp"
#include "View.hpp"
#include "events.hpp"

// generated by cmake
extern "C" int load_luafiles(lua_State* L);

static lua_State* L;
static kaguya::State* state;

KAGUYA_MEMBER_FUNCTION_OVERLOADS(sequence_set_edit, Sequence, setEdit, 1, 2)
KAGUYA_FUNCTION_OVERLOADS(is_key_pressed, isKeyPressed, 1, 2)

std::vector<Window*>& getWindows() {
    return gWindows;
}

void config::load()
{
    L = luaL_newstate();
    assert(L);
    luaL_openlibs(L);

    state = new kaguya::State(L);

    (*state)["iskeydown"] = isKeyDown;
    (*state)["iskeypressed"] = kaguya::function(is_key_pressed());
    (*state)["ismouseclicked"] = ImGui::IsMouseClicked;
    (*state)["ismousedown"] = ImGui::IsMouseDown;;
    (*state)["ismousereleased"] = ImGui::IsMouseReleased;

    (*state)["ImVec2"].setClass(kaguya::UserdataMetatable<ImVec2>()
                             .setConstructors<ImVec2(),ImVec2(float,float)>()
                             .addProperty("x", &ImVec2::x)
                             .addProperty("y", &ImVec2::y)
                             .addStaticFunction("__add", [](const ImVec2& a, const ImVec2& b) { return a+b; })
                             .addStaticFunction("__sub", [](const ImVec2& a, const ImVec2& b) { return a-b; })
                             .addStaticOverloadedFunctions("__div",
                                                           [](const ImVec2& a, const ImVec2& b) { return a/b; },
                                                           [](const ImVec2& a, const float& b) { return a/b; }
                                                          )
                            );

    (*state)["ImVec4"].setClass(kaguya::UserdataMetatable<ImVec4>()
                             .setConstructors<ImVec4(),ImVec4(float,float,float,float)>()
                             .addProperty("x", &ImVec4::x)
                             .addProperty("y", &ImVec4::y)
                             .addProperty("z", &ImVec4::z)
                             .addProperty("w", &ImVec4::w)
                            );

    (*state)["ImRect"].setClass(kaguya::UserdataMetatable<ImRect>()
                             .setConstructors<ImRect(),ImRect(ImVec2,ImVec2)>()
                             .addProperty("min", &ImRect::Min)
                             .addProperty("max", &ImRect::Max)
                             .addFunction("get_width", &ImRect::GetWidth)
                             .addFunction("get_height", &ImRect::GetHeight)
                             .addFunction("get_size", &ImRect::GetSize)
                             .addFunction("get_center", &ImRect::GetCenter)
                            );

    (*state)["ImGuiStyle"].setClass(kaguya::UserdataMetatable<ImGuiStyle>()
                             .setConstructors<ImGuiStyle()>()
                             .addProperty("Alpha", &ImGuiStyle::Alpha)
                             .addProperty("WindowPadding", &ImGuiStyle::WindowPadding)
                             .addProperty("WindowRounding", &ImGuiStyle::WindowRounding)
                             .addProperty("WindowBorderSize", &ImGuiStyle::WindowBorderSize)
                             .addProperty("WindowMinSize", &ImGuiStyle::WindowMinSize)
                             .addProperty("WindowTitleAlign", &ImGuiStyle::WindowTitleAlign)
                             .addProperty("ChildRounding", &ImGuiStyle::ChildRounding)
                             .addProperty("ChildBorderSize", &ImGuiStyle::ChildBorderSize)
                             .addProperty("PopupRounding", &ImGuiStyle::PopupRounding)
                             .addProperty("PopupBorderSize", &ImGuiStyle::PopupBorderSize)
                             .addProperty("FramePadding", &ImGuiStyle::FramePadding)
                             .addProperty("FrameRounding", &ImGuiStyle::FrameRounding)
                             .addProperty("FrameBorderSize", &ImGuiStyle::FrameBorderSize)
                             .addProperty("ItemSpacing", &ImGuiStyle::ItemSpacing)
                             .addProperty("ItemInnerSpacing", &ImGuiStyle::ItemInnerSpacing)
                             .addProperty("TouchExtraPadding", &ImGuiStyle::TouchExtraPadding)
                             .addProperty("IndentSpacing", &ImGuiStyle::IndentSpacing)
                             .addProperty("ColumnsMinSpacing", &ImGuiStyle::ColumnsMinSpacing)
                             .addProperty("ScrollbarSize", &ImGuiStyle::ScrollbarSize)
                             .addProperty("ScrollbarRounding", &ImGuiStyle::ScrollbarRounding)
                             .addProperty("GrabMinSize", &ImGuiStyle::GrabMinSize)
                             .addProperty("GrabRounding", &ImGuiStyle::GrabRounding)
                             .addProperty("ButtonTextAlign", &ImGuiStyle::ButtonTextAlign)
                             .addProperty("DisplayWindowPadding", &ImGuiStyle::DisplayWindowPadding)
                             .addProperty("DisplaySafeAreaPadding", &ImGuiStyle::DisplaySafeAreaPadding)
                             .addProperty("MouseCursorScale", &ImGuiStyle::MouseCursorScale)
                             .addProperty("AntiAliasedLines", &ImGuiStyle::AntiAliasedLines)
                             .addProperty("AntiAliasedFill", &ImGuiStyle::AntiAliasedFill)
                             .addProperty("CurveTessellationTol", &ImGuiStyle::CurveTessellationTol)
                             .addProperty("Colors", &ImGuiStyle::Colors)
                             );

    (*state)["ImGuiCol_Text"] = ImGuiCol_Text + 1;
    (*state)["ImGuiCol_TextDisabled"] = ImGuiCol_TextDisabled + 1;
    (*state)["ImGuiCol_WindowBg"] = ImGuiCol_WindowBg + 1;
    (*state)["ImGuiCol_ChildBg"] = ImGuiCol_ChildBg + 1;
    (*state)["ImGuiCol_PopupBg"] = ImGuiCol_PopupBg + 1;
    (*state)["ImGuiCol_Border"] = ImGuiCol_Border + 1;
    (*state)["ImGuiCol_BorderShadow"] = ImGuiCol_BorderShadow + 1;
    (*state)["ImGuiCol_FrameBg"] = ImGuiCol_FrameBg + 1;
    (*state)["ImGuiCol_FrameBgHovered"] = ImGuiCol_FrameBgHovered + 1;
    (*state)["ImGuiCol_FrameBgActive"] = ImGuiCol_FrameBgActive + 1;
    (*state)["ImGuiCol_TitleBg"] = ImGuiCol_TitleBg + 1;
    (*state)["ImGuiCol_TitleBgActive"] = ImGuiCol_TitleBgActive + 1;
    (*state)["ImGuiCol_TitleBgCollapsed"] = ImGuiCol_TitleBgCollapsed + 1;
    (*state)["ImGuiCol_MenuBarBg"] = ImGuiCol_MenuBarBg + 1;
    (*state)["ImGuiCol_ScrollbarBg"] = ImGuiCol_ScrollbarBg + 1;
    (*state)["ImGuiCol_ScrollbarGrab"] = ImGuiCol_ScrollbarGrab + 1;
    (*state)["ImGuiCol_ScrollbarGrabHovered"] = ImGuiCol_ScrollbarGrabHovered + 1;
    (*state)["ImGuiCol_ScrollbarGrabActive"] = ImGuiCol_ScrollbarGrabActive + 1;
    (*state)["ImGuiCol_CheckMark"] = ImGuiCol_CheckMark + 1;
    (*state)["ImGuiCol_SliderGrab"] = ImGuiCol_SliderGrab + 1;
    (*state)["ImGuiCol_SliderGrabActive"] = ImGuiCol_SliderGrabActive + 1;
    (*state)["ImGuiCol_Button"] = ImGuiCol_Button + 1;
    (*state)["ImGuiCol_ButtonHovered"] = ImGuiCol_ButtonHovered + 1;
    (*state)["ImGuiCol_ButtonActive"] = ImGuiCol_ButtonActive + 1;
    (*state)["ImGuiCol_Header"] = ImGuiCol_Header + 1;
    (*state)["ImGuiCol_HeaderHovered"] = ImGuiCol_HeaderHovered + 1;
    (*state)["ImGuiCol_HeaderActive"] = ImGuiCol_HeaderActive + 1;
    (*state)["ImGuiCol_Separator"] = ImGuiCol_Separator + 1;
    (*state)["ImGuiCol_SeparatorHovered"] = ImGuiCol_SeparatorHovered + 1;
    (*state)["ImGuiCol_SeparatorActive"] = ImGuiCol_SeparatorActive + 1;
    (*state)["ImGuiCol_ResizeGrip"] = ImGuiCol_ResizeGrip + 1;
    (*state)["ImGuiCol_ResizeGripHovered"] = ImGuiCol_ResizeGripHovered + 1;
    (*state)["ImGuiCol_ResizeGripActive"] = ImGuiCol_ResizeGripActive + 1;
    (*state)["ImGuiCol_PlotLines"] = ImGuiCol_PlotLines + 1;
    (*state)["ImGuiCol_PlotLinesHovered"] = ImGuiCol_PlotLinesHovered + 1;
    (*state)["ImGuiCol_PlotHistogram"] = ImGuiCol_PlotHistogram + 1;
    (*state)["ImGuiCol_PlotHistogramHovered"] = ImGuiCol_PlotHistogramHovered + 1;
    (*state)["ImGuiCol_TextSelectedBg"] = ImGuiCol_TextSelectedBg + 1;
    (*state)["ImGuiCol_ModalWindowDarkening"] = ImGuiCol_ModalWindowDarkening + 1;
    (*state)["ImGuiCol_DragDropTarget"] = ImGuiCol_DragDropTarget + 1;
    (*state)["ImGuiCol_NavHighlight"] = ImGuiCol_NavHighlight + 1;
    (*state)["ImGuiCol_NavWindowingHighlight"] = ImGuiCol_NavWindowingHighlight + 1;

    (*state)["PLAMBDA"] = PLAMBDA;
    (*state)["OCTAVE"] = OCTAVE;
    (*state)["GIMP"] = GMIC;

    (*state)["Player"].setClass(kaguya::UserdataMetatable<Player>()
                             .setConstructors<Player()>()
                             .addProperty("frame", &Player::frame)
                             .addFunction("check_bounds", &Player::checkBounds)
                            );

    (*state)["View"].setClass(kaguya::UserdataMetatable<View>()
                             .setConstructors<View()>()
                             .addProperty("center", &View::center)
                             .addProperty("zoom", &View::zoom)
                             .addProperty("should_rescale", &View::shouldRescale)
                             .addProperty("svg_offset", &View::svgOffset)
                            );

    (*state)["Image"].setClass(kaguya::UserdataMetatable<Image>()
                             .addProperty("size", &Image::size)
                            );

    (*state)["Sequence"].setClass(kaguya::UserdataMetatable<Sequence>()
                             .setConstructors<Sequence()>()
                             .addProperty("player", &Sequence::player)
                             .addProperty("view", &Sequence::view)
                             .addProperty("image", &Sequence::image)
                             .addFunction("set_edit", sequence_set_edit())
                             .addFunction("get_edit", &Sequence::getEdit)
                             .addFunction("get_id", &Sequence::getId)
                            );

    (*state)["Window"].setClass(kaguya::UserdataMetatable<Window>()
                             .setConstructors<Window()>()
                             .addProperty("opened", &Window::opened)
                             .addProperty("position", &Window::position)
                             .addProperty("size", &Window::size)
                             .addProperty("force_geometry", &Window::forceGeometry)
                             .addProperty("content_rect", &Window::contentRect)
                             .addProperty("index", &Window::index)
                             .addProperty("sequences", &Window::sequences)
                            );

    (*state)["gHoveredPixel"] = &gHoveredPixel;
    (*state)["get_windows"] = getWindows;

    load_luafiles(L);
}

float config::get_float(const std::string& name)
{
#if 0
    lua_getglobal(L, name.c_str());
    float num = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    return num;
#else
    kaguya::State state(L);
    return state[name.c_str()];
#endif
}

bool config::get_bool(const std::string& name)
{
#if 0
    lua_getglobal(L, name.c_str());
    float num = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return num;
#else
    kaguya::State state(L);
    return state[name.c_str()];
#endif
}

std::string config::get_string(const std::string& name)
{
#if 0
    lua_getglobal(L, name.c_str());
    const char* str = luaL_checkstring(L, -1);
    std::string ret(str);
    lua_pop(L, 1);
    return ret;
#else
    kaguya::State state(L);
    return state[name.c_str()];
#endif
}

#include "shaders.hpp"
void config::load_shaders()
{
#if 0
    lua_getglobal(L, "SHADERS");
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        const char* name = lua_tostring(L, -2);
        const char* code = lua_tostring(L, -1);
        loadShader(name, code);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
#else
    kaguya::State state(L);
    std::map<std::string,std::string> shaders = state["SHADERS"];
    for (auto s : shaders) {
        loadShader(s.first, s.second);
    }
#endif
}

kaguya::State& config::get_lua()
{
    return *state;
}

