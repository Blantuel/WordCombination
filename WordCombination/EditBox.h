#pragma once

#include "main.h"
#include <object/ScaleHeader.h>
#include <text/SizeLabel.h>

#include <object/Shape.h>
#include <object/Image.h>
#include <resource/ShapeVertex.h>

class EditBox {
	bool focus;
	unsigned prevCharsLen;
	int cursorPos;
	float dt;
	unsigned baseTextWidth;

	Image* textImage;
	Shape* box;
	Shape* cursor;
	ShapeVertex* shapeVertex;

	std::vector<int> textWidths;
	
	void EnterChar(bool* _changePos, bool* _requireDraw);
	void _Draw(bool* _changePos, bool* _requireDraw);
public:
	PointF baseScale;
	PointF basePos;

	PosType posType;
	std::wstring text;
	SizeLabel* label;
	EditBox(PointF _basePos, PointF _baseScale, unsigned _width, unsigned _textPixelSize);
	bool Update();
	void Draw();
	void Size();

	void SetPos(PointF _pos);

	~EditBox();
};