#pragma once

#include <system/System.h>
#include <sound/Sound.h>
#include <object/CenterPointPos.h>
#include <text/Font.h>

#include <fstream>

extern std::mt19937_64 random;


constexpr unsigned uiButtonPx = 40;
constexpr unsigned windowWidth = 1280;
constexpr unsigned windowHeight = 720;
constexpr float windowWidthF = 1280.f;
constexpr float windowHeightF = 720.f;

inline Font* font;
inline Font* boldFont;

struct FontRender;
inline bool sizeRequest = true;

void Main_Size(void* _data);

class Vertex;

inline Vertex* lineVertex;