#include "main.h"
#include <resource/Sampler.h>
#include <system/System.h>
#include <system/Context.h>
#include <data/Array.h>
#include <file/File.h>
#include <text/Label.h>
#include <effect/Blend.h>
#include <sound/Sound.h>
#include <text/TextBox.h>
#include <resource/ShapeVertex.h>
#include <system/Input.h>
#include <math/Rect.h>
#include <physics/Collision.h>
#include <component/Button.h>
#include <physics/RectHitTest.h>
#include <decoder/WAVDecoder.h>
#include <object/Shape.h>
#include <object/ShapeInstance.h>

#include <object/LabelImage.h>
#include <component/LabelButton.h>
#include <component/LabelToggleButton.h>
#include "List.h"
#include <component/EditBox.h>
#include <component/Scrollbar.h>

#include <decoder/PNGDecoder.h>

using namespace std;

static random_device rd;
mt19937_64 random(rd());

static void* fontData;
static void* boldFontData;

static File file;

List<>* list;

//VisualTextBox* visualTextBox;
EditBox* editBox;


Scrollbar* nodeScrollbar;

void LoadPNGImage(const char* _path, Frame* _outFrame, bool _noUseMipmap = false) {
	PNGDecoder decoder;

	File file(_path);
	unsigned size = file.GetSize();
	unsigned char* data = new unsigned char[size];

	file.ReadBytes(size, data);


	decoder.LoadHeader(data, size);
	unsigned char* outData = new unsigned char[decoder.GetOutputSize()];
	decoder.Decode(outData);

	if (_noUseMipmap) {
		_outFrame->Build(outData, decoder.GetWidth(), decoder.GetHeight(), FrameFormat::RGBA, 1);
	} else {
#ifdef __ANDROID__
		_outFrame->Build(outData, decoder.GetWidth(), decoder.GetHeight(), FrameFormat::RGBA, 3);
#else
		_outFrame->Build(outData, decoder.GetWidth(), decoder.GetHeight(), FrameFormat::RGBA, 0);
#endif
	}

	delete[]data;
	delete[]outData;
}
Frame* LoadPNGImageGetFrame(const char* _path, bool _noUseMipmap = false) {
	Frame* outFrame = new Frame;
	LoadPNGImage(_path, outFrame, _noUseMipmap);
	return outFrame;
}


struct Node {
	List<>* list;
	EditBox* editBox;
	LabelImage* labelImage;
	LabelButton* AddButton;
	LabelButton* DeleteButton;
};

vector<Node> nodes;
LabelImage* nodeCountImage;
LabelButton* nodeIncreaseButton;
LabelButton* nodeDecreaseButton;
LabelButton* startButton;

LabelButton* nextNodeButton;
LabelButton* prevNodeButton;

LabelImage* listCountLabelImage;
LabelButton* changeListCountButton;
EditBox* listCountEditBox;

Frame* editBox_Box;
Frame* editBox_Cursor;

bool playing = false;

unsigned listCount = 3;

void MakeList();
Image* vectorImage;
wchar_t texts[1000][3];

bool AddButtonUp(Button* _target, PointF _mousePos, void* _data) {
	if (!playing) {
		int i = (int)_data;
		if (nodes[i].editBox->text.size() > 0) {
			auto label = new SizeLabel;
			label->sizes = new FontSize[1];
			label->colors = fontColor;
			label->baseSizes = new unsigned[1];
			label->fonts = fontContainer;
			label->sizeLen = 1;

			label->sizes[0].pixelSize = 15;
			label->sizes[0].len = 0;

			label->baseSizes[0] = 15;

			auto text = new wchar_t[nodes[i].editBox->text.size() + 1];
			wcscpy_s(text, nodes[i].editBox->text.size() + 1, nodes[i].editBox->text.c_str());
			label->text = text;
			label->SizePrepareDraw(WindowRatio());
			if (nodes[i].list->nodes.Size() >= nodes[i].list->nodes.MaxSize()) {
				nodes[i].list->nodes.ReAlloc(nodes[i].list->nodes.MaxSize() + 1000);
			}
			nodes[i].list->nodes.EmplaceLast(DefaultNode(label));
			nodes[i].list->UpdateNodes();
		}
	}
	return true;
}
bool DeleteButtonUp(Button* _target, PointF _mousePos, void* _data) {
	if (!playing) {
		int i = (int)_data;
		if (nodes[i].list->GetSelectIndex() != UINT_MAX) {
			delete []nodes[i].list->nodes[nodes[i].list->GetSelectIndex()].label->text;
			delete[]nodes[i].list->nodes[nodes[i].list->GetSelectIndex()].label->sizes;
			delete[]nodes[i].list->nodes[nodes[i].list->GetSelectIndex()].label->baseSizes;
			delete nodes[i].list->nodes[nodes[i].list->GetSelectIndex()].label;
			nodes[i].list->nodes.EraseIndex(nodes[i].list->GetSelectIndex());
			nodes[i].list->UpdateNodes();
		}
	}
	return true;
}

float screenWidth;

void NodesControlling(Scrollbar* _target, bool _leftOrRight) {
	float minX = -screenWidth / 2.f + 150.f;
	float total = (float)(nodes.size()) * 300.f + (float)(nodes.size() - 1) * 20.f;
	if (nodeScrollbar->GetContentRatio() >= 1.f) {
		for (unsigned i = 0; i < nodes.size(); i++) {
			float x = -0.5f * total + 150.f + i * 320.f;
			nodes[i].list->SetPos(PointF(x, 70.f));
			nodes[i].AddButton->SetPos(PointF(x, -210.f));
			nodes[i].DeleteButton->SetPos(PointF(x, -250.f));
			nodes[i].editBox->SetPos(PointF(x, -170.f));
			nodes[i].labelImage ->SetPos(PointF(x, -290.f));
		}
	} else {
		for (unsigned i = 0; i < nodes.size(); i++) {
			float x = minX - nodeScrollbar->GetValue() * (1.f - nodeScrollbar->GetContentRatio()) * total + i * 320.f;
			nodes[i].list->SetPos(PointF(x, 70.f));
			nodes[i].AddButton->SetPos(PointF(x, -210.f));
			nodes[i].DeleteButton->SetPos(PointF(x, -250.f));
			nodes[i].editBox->SetPos(PointF(x, -170.f));
			nodes[i].labelImage->SetPos(PointF(x, -290.f));
		}
	}
}

bool ChangeButtonUp(Button* _target, PointF _mousePos, void* _data) {
	if (!playing) {
		wstringstream ss;
		ss << listCountEditBox->text;
		int i;
		ss >> i;
		if (i <= 0 || ss.fail())return false;
		listCount = i;
		MakeList();
		return true;
	}
	return false;
}

unsigned playCount = 0;
unsigned maxPlayCount;
unsigned nodeCount = 0;
int textIndex = -1;
float speed = 0.5f;
float speedT = 0.f;

bool StartButtonUp(Button* _target, PointF _mousePos, void* _data) {
	if (playing)return true;
	playing = true;
	playCount = 0;
	nodeCount = 0;
	speed = 0.5f;
	speedT = 0.f;
	textIndex = -1;
	maxPlayCount = 30 + random() % 10;
	for (int i = 0; i < nodes.size(); i++) {
		if (wcscmp(nodes[i].labelImage->GetLabel()->text, L"\"\"") != 0) {
			nodes[i].labelImage->GetLabel()->text = L"\"\"";
			nodes[i].labelImage->GetLabel()->SizePrepareDraw(WindowRatio());

			nodes[i].labelImage->Size();
		}
	}
	return true;
}

Sound* sound1, * sound2;
SoundSource source;
SoundSource source2;

SizeLabel* addNodeLabel;
SizeLabel* deleteNodeLabel;


void MakeList() {
	//1280x720일때 간격 320
	int osize = listCount - nodes.size();
	if (osize < 0) {
		osize *= -1;
		for (int i = 0; i < osize; i++) {
			Node& node = nodes[nodes.size() - 1];
			
			for (auto& i : node.list->nodes) {
				delete []i.label->sizes;
				delete []i.label->baseSizes;
				delete []i.label->text;
				delete i.label;
			}

			node.list->nodes.Free();
			node.list->FreeNodes();
			delete node.list;

			delete node.AddButton;
			delete node.DeleteButton;

			delete node.editBox;

			delete[] node.labelImage->GetLabel()->sizes;
			delete[] node.labelImage->GetLabel()->baseSizes;
			delete node.labelImage->GetLabel();
			delete node.labelImage;

			nodes.pop_back();
		}
		nodeScrollbar->SetContentRatio(screenWidth / (300.f * nodes.size() + 20.f * (nodes.size() - 1)));
		nodeScrollbar->SetValue(0.5f);
		return;
	}
	unsigned index = nodes.size();
	for (int i = 0; i < osize; i++) {
		Node node;
		node.list = new List<>(PointF(300.f, 400.f), 20.f, PointF(0.f,0.f));

		node.list->nodes.Alloc(1000);

		node.list->BuildNodes(1000);

		ScaleImage* box = new ScaleImage(PointF(0.f, 0.f), PointF(1.f, 1.f), 0.f, CenterPointPos::Center, false, nullptr, System::defaultSampler, editBox_Box, System::defaultUV, System::defaultIndex);
		ScaleImage* cursor = new ScaleImage(PointF(0.f, 0.f), PointF(1.f, 1.f), 0.f, CenterPointPos::Center, false, nullptr, System::defaultSampler, editBox_Cursor, System::defaultUV, System::defaultIndex);

		node.editBox = new EditBox(fontColor, fontContainer, box, cursor, PointF(-320.f + i * 320.f, -170.f), 300, 20);

		node.AddButton = new LabelButton(addNodeLabel);
		node.DeleteButton = new LabelButton(deleteNodeLabel);

		node.AddButton->data = (void*)(index + i);
		node.DeleteButton->data = (void*)(index + i);

		node.AddButton->buttonUp = AddButtonUp;
		node.DeleteButton->buttonUp = DeleteButtonUp;

		auto label = new SizeLabel;
		label->sizes = new FontSize[1];
		label->colors = fontColor;
		label->baseSizes = new unsigned[1];
		label->fonts = fontContainer;
		label->sizeLen = 1;

		label->sizes[0].pixelSize = 20;
		label->sizes[0].len = 0;

		label->baseSizes[0] = 20;

		label->text = L"\"\"";
		label->SizePrepareDraw(WindowRatio());

		node.labelImage = new LabelImage(label);

		nodes.push_back(node);
	}
	nodeScrollbar->SetContentRatio(screenWidth / (300.f * nodes.size() + 20.f * (nodes.size() - 1)));
	nodeScrollbar->SetValue(0.5f);
}
static void Init() {
	System::DragFileOn();

	originalWindowWidth = 1280;
	originalWindowHeight = 720;

	if ((float)System::GetWindowWidth() / (float)System::GetWindowHeight() > 16.f / 9.f) {
		screenWidth = originalWindowWidth * (((float)System::GetWindowWidth() / (float)System::GetWindowHeight()) / (originalWindowWidth / originalWindowHeight));
	} else {
		screenWidth = originalWindowWidth;
	}

	System::SetClearColor(1.f, 1.f, 1.f, 1.f);

	lineVertex = new Vertex;
	lineVertex->vertices.Alloc(2);

	InitWindowRatio();

	//"DungGeunMo.otf"//16배수 크기 픽셀 폰트
	File file("GmarketSansTTFMedium.ttf");
	unsigned fileSize = file.GetSize();
	fontData = new unsigned char[fileSize];
	file.ReadBytes(fileSize, fontData);
	font = new Font(fontData, fileSize, 0);
	file.Close();

	//file.Open("Spoqa Han Sans Bold.ttf");
	//fileSize = file.GetSize();
	//boldFontData = new unsigned char[fileSize];
	//file.ReadBytes(fileSize, boldFontData);
	//boldFont = new Font(boldFontData, fileSize, 0);
	//file.Close();

	//bgSound = new Sound;

	fontContainer[0].len = 0;
	fontContainer[0].font = font;

	sound1 = new Sound;
	sound2 = new Sound;

	file.Open("sound1.wav");

	fileSize = file.GetSize();
	void* sound1Data = new unsigned char[fileSize];
	file.ReadBytes(fileSize, sound1Data);

	WAVDecoder decoder1;

	auto data = decoder1.LoadHeaderAndDecode(sound1Data, fileSize);
	source.rawData = data;
	source.size = decoder1.GetOutputSize();
	sound1->Decode(&source);

	file.Close();

	file.Open("sound2.wav");

	fileSize = file.GetSize();
	void* sound2Data = new unsigned char[fileSize];
	file.ReadBytes(fileSize, sound2Data);

	WAVDecoder decoder2;

	data = decoder2.LoadHeaderAndDecode(sound2Data, fileSize);
	source2.rawData = data;
	source2.size = decoder2.GetOutputSize();
	sound2->Decode(&source2);

	file.Close();

	auto nodeCountImageLabel = new SizeLabel;
	nodeCountImageLabel->sizes = new FontSize[1];
	nodeCountImageLabel->colors = new FontColor[1];
	nodeCountImageLabel->baseSizes = new unsigned[1];
	nodeCountImageLabel->fonts = new FontContainer[1];
	nodeCountImageLabel->sizeLen = 1;

	nodeCountImageLabel->sizes[0].pixelSize = 25;
	nodeCountImageLabel->sizes[0].len = 0;

	nodeCountImageLabel->colors[0].color = 0;
	nodeCountImageLabel->colors[0].len = 0;

	nodeCountImageLabel->baseSizes[0] = 25;

	nodeCountImageLabel->fonts[0].font = font;
	nodeCountImageLabel->fonts[0].len = 0;

	nodeCountImageLabel->text = L"3";
	nodeCountImageLabel->SizePrepareDraw(WindowRatio());

	nodeCountImage = new LabelImage(nodeCountImageLabel, PointF(0.f, 300.f));

	auto nodeIncreaseImageLabel = new SizeLabel;
	nodeIncreaseImageLabel->sizes = new FontSize[1];
	nodeIncreaseImageLabel->colors = new FontColor[1];
	nodeIncreaseImageLabel->baseSizes = new unsigned[1];
	nodeIncreaseImageLabel->fonts = new FontContainer[1];
	nodeIncreaseImageLabel->sizeLen = 1;

	nodeIncreaseImageLabel->sizes[0].pixelSize = 20;
	nodeIncreaseImageLabel->sizes[0].len = 0;

	nodeIncreaseImageLabel->colors[0].color = 0x444444;
	nodeIncreaseImageLabel->colors[0].len = 0;

	nodeIncreaseImageLabel->baseSizes[0] = 20;

	nodeIncreaseImageLabel->fonts[0].font = font;
	nodeIncreaseImageLabel->fonts[0].len = 0;

	nodeIncreaseImageLabel->text = L"증가";
	nodeIncreaseImageLabel->SizePrepareDraw(WindowRatio());

	nodeIncreaseButton = new LabelButton(nodeIncreaseImageLabel, PointF(-50.f, 300.f));

	auto nodeDecreaseImageLabel = new SizeLabel;
	nodeDecreaseImageLabel->sizes = new FontSize[1];
	nodeDecreaseImageLabel->colors = new FontColor[1];
	nodeDecreaseImageLabel->baseSizes = new unsigned[1];
	nodeDecreaseImageLabel->fonts = new FontContainer[1];
	nodeDecreaseImageLabel->sizeLen = 1;

	nodeDecreaseImageLabel->sizes[0].pixelSize = 20;
	nodeDecreaseImageLabel->sizes[0].len = 0;

	nodeDecreaseImageLabel->colors[0].color = 0x444444;
	nodeDecreaseImageLabel->colors[0].len = 0;

	nodeDecreaseImageLabel->baseSizes[0] = 20;

	nodeDecreaseImageLabel->fonts[0].font = font;
	nodeDecreaseImageLabel->fonts[0].len = 0;

	nodeDecreaseImageLabel->text = L"감소";
	nodeDecreaseImageLabel->SizePrepareDraw(WindowRatio());

	nodeDecreaseButton = new LabelButton(nodeDecreaseImageLabel, PointF(50.f, 300.f));

	FontColor* colors = new FontColor[1];
	colors[0].color = 0;
	colors[0].len = 0;
	FontSize* sizes = new FontSize[1];
	sizes[0].len = 0;
	sizes[0].pixelSize = 10;
	unsigned* baseSizes = new unsigned[1];
	baseSizes[0] = 10;
	FontContainer* container = new FontContainer[1];
	container[0].font = font;
	container[0].len = 0;

	addNodeLabel = new SizeLabel;
	addNodeLabel->sizes = new FontSize[1];
	addNodeLabel->colors = new FontColor[1];
	addNodeLabel->baseSizes = new unsigned[1];
	addNodeLabel->fonts = new FontContainer[1];
	addNodeLabel->sizeLen = 1;

	addNodeLabel->sizes[0].pixelSize = 20;
	addNodeLabel->sizes[0].len = 0;

	addNodeLabel->colors[0].color = 0x00ff00;
	addNodeLabel->colors[0].len = 0;

	addNodeLabel->baseSizes[0] = 20;

	addNodeLabel->fonts[0].font = font;
	addNodeLabel->fonts[0].len = 0;


	addNodeLabel->text = L"항목 추가";
	addNodeLabel->SizePrepareDraw(WindowRatio());

	deleteNodeLabel = new SizeLabel;
	deleteNodeLabel->sizes = new FontSize[1];
	deleteNodeLabel->colors = new FontColor[1];
	deleteNodeLabel->baseSizes = new unsigned[1];
	deleteNodeLabel->fonts = new FontContainer[1];
	deleteNodeLabel->sizeLen = 1;

	deleteNodeLabel->sizes[0].pixelSize = 20;
	deleteNodeLabel->sizes[0].len = 0;

	deleteNodeLabel->colors[0].color = 0xff0000;
	deleteNodeLabel->colors[0].len = 0;

	deleteNodeLabel->baseSizes[0] = 20;

	deleteNodeLabel->fonts[0].font = font;
	deleteNodeLabel->fonts[0].len = 0;

	deleteNodeLabel->text = L"항목 삭제";
	deleteNodeLabel->SizePrepareDraw(WindowRatio());

	editBox_Box = LoadPNGImageGetFrame("EditBox_Box.png", true);
	editBox_Cursor = LoadPNGImageGetFrame("EditBox_Cursor.png", true);

	ScaleImage* bar = new ScaleImage(PointF(0.f, 0.f), PointF(1.f, 1.f), 0.f, CenterPointPos::Center, false, nullptr, System::defaultSampler, LoadPNGImageGetFrame("Bar.png", true), System::defaultUV, System::defaultIndex);
	ScaleImage* stick = new ScaleImage(PointF(0.f, 0.f), PointF(1.f, 1.f), 0.f, CenterPointPos::Center, false, nullptr, System::defaultSampler, LoadPNGImageGetFrame("Stick.png", true), System::defaultUV, System::defaultIndex);
	nodeScrollbar = new Scrollbar(false, bar, stick, PointF(0.f, -320.f), 1.f, 0.5f, RectF(-640.f, 640.f, -130.f, -312.5f), PointF(0.f,0.f),PointF(0.f,0.f),0.1f);
	nodeScrollbar->controlling = NodesControlling;

	MakeList();
	
	auto startLabel = new SizeLabel;
	startLabel->sizes = new FontSize[1];
	startLabel->colors = new FontColor[1];
	startLabel->baseSizes = new unsigned[1];
	startLabel->fonts = new FontContainer[1];
	startLabel->sizeLen = 1;

	startLabel->sizes[0].pixelSize = 30;
	startLabel->sizes[0].len = 0;

	startLabel->colors[0].color = 0;
	startLabel->colors[0].len = 0;

	startLabel->baseSizes[0] = 30;

	startLabel->fonts[0].font = font;
	startLabel->fonts[0].len = 0;

	startLabel->text = L"시작!";
	startLabel->SizePrepareDraw(WindowRatio());
	startButton = new LabelButton(startLabel, PointF(560.f, 310.f));

	startButton->buttonUp = StartButtonUp;

	/*unsigned a = 0;
	
	for (unsigned i = 0; i < 100; i++) {
		texts[i][0] = L'안';
		texts[i][1] = L'0'+i;
		texts[i][2] = L'\0';
	}
	for (auto& i : list->nodes) {
		i.label->colors = colors;
		i.label->sizeLen = 1;
		i.label->baseSizes = baseSizes;
		i.label->sizes = sizes;
		i.label->fonts = container;
		i.label->renders = fontRender;
		i.label->text = texts[a];
		a++;
	}*/
	
	
	
	//------------------------------------------------------------------------------------------------
	//editBox = new EditBox(PointF(0.f,0.f),PointF(1.f,1.f),200, 20);

	//------------------------------------------------------------------------------------------------

	SizeLabel* listCountLabel = new SizeLabel;
	FontSize* listCountFontSize = new FontSize[1];
	unsigned* listCountBaseSize = new unsigned[1];
	SizeLabel::MakeSizeLabelText(L"리스트 갯수", listCountFontSize, listCountBaseSize, listCountLabel, 30);

	listCountLabelImage = new LabelImage(listCountLabel, PointF(-500.f, 330.f));

	ScaleImage* listCountBox = new ScaleImage(PointF(0.f, 0.f), PointF(1.f, 1.f), 0.f, CenterPointPos::Center, false, nullptr, System::defaultSampler, editBox_Box, System::defaultUV, System::defaultIndex);
	ScaleImage* listCountCursor = new ScaleImage(PointF(0.f, 0.f), PointF(1.f, 1.f), 0.f, CenterPointPos::Center, false, nullptr, System::defaultSampler, editBox_Cursor, System::defaultUV, System::defaultIndex);
	listCountEditBox = new EditBox(fontColor, fontContainer, listCountBox, listCountCursor, PointF(-350.f, 330.f), 100, 30);

	SizeLabel* changeListCountLabel = new SizeLabel;
	FontSize* changeListCountFontSize = new FontSize[1];
	unsigned* changeListCountBaseSize = new unsigned[1];
	SizeLabel::MakeSizeLabelText(L"변경", changeListCountFontSize, changeListCountBaseSize, changeListCountLabel, 30);
	changeListCountButton = new LabelButton(changeListCountLabel, PointF(-250.f, 330.f));
	changeListCountButton->buttonUp = ChangeButtonUp;

}

static void DragDrop() {
	if (!playing) {

		PointF pos = System::GetDragFilePoint();

		unsigned len = System::GetDragFileCount();

		if (len >= 1) {
			char name[255];
			System::GetDragFile(0, name);

			for (int i = 0; i < nodes.size(); i++) {
				if (nodes[i].list->GetRect().IsPointIn(pos)) {
					ifstream ifs;
					ifs.open(name);
					while (true) {
						string str;
						getline(ifs, str);
						if (ifs.eof())break;
						auto label = new SizeLabel;
						label->sizes = new FontSize[1];
						label->colors = new FontColor[1];
						label->baseSizes = new unsigned[1];
						label->fonts = new FontContainer[1];
						label->sizeLen = 1;

						label->sizes[0].pixelSize = 15;
						label->sizes[0].len = 0;

						label->colors[0].color = 0;
						label->colors[0].len = 0;

						label->baseSizes[0] = 15;

						label->fonts[0].font = font;
						label->fonts[0].len = 0;

						
						int tlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), NULL, NULL);
						auto text = new wchar_t[tlen + 1];
						MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), text, tlen);
						text[tlen] = 0;
						label->text = text;
						label->SizePrepareDraw(WindowRatio());
						if (nodes[i].list->nodes.Size() >= nodes[i].list->nodes.MaxSize()) {
							nodes[i].list->nodes.ReAlloc(nodes[i].list->nodes.MaxSize() + 1000);
						}
						nodes[i].list->nodes.EmplaceLast(DefaultNode(label));
						nodes[i].list->UpdateNodes();
					}
					break;
				}
			}
		}
	}
}

static void Update() {
	bool requireDraw = sizeRequest;

	if (playing) {
		while (speedT >= speed) {
			speedT -= speed;
			if (playCount >= maxPlayCount) {
				speed = 0.5f;
				speedT = 0.f;
				nodeCount++;
				playCount = 0;
				maxPlayCount = 30 + random() % 10;
				sound2->SetPos(0);
				sound2->Play(1);
			} else {
				sound1->SetPos(0);
				sound1->Play(1);
			}
			while (nodeCount < nodes.size()) {
				if (nodes[nodeCount].list->nodes.Size() == 0) {
					nodeCount++;
				} else if (nodes[nodeCount].list->nodes.Size() == 1) {
					nodes[nodeCount].labelImage->GetLabel()->text = nodes[nodeCount].list->nodes[0].label->text;
					nodes[nodeCount].labelImage->GetLabel()->SizePrepareDraw(WindowRatio());

					nodes[nodeCount].labelImage->Size();

					nodeCount++;
				} else break;
			}
			if (nodeCount == nodes.size()) {
				playing = false;
				goto end;
			}
			unsigned len = nodes[nodeCount].list->nodes.Size();
			unsigned index;
			if (textIndex == -1) {
				index = random() % len;
			} else {
				while (true) {
					index = random() % len;
					if (textIndex != index)break;
				}
			}
			textIndex = index;
			nodes[nodeCount].labelImage->GetLabel()->text = nodes[nodeCount].list->nodes[textIndex].label->text;
			nodes[nodeCount].labelImage->GetLabel()->SizePrepareDraw(WindowRatio());

			nodes[nodeCount].labelImage->Size();

			playCount++;
			speed *= 0.9f;
		}
		speedT += System::GetDeltaTime();
	}
end:;
	

	startButton->Update();
	for (int i = 0; i < nodes.size(); i++) {
		nodes[i].list->Update();
		nodes[i].editBox->Update();
		nodes[i].AddButton->Update();
		nodes[i].DeleteButton->Update();
	}
	listCountEditBox->Update();
	changeListCountButton->Update();
	nodeScrollbar->Update();

	//if(requireDraw && !System::IsPause()) {
	sizeRequest = false;

	System::Clear(true);

	//nodeCountImage->Draw();
	startButton->Draw();
	for (int i = 0; i < nodes.size(); i++) {
		if (System::GetScreenRect().IsRectIn(nodes[i].list->GetRect())) {
			nodes[i].list->Draw();
			nodes[i].editBox->Draw();
			nodes[i].AddButton->Draw();
			nodes[i].DeleteButton->Draw();
			nodes[i].labelImage->Draw();
		}
	}
	listCountLabelImage->Draw();
	listCountEditBox->Draw();
	changeListCountButton->Draw();
	nodeScrollbar->Draw();

	System::Render();
}
static void Activate() {

}
static void Destroy() {
	delete font;
	delete boldFont;


	Font::ThreadRelease();
	Font::Release();

	delete[]fontData;
	delete[]boldFontData;

	Sound::Release();
}
void Main_Size() {
	if (!nodeCountImage) return;

	LabelImage::LABELIMAGE_SIZE(nodeCountImage);
	LabelButton::LABELBUTTON_SIZE(startButton);
	for (int i = 0; i < nodes.size(); i++) {
		nodes[i].list->Size();
		nodes[i].editBox->Size();
		LabelButton::LABELBUTTON_SIZE(nodes[i].AddButton);
		LabelButton::LABELBUTTON_SIZE(nodes[i].DeleteButton);
		LabelImage::LABELIMAGE_SIZE(nodes[i].labelImage);
	}
	LabelImage::LABELIMAGE_SIZE(listCountLabelImage);
	LabelButton::LABELBUTTON_SIZE(changeListCountButton);
	listCountEditBox->Size();
	nodeScrollbar->Size();

	if ((float)System::GetWindowWidth() / (float)System::GetWindowHeight() > 16.f / 9.f) {
		screenWidth = originalWindowWidth * (((float)System::GetWindowWidth() / (float)System::GetWindowHeight()) / (originalWindowWidth / originalWindowHeight));
	} else {
		screenWidth = originalWindowWidth;
	}

	nodeScrollbar->SetContentRatio(screenWidth / (300.f * nodes.size() + 20.f * (nodes.size() - 1)));
	if(nodeScrollbar->GetContentRatio() > 1.f)nodeScrollbar->SetValue(0.5f);
	else nodeScrollbar->SetValue(nodeScrollbar->GetValue());

	sizeRequest = true;
}

static void Create() {
	System::CreateInfo createInfo;
	createInfo.screenMode = System::ScreenMode::Window;
	createInfo.windowSize.width = 1280;
	createInfo.windowSize.height = 720;
	createInfo.windowShow = System::WindowShow::Default;

#ifdef _WIN32
	createInfo.title = _T("WordCombination v0.1");
	createInfo.windowPos.x = System::WindowDefaultPos;
	createInfo.windowPos.y = System::WindowDefaultPos;
	createInfo.cursorResource = nullptr;
	createInfo.iconResource = nullptr;
	createInfo.maximized = true;
	createInfo.minimized = true;
	createInfo.resizeWindow = true;
#endif

	createInfo.refleshRateTop = 0;
	createInfo.refleshRateBottom = 1;

	createInfo.msaaCount = 1;
	createInfo.msaaQuality = 0;
	createInfo.vSync = true;
	createInfo.maxFrame = 0;
	createInfo.screenIndex = 0;

	System::updateFuncs = Update;
	System::activateFunc = Activate;
	System::destroyFunc = Destroy;
	System::sizeFunc = Main_Size;
	System::dragDropFuncs = DragDrop;

	System::Init(&createInfo);

	Font::Init(30000, 30000);
	Font::ThreadInit(1000, 10000, 1920 * 1080, 1920 * 1080);

	Sound::Init(100, 44100);

	System::SetClearColor(1.f, 1.f, 1.f, 1.f);

	Init();
}

#ifdef _WIN32
int APIENTRY _tWinMain(HINSTANCE _hInstance, HINSTANCE, LPTSTR, int) {
	System::createFunc = Create;

	System::Create(_hInstance);

	return 0;
}
#elif __ANDROID__
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
	System::createFunc = Create;

	System::Create(activity);
}
#endif