#include <cstdlib>
#include <cstring>
#include <glob.h>
#include <algorithm>
#include <limits>

#include <SFML/OpenGL.hpp>

#include "Sequence.hpp"
#include "Player.hpp"
#include "View.hpp"
#include "Colormap.hpp"
#include "Image.hpp"
#include "alphanum.hpp"
#include "globals.hpp"

#include "plambda.h"
#include "gmic/gmic.h"

const char* getGLError(GLenum error)
{
#define casereturn(x) case x: return #x
    switch (error) {
        casereturn(GL_INVALID_ENUM);
        casereturn(GL_INVALID_VALUE);
        casereturn(GL_INVALID_OPERATION);
        casereturn(GL_INVALID_FRAMEBUFFER_OPERATION);
        casereturn(GL_OUT_OF_MEMORY);
        default:
        case GL_NO_ERROR:
        return "";
    }
#undef casereturn
}

#define GLDEBUG(x) \
    x; \
{ \
    GLenum e; \
    while((e = glGetError()) != GL_NO_ERROR) \
    { \
        printf("%s:%s:%d for call %s", getGLError(e), __FILE__, __LINE__, #x); \
    } \
}

Sequence::Sequence()
{
    static int id = 0;
    id++;
    ID = "Sequence " + std::to_string(id);

    view = nullptr;
    player = nullptr;
    colormap = nullptr;
    image = nullptr;

    valid = false;
    force_reupload = false;

    loadedFrame = -1;
    loadedRect = ImRect();

    glob.reserve(4096);
    glob_.reserve(4096);
    glob = "";
    glob_ = "";

    editprog[0] = 0;
}

void Sequence::loadFilenames() {
    glob_t res;
    ::glob(glob.c_str(), GLOB_TILDE | GLOB_NOSORT, NULL, &res);
    filenames.resize(res.gl_pathc);
    for(unsigned int j = 0; j < res.gl_pathc; j++) {
        filenames[j] = res.gl_pathv[j];
    }
    globfree(&res);
    std::sort(filenames.begin(), filenames.end(), doj::alphanum_less<std::string>());

    if (filenames.empty() && !strcmp(glob.c_str(), "-")) {
        filenames.push_back("-");
    }

    valid = filenames.size() > 0;
    strcpy(&glob_[0], &glob[0]);

    loadedFrame = -1;
    if (player)
        player->reconfigureBounds();
}

void Sequence::loadTextureIfNeeded()
{
    if (valid && player) {
        if (loadedFrame != player->frame || force_reupload) {
            forgetImage();
        }
    }
}

void Sequence::forgetImage()
{
    loadedRect = ImRect();
    if (image && !image->is_cached) {
        delete image;
    }
    image = nullptr;
    force_reupload = true;
}

void Sequence::requestTextureArea(ImRect rect)
{
    int frame = player->frame;
    assert(frame > 0);

    if (frame != loadedFrame) {
        forgetImage();
    }

    const Image* img = getCurrentImage();
    if (!img)
        return;

    int w = img->w;
    int h = img->h;

    rect.Clip(ImRect(0, 0, w, h));

    bool reupload = force_reupload;
    if (!loadedRect.ContainsInclusive(rect)) {
        loadedRect.Add(rect);
        reupload = true;

        loadedRect.Expand(128);  // to avoid multiple uploads during zoom-out
        loadedRect.Clip(ImRect(0, 0, w, h));
    }

    if (reupload) {
        unsigned int gltype;
        if (img->type == Image::UINT8)
            gltype = GL_UNSIGNED_BYTE;
        else if (img->type == Image::FLOAT)
            gltype = GL_FLOAT;
        else
            assert(0);

        unsigned int glformat;
        if (img->format == Image::R)
            glformat = GL_RED;
        else if (img->format == Image::RG)
            glformat = GL_RG;
        else if (img->format == Image::RGB)
            glformat = GL_RGB;
        else if (img->format == Image::RGBA)
            glformat = GL_RGBA;
        else
            assert(0);

        if (texture.id == -1 || texture.size.x != w || texture.size.y != h
            || texture.type != gltype || texture.format != glformat) {
            texture.create(w, h, gltype, glformat);
        }

        size_t elsize;
        if (img->type == Image::UINT8) elsize = sizeof(uint8_t);
        else if (img->type == Image::FLOAT) elsize = sizeof(float);
        else assert(0);

        auto area = loadedRect;
        const uint8_t* data = (uint8_t*) img->pixels + elsize*(w * (int)area.Min.y + (int)area.Min.x)*img->format;

        if (area.GetWidth() > 0 && area.GetHeight() > 0) {
            glBindTexture(GL_TEXTURE_2D, texture.id);
            GLDEBUG();

            glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
            GLDEBUG();
            glTexSubImage2D(GL_TEXTURE_2D, 0, area.Min.x, area.Min.y, area.GetWidth(), area.GetHeight(),
                            glformat, gltype, data);
            GLDEBUG();
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            GLDEBUG();
        }

        loadedFrame = frame;
        force_reupload = false;
        printf("uploaded\n");
    }
}

void Sequence::autoScaleAndBias()
{
    colormap->center = .5f;
    colormap->radius = .5f;

    const Image* img = getCurrentImage();
    if (!img)
        return;

    colormap->autoCenterAndRadius(img->min, img->max);
}

void Sequence::smartAutoScaleAndBias(ImVec2 p1, ImVec2 p2)
{
    colormap->center = .5f;
    colormap->radius = .5f;

    const Image* img = getCurrentImage();
    if (!img)
        return;

    if (p1.x < 0) p1.x = 0;
    if (p1.y < 0) p1.y = 0;
    if (p2.x >= img->w-1) p2.x = img->w - 1;
    if (p2.y >= img->h-1) p2.y = img->h - 1;

    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::min();
    const float* data = (const float*) img->pixels;
    for (int y = p1.y; y < p2.y; y++) {
        for (int x = p1.x; x < p2.x; x++) {
            for (int d = 0; d < img->format; d++) {
                min = std::min(min, data[d + img->format*(x+y*img->w)]);
                max = std::max(max, data[d + img->format*(x+y*img->w)]);
            }
        }
    }

    colormap->autoCenterAndRadius(min, max);
}

Image* run_edit_program(char* prog, Sequence::EditType edittype)
{
    std::vector<Sequence*> seq;
    while (*prog && *prog != ' ') {
        char* old = prog;
        int a = strtol(prog, &prog, 10);
        if (prog == old) break;
        if (a >= gSequences.size()) return 0;
        seq.push_back(gSequences[a]);
        if (*prog == ' ') break;
        if (*prog) prog++;
    }

    int n = seq.size();
    float* x[n];
    int w[n];
    int h[n];
    int d[n];
    for (int i = 0; i < n; i++) {
        const Image* img = seq[i]->getCurrentImage(true);
        x[i] = (float*) img->pixels;
        w[i] = img->w;
        h[i] = img->h;
        d[i] = img->format;
    }

    if (edittype == Sequence::PLAMBDA) {
        int dd;

        float* pixels = execute_plambda(n, x, w, h, d, prog, &dd);
        if (!pixels)
            return 0;

        Image* img = new Image(pixels, *w, *h, (Image::Format) dd);
        return img;
    } else {
        gmic_list<char> images_names;
        gmic_list<float> images;
        images.assign(n);
        for (int i = 0; i < n; i++) {
            gmic_image<float>& img = images[i];
            img.assign(w[i], h[i], 1, d[i]);
            float* xptr = x[i];
            for (int y = 0; y < h[i]; y++) {
                for (int x = 0; x < w[i]; x++) {
                    for (int z = 0; z < d[i]; z++) {
                        img(x, y, 0, z) = *(xptr++);
                    }
                }
            }
        }

        try {
            gmic(prog, images, images_names);
        } catch (gmic_exception &e) {
            std::fprintf(stderr,"\n- Error encountered when calling G'MIC : '%s'\n", e.what());
            return 0;
        }

        gmic_image<float>& image = images[0];
        size_t size = image._width * image._height * image._spectrum;
        float* data = new float[size];
        float* ptrdata = data;
        for (int y = 0; y < image._height; y++) {
            for (int x = 0; x < image._width; x++) {
                for (int z = 0; z < image._spectrum; z++) {
                    *(ptrdata++) = image(x, y, 0, z);
                }
            }
        }
        Image* img = new Image(data, image._width, image._height, (Image::Format) image._spectrum);
        return img;
    }
}

const Image* Sequence::getCurrentImage(bool noedit) {
    if (!valid || !player) {
        return 0;
    }

    if (!image || noedit) {
        int frame = player->frame;
        const Image* img = Image::load(filenames[frame - 1]);
        image = img;

        if (!noedit && editprog[0]) {
            image = run_edit_program(editprog, edittype);
            if (!image)
                image = img;
        }
    }

    return image;
}

const std::string Sequence::getTitle() const
{
    std::string seqname = std::string(glob.c_str());
    if (!valid)
        return "(the sequence '" + seqname + "' contains no images)";
    if (!player)
        return "(no player associated with the sequence '" + seqname + "')";
    if (!colormap)
        return "(no colormap associated with the sequence '" + seqname + "')";

    std::string title;
    int id = 0;
    while (gSequences[id] != this && id < gSequences.size())
        id++;
    title += "#" + std::to_string(id) + " ";
    title += "[" + std::to_string(player->frame) + '/' + std::to_string(filenames.size()) + "]";
    title += " " + filenames[player->frame - 1];
    if (image) {
        title += " (" + std::to_string(image->w) + "x" + std::to_string(image->h) + "x" + std::to_string(image->format) + ")";
        title += " [" + std::to_string(image->min) + ".." + std::to_string(image->max) + "]";
        title += " shader:" + colormap->getShaderName();
        if (*editprog) {
            title += " (EDITED)";
        }
    } else {
        title += " cannot be loaded";
    }
    return title;
}

