#include <iostream>
#include <fstream>
#include <cmath>

#ifndef SDL
#include <SFML/OpenGL.hpp>
#else
#include <GL/gl3w.h>
#endif

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

extern "C" {
#include "iio.h"
}

#include "globals.hpp"
#include "Window.hpp"
#include "Sequence.hpp"
#include "View.hpp"
#include "Player.hpp"
#include "Colormap.hpp"
#include "Image.hpp"
#include "ImageProvider.hpp"
#include "ImageCollection.hpp"
#include "Shader.hpp"
#include "layout.hpp"
#include "SVG.hpp"
#include "Histogram.hpp"
#include "EditGUI.hpp"
#include "config.hpp"
#include "events.hpp"
#include "imgui_custom.hpp"

static ImRect getClipRect();

static bool file_exists(const char *fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

Window::Window()
{
    static int id = 0;
    id++;
    ID = "Window " + std::to_string(id);

    histogram = std::make_shared<Histogram>();

    opened = true;
    index = 0;
    shouldAskFocus = false;
    screenshot = false;
    dontLayout = false;
    alwaysOnTop = false;
}

void Window::display()
{
    screenshot = false;

    int index = std::find(gWindows.begin(), gWindows.end(), this) - gWindows.begin();
    char d[2] = {static_cast<char>('1' + index), 0};
    bool isKeyFocused = index <= 9 && isKeyPressed(d) && !isKeyDown("alt") && !isKeyDown("s");

    if (isKeyFocused && !opened) {
        opened = true;
        relayout(false);
    }

    bool gotFocus = false;
    if (isKeyFocused || shouldAskFocus) {
        ImGui::SetNextWindowFocus();
        shouldAskFocus = false;
        gotFocus = true;
    }

    if (!opened) {
        return;
    }

    if (forceGeometry) {
        ImGui::SetNextWindowPos(position);
        ImGui::SetNextWindowSize(size);
        forceGeometry = false;
    }

    auto prevStyle = ImGui::GetStyle();
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    ImGui::GetStyle().WindowPadding = ImVec2(1, 1);

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###%s", getTitle().c_str(), ID.c_str());
    int flags = ImGuiWindowFlags_NoScrollbar
                | ImGuiWindowFlags_NoScrollWithMouse
                | ImGuiWindowFlags_NoFocusOnAppearing
                | ImGuiWindowFlags_NoCollapse
                | (getLayoutName()!="free" ? ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize : 0);
    if (gShowWindowBar == 0 || (gShowWindowBar == 2 && gWindows.size() == 1)) {
        flags |= ImGuiWindowFlags_NoTitleBar;
    }
    if (!ImGui::Begin(buf, &opened, flags)) {
        ImGui::End();
        ImGui::GetStyle() = prevStyle;
        return;
    }

    if (alwaysOnTop) {
        ImGui::BringFront();
    }

    ImGui::GetStyle() = prevStyle;

    // just closed
    if (!opened) {
        relayout(false);
    }

    if (ImGui::IsWindowFocused()) {
        if (isKeyPressed(" ")) {
            this->index = (this->index + 1) % sequences.size();
        }
        if (isKeyPressed("\b")) {
            this->index = (sequences.size() + this->index - 1) % sequences.size();
        }
        if (!gotFocus && isKeyPressed("\t")) {
            int d = isKeyDown("shift") ? -1 : 1;
            int nindex = (index + d + gWindows.size()) % gWindows.size();
            gWindows[nindex]->shouldAskFocus = true;
        }
    }

    Sequence* seq = getCurrentSequence();

    if (!seq) {
        ImGui::End();
        return;
    }

    displaySequence(*seq);

    ImGui::End();
}

void Window::displaySequence(Sequence& seq)
{
    View* view = seq.view;

    bool focusedit = false;
    float factor = seq.getViewRescaleFactor();
    ImRect clip = getClipRect();
    ImVec2 winSize = clip.Max - clip.Min;

    if (seq.colormap && seq.view && seq.player) {
        if (gShowImage && seq.colormap->shader) {
            ImGui::PushClipRect(clip.Min, clip.Max, true);
            displayarea.draw(seq.getCurrentImage(), clip.Min, winSize, seq.colormap, seq.view, factor);
            ImGui::PopClipRect();
        }

        std::vector<const SVG*> svgs = seq.getCurrentSVGs();
        if (!svgs.empty()) {
            ImVec2 TL = view->image2window(seq.view->svgOffset, displayarea.getCurrentSize(), winSize, factor) + clip.Min;
            ImGui::PushClipRect(clip.Min, clip.Max, true);
            for (int i = 0; i < svgs.size(); i++) {
                if (svgs[i] && (i >= 9 || gShowSVGs[i]))
                    svgs[i]->draw(TL, seq.view->zoom*factor);
            }
            ImGui::PopClipRect();
        }

        contentRect = clip;

        if (gSelectionShown) {
            ImRect clip = getClipRect();
            ImVec2 off1, off2;
            if (gSelectionFrom.x <= gSelectionTo.x)
                off2.x = 1;
            else
                off1.x = 1;
            if (gSelectionFrom.y <= gSelectionTo.y)
                off2.y = 1;
            else
                off1.y = 1;
            ImVec2 from = view->image2window(gSelectionFrom+off1, displayarea.getCurrentSize(), winSize, factor);
            ImVec2 to = view->image2window(gSelectionTo+off2, displayarea.getCurrentSize(), winSize, factor);
            from += clip.Min;
            to += clip.Min;
            ImU32 green = ImGui::GetColorU32(ImVec4(0,1,0,1));
            ImGui::GetWindowDrawList()->AddRect(from, to, green);
            char buf[2048];
            snprintf(buf, sizeof(buf), "%d %d", (int)gSelectionFrom.x, (int)gSelectionFrom.y);
            ImGui::GetWindowDrawList()->AddText(from, green, buf);
            snprintf(buf, sizeof(buf), "  %d %d (w=%d,h=%d,d=%.2f)", (int)gSelectionTo.x, (int)gSelectionTo.y,
                     (int)std::abs((gSelectionTo-gSelectionFrom).x),
                     (int)std::abs((gSelectionTo-gSelectionFrom).y),
                     std::sqrt(ImLengthSqr(gSelectionTo - gSelectionFrom)));
            ImGui::GetWindowDrawList()->AddText(to, green, buf);
        }

        ImVec2 from = view->image2window(gHoveredPixel, displayarea.getCurrentSize(), winSize, factor);
        ImVec2 to = view->image2window(gHoveredPixel+ImVec2(1,1), displayarea.getCurrentSize(), winSize, factor);
        from += clip.Min;
        to += clip.Min;
        if (view->zoom*factor >= gDisplaySquareZoom) {
            ImU32 green = ImGui::GetColorU32(ImVec4(0,1,0,1));
            ImU32 black = ImGui::GetColorU32(ImVec4(0,0,0,1));
            if (from.x+1.f == to.x && from.y+1.f == to.y) {
                // somehow this is necessary, otherwise the square disappear :(
                to.x += 1e-3;
                to.y += 1e-3;
            }
            ImGui::GetWindowDrawList()->AddRect(from, to, black, 0, ~0, 2.5f);
            ImGui::GetWindowDrawList()->AddRect(from, to, green);
        }

        if (seq.imageprovider && !seq.imageprovider->isLoaded()) {
            ImVec2 pos = ImGui::GetCursorPos();
            const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
            const ImU32 bg = ImColor(100,100,100);
            ImGui::BufferingBar("##bar", seq.imageprovider->getProgressPercentage(),
                                ImVec2(ImGui::GetWindowWidth(), 6), bg, col);
            ImGui::SetCursorPos(pos);
        }

        if (seq.image) {
            std::shared_ptr<Image> image = seq.image;
            float r = (float) image->h / image->w;
            int w = 82;
            float alpha = gShowView > 20 ? 1. : gShowView/20.;
            ImU32 gray = ImGui::GetColorU32(ImVec4(1,1,1,0.6*alpha));
            ImU32 black = ImGui::GetColorU32(ImVec4(0,0,0,0.4*alpha));
            ImVec2 size = ImVec2(w, r*w);
            ImVec2 p1 = view->window2image(ImVec2(0, 0), displayarea.getCurrentSize(), winSize, factor);
            ImVec2 p2 = view->window2image(winSize, displayarea.getCurrentSize(), winSize, factor);
            p1 = p1 * size / displayarea.getCurrentSize();
            p2 = p2 * size / displayarea.getCurrentSize();
            int border = 5;
            ImVec2 pos(clip.Max.x - size.x - border, clip.Min.y + border);
            ImRect rout(pos, pos + size);
            ImRect rin(rout.Min+p1, rout.Min+p2);
            rin.ClipWithFull(rout);
            if ((rin.GetWidth() < rout.GetWidth() || rin.GetHeight() < rout.GetHeight()) && gShowView) {
                ImGui::GetWindowDrawList()->AddRectFilled(rout.Min, rout.Max, black);
                ImGui::GetWindowDrawList()->AddRectFilled(rin.Min, rin.Max, gray);
            }
        }
    }

    static bool showthings = false;
    if (ImGui::IsWindowFocused() && isKeyPressed("F6")) {
        showthings = !showthings;
    }
    if (showthings) {
        ImGui::BeginChildFrame(ImGui::GetID(".."), ImVec2(0, size.y * 0.25));
        for (size_t i = 0; i < sequences.size(); i++) {
            const Sequence* seq = sequences[i];
            bool flags = index == i ? ImGuiTreeNodeFlags_DefaultOpen : 0;
            if (ImGui::CollapsingHeader(seq->glob.c_str(), flags)) {
                int frame = seq->player->frame - 1;
                for (int f = 0; f < seq->collection->getLength(); f++) {
                    const std::string& filename = seq->collection->getFilename(f);
                    bool current = f == frame;
                    if (ImGui::Selectable(filename.c_str(), current)) {
                        seq->player->frame = f + 1;
                    }
                }
            }
        }
        ImGui::EndChildFrame();
    }

    if (!seq.valid || !seq.player)
        return;

    auto f = config::get_lua()["on_window_tick"];
    if (f) {
        f(this, ImGui::IsWindowFocused());
    }

    if (ImGui::IsWindowFocused()) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;

        bool zooming = isKeyDown("z");

        ImRect winclip = getClipRect();
        ImVec2 cursor = ImGui::GetMousePos() - winclip.Min;
        ImVec2 im = ImFloor(view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor));
        gHoveredPixel = im;

        if (zooming && ImGui::GetIO().MouseWheel != 0.f) {
            ImRect clip = getClipRect();
            ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
            ImVec2 pos = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);

            view->changeZoom(view->zoom * (1 + 0.1 * ImGui::GetIO().MouseWheel));

            ImVec2 pos2 = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);
            view->center += (pos - pos2) / displayarea.getCurrentSize();
            gShowView = MAX_SHOWVIEW;
        }
        if (isKeyPressed("i")) {
            view->changeZoom(std::pow(2, floor(log2(view->zoom) + 1)));
            gShowView = MAX_SHOWVIEW;
        }
        if (isKeyPressed("o")) {
            view->changeZoom(std::pow(2, ceil(log2(view->zoom) - 1)));
            gShowView = MAX_SHOWVIEW;
        }

        bool dragging = ImGui::IsMouseDown(0) && (delta.x || delta.y);
        if (ImGui::IsWindowHovered() || (dragging && !ImGui::IsAnyItemActive())) {
            if (!ImGui::IsMouseClicked(0) && dragging) {
                ImVec2 pos = view->window2image(ImVec2(0, 0), displayarea.getCurrentSize(), winSize, factor);
                ImVec2 pos2 = view->window2image(delta, displayarea.getCurrentSize(), winSize, factor);
                ImVec2 diff = pos - pos2;
                view->center += diff / displayarea.getCurrentSize();
                gShowView = MAX_SHOWVIEW;
            }

            if (ImGui::IsMouseClicked(1)) {
                gSelecting = true;

                ImRect clip = getClipRect();
                ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
                ImVec2 pos = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);
                gSelectionFrom = ImFloor(pos);
                gSelectionShown = true;
            }
        }

        if (gSelecting) {
            if (ImGui::IsMouseDown(1)) {
                ImRect clip = getClipRect();
                ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
                ImVec2 pos = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);
                gSelectionTo = ImFloor(pos);
            } else if (ImGui::IsMouseReleased(1)) {
                gSelecting = false;
                if (std::hypot(gSelectionFrom.x - gSelectionTo.x, gSelectionFrom.y - gSelectionTo.y) <= 1) {
                    gSelectionShown = false;
                } else {
                    ImVec2 diff = gSelectionTo - gSelectionFrom;
                    printf("%d %d %d %d\n", (int)gSelectionFrom.x, (int)gSelectionFrom.y, (int)diff.x, (int)diff.y);
                }
            }

            for (auto win : gWindows) {
                Sequence* s = win->getCurrentSequence();
                if (!s) continue;
                std::shared_ptr<Image> img = s->getCurrentImage();
                if (!img) continue;
                win->histogram->request(img, img->min, img->max, gSmoothHistogram ? Histogram::SMOOTH : Histogram::EXACT,
                                        ImRect(gSelectionFrom, gSelectionTo));
            }
        }

        if (isKeyPressed("r")) {
            view->center = ImVec2(0.5, 0.5);
            gShowView = MAX_SHOWVIEW;
            if (isKeyDown("shift")) {
                view->resetZoom();
            } else {
                view->setOptimalZoom(contentRect.GetSize(), displayarea.getCurrentSize(), factor);
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && ImGui::IsWindowHovered()
            && !zooming && (ImGui::GetIO().MouseWheel || ImGui::GetIO().MouseWheelH)) {
            static float f = 0.1f;
            std::shared_ptr<Image> img = seq.getCurrentImage();
            if (img) {
                if (isKeyDown("shift")) {
                    seq.colormap->radius = std::max(0.f, seq.colormap->radius * (1.f + f * ImGui::GetIO().MouseWheel));
                } else {
                    for (int i = 0; i < 3; i++) {
                        float newcenter = seq.colormap->center[i] + seq.colormap->radius * f * ImGui::GetIO().MouseWheel;
                        seq.colormap->center[i] = std::min(std::max(newcenter, img->min), img->max);
                    }
                }
                seq.colormap->radius = std::max(0.f, seq.colormap->radius * (1.f + f * ImGui::GetIO().MouseWheelH));
            }
        }

        if (!ImGui::GetIO().WantCaptureKeyboard && (delta.x || delta.y) && isKeyDown("shift")) {
            ImRect clip = getClipRect();
            ImVec2 cursor = ImGui::GetMousePos() - clip.Min;
            ImVec2 pos = view->window2image(cursor, displayarea.getCurrentSize(), winSize, factor);
            std::shared_ptr<Image> img = seq.getCurrentImage();
            if (img && pos.x >= 0 && pos.y >= 0 && pos.x < img->w && pos.y < img->h) {
                std::array<float,3> v{};
                int n = std::min(img->c, (size_t)3);
                img->getPixelValueAt(pos.x, pos.y, &v[0], n);
                float mean = 0;
                for (int i = 0; i < n; i++) mean += v[i] / n;
                if (!std::isnan(mean) && !std::isinf(mean)) {
                    if (isKeyDown("alt")) {
                        seq.colormap->center = v;
                    } else {
                        for (int i = 0; i < 3; i++)
                            seq.colormap->center[i] = mean;
                    }
                }
            }
        }

        if (seq.player) {
            // TODO: this delays the actual change of frame when we press left/right
            seq.player->checkShortcuts();
        }

        if (isKeyPressed("a")) {
            if (isKeyDown("shift")) {
                seq.snapScaleAndBias();
            } else if (isKeyDown("control")) {
                ImVec2 p1 = view->window2image(ImVec2(0, 0), displayarea.getCurrentSize(), winSize, factor);
                ImVec2 p2 = view->window2image(winSize, displayarea.getCurrentSize(), winSize, factor);
                seq.localAutoScaleAndBias(p1, p2);
            } else if (isKeyDown("alt")) {
                seq.cutScaleAndBias(config::get_float("SATURATION"));
            } else {
                seq.autoScaleAndBias();
            }
        }

        if (isKeyPressed("s") && !isKeyDown("control")) {
            if (isKeyDown("shift")) {
                seq.colormap->previousShader();
            } else {
                seq.colormap->nextShader();
            }
        }

        if (isKeyPressed("e")) {
            if (!seq.editGUI->isEditing()) {
                EditType type = PLAMBDA;
                if (isKeyDown("shift")) {
#ifdef USE_GMIC
                    type = EditType::GMIC;
#else
                    std::cerr << "GMIC isn't enabled, check your compilation." << std::endl;
#endif
                } else if (isKeyDown("control")) {
#ifdef USE_OCTAVE
                    type = EditType::OCTAVE;
#else
                    std::cerr << "Octave isn't enabled, check your compilation." << std::endl;
#endif
                }
                seq.setEdit(std::to_string(seq.getId()), type);
            }
            focusedit = true;
        }

        if (isKeyPressed(",")) {
            screenshot = true;
        }
    }

    if (!screenshot) {
        seq.editGUI->display(seq, focusedit);
    }

    if (!seq.error.empty()) {
        ImGui::TextColored(ImColor(255, 0, 0), "error: %s", seq.error.c_str());
    }

    if (gShowHud && seq.image) {
        displayInfo(seq);
    }
}

void Window::displayInfo(Sequence& seq)
{
    ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImGui::GetCursorPos());
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_AlwaysUseWindowPadding|ImGuiWindowFlags_NoFocusOnAppearing;

    auto prevstyle = ImGui::GetStyle();
    ImGui::GetStyle().WindowPadding = ImVec2(4, 3);
    ImGui::GetStyle().WindowRounding = 5.f;
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.5f);

    char buf[512];
    snprintf(buf, sizeof(buf), "%s###info%s", getTitle().c_str(), ID.c_str());
    ImGuiWindow* parent = ImGui::GetCurrentContext()->CurrentWindow;
    ImGui::Begin(buf, NULL, flags);
    ImGui::BringBefore(parent);

    seq.showInfo();

    float v[4] = {0};
    bool highlights = false;
    if (!seq.valid)
        goto end;

    ImGui::Separator();

    {
        ImVec2 im = gHoveredPixel;
        ImGui::Text("Pixel: (%d, %d)", (int)im.x, (int)im.y);

        std::shared_ptr<Image> img = seq.getCurrentImage();
        if (img && im.x >= 0 && im.y >= 0 && im.x < img->w && im.y < img->h) {
            highlights = true;
            img->getPixelValueAt(im.x, im.y, v, 4);
            if (img->c == 1) {
                ImGui::Text("Gray: %g", v[0]);
            } else if (img->c == 2) {
                ImGui::Text("Flow: %g, %g", v[0], v[1]);
            } else if (img->c == 3) {
                ImGui::Text("RGB: %g, %g, %g", v[0], v[1], v[2]);
            } else if (img->c == 4) {
                ImGui::Text("RGBA: %g, %g, %g, %g", v[0], v[1], v[2], v[3]);
            }
        } else {
            ImGui::Text("");
        }
    }

    if (gShowHistogram) {
        std::array<float,3> cmin, cmax;
        seq.colormap->getRange(cmin, cmax);

        std::shared_ptr<Image> img = seq.getCurrentImage();
        if (img) {
            std::shared_ptr<Histogram> imghist = img->histogram;
            imghist->draw(cmin, cmax, highlights ? v : 0);
            if (gSelectionShown) {
                histogram->draw(cmin, cmax, highlights ? v : 0);
            }
        }
    }

    if (ImGui::IsWindowFocused() && ImGui::GetIO().MouseDoubleClicked[0]) {
        gShowHud = false;
    }

end:
    ImGui::End();
    ImGui::GetStyle() = prevstyle;
}

void Window::displaySettings()
{
    if (ImGui::Checkbox("Opened", &opened))
        relayout(false);
    ImGui::Text("Sequences");
    ImGui::SameLine(); ImGui::ShowHelpMarker("Choose which sequences are associated with this window");
    ImGui::BeginChild("scrolling", ImVec2(350, ImGui::GetItemsLineHeightWithSpacing()*3 + 20),
                      true, ImGuiWindowFlags_HorizontalScrollbar);
    for (auto seq : gSequences) {
        auto it = std::find(sequences.begin(), sequences.end(), seq);
        bool selected = it != sequences.end();
        ImGui::PushID(seq);
        if (ImGui::Selectable(seq->glob.c_str(), selected)) {
            if (!selected) {
                sequences.push_back(seq);
            } else {
                sequences.erase(it);
            }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::SliderInt("Index", &index, 0, sequences.size()-1);
    ImGui::SameLine(); ImGui::ShowHelpMarker("Choose which sequence to display in the window (space / backspace)");
    if (sequences.size() > 0)
        this->index = (this->index + sequences.size()) % sequences.size();
}

void Window::postRender()
{
    if (!screenshot) return;

    ImVec2 winSize = ImGui::GetIO().DisplaySize;
    size_t x = contentRect.Min.x;
    size_t y = winSize.y - contentRect.Max.y;
    size_t w = contentRect.Max.x - contentRect.Min.x;
    size_t h = contentRect.Max.y - contentRect.Min.y;
    x *= ImGui::GetIO().DisplayFramebufferScale.x;
    w *= ImGui::GetIO().DisplayFramebufferScale.x;
    y *= ImGui::GetIO().DisplayFramebufferScale.y;
    h *= ImGui::GetIO().DisplayFramebufferScale.y;
    size_t size = 3 * w * h;

    float* data = new float[size];
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(x, y, w, h, GL_RGB, GL_FLOAT, data);
    for (size_t i = 0; i < size; i++)
        data[i] *= 255.f;
    for (size_t y = 0; y < h/2; y++) {
        for (size_t i = 0; i < 3*w; i++) {
            float v = data[(h - y - 1)*(3*w) + i];
            data[(h - y - 1)*(3*w) + i] = data[y*(3*w) + i];
            data[y*(3*w) + i] = v;
        }
    }

    std::string filename_fmt = config::get_string("SCREENSHOT");
    int i = 1;
    while (true) {
        char filename[512];
        snprintf(filename, sizeof(filename), filename_fmt.c_str(), i);
        if (!file_exists(filename)) {
            iio_write_image_float_vec(filename, data, w, h, 3);
            printf("Screenshot saved to '%s'.\n", filename);
            break;
        }
        i++;
    }
    delete[] data;
}

Sequence* Window::getCurrentSequence() const
{
    if (sequences.empty())
        return nullptr;
    return sequences[index];
}

std::string Window::getTitle() const
{
    const Sequence* seq = getCurrentSequence();
    if (!seq)
        return "(no sequence associated)";
    return seq->getTitle();
}

ImRect getRenderingRect(ImVec2 texSize, ImRect* windowRect)
{
    ImVec2 pos = ImGui::GetWindowPos() + ImGui::GetCursorPos();
    ImVec2 pos2 = pos + ImGui::GetContentRegionAvail();
    if (windowRect) {
        *windowRect = ImRect(pos, pos2);
    }

    ImVec2 diff = pos2 - pos;
    float aspect = (float) texSize.x / texSize.y;
    float nw = std::max(diff.x, diff.y * aspect);
    float nh = std::max(diff.y, diff.x / aspect);
    ImVec2 offset = ImVec2(nw - diff.x, nh - diff.y);

    pos -= offset / 2;
    pos2 += offset / 2;
    return ImRect(pos, pos2);
}

ImRect getClipRect()
{
    ImVec2 pos = ImGui::GetWindowPos() + ImGui::GetCursorPos();
    ImVec2 pos2 = pos + ImGui::GetContentRegionAvail();
    return ImRect(pos, pos2);
}

