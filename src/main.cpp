#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <cocos2d.h>
#include <vector>
#include <chrono>
#include "fileSystem.hpp"

Mod* mod = nullptr;

float leftOver = 0.f; // For CCScheduler

double prevSpeed = 1.0f;

int fixedFps = 90;
int androidFps = 60;
int fpsIndex = 0;

#ifdef GEODE_IS_ANDROID
	int offset = 0x320;
#else
	int offset = 0x328;
#endif

bool restart = false;
bool stepFrame = false;
bool playerHolding = false;
bool lastHold = false;
bool playingAction = false;
bool shouldPlay = false;
bool shouldPlay2 = false;
bool holdV = false;
bool playedMacro = false;
bool noDelayedReset = false;

const int fpsArr[4] = {60,120,180,240};

bool areEqual(float a, float b) {
    return std::abs(a - b) < 0.1f;
}

CCLabelBMFont* frameLabel = nullptr;
CCLabelBMFont* stateLabel = nullptr;

CCMenu* buttonsMenu = nullptr;
CCMenuItemSpriteExtra* advanceFrameBtn = nullptr;
CCMenuItemSpriteExtra* disableFSBtn = nullptr;
CCMenuItemSpriteExtra* speedhackBtn = nullptr;

using namespace geode::prelude;

struct playerData {
	float xPos;
	float yPos;
	bool upsideDown;
	float rotation;
	double xSpeed;
	double ySpeed;
};

struct data {
    bool player1;
    int frame;
    int button;
    bool holding;
	bool posOnly;
	playerData p1;
	playerData p2;
};

data* androidAction = nullptr;

enum state {
    off,
    recording,
    playing
};

class recordSystem {
public:
	bool android = false;
	int fps = 0;
    state state = off;
 	size_t currentAction = 0;
   	std::vector<data> macro;

	int currentFrame() {
		int fps2 = (android) ? 90 : fps;
		return PlayLayer::get()->m_gameState.m_currentProgress * (fps2 / 90);
	}
	void recordAction(bool holding, int button, bool player1, int frame, GJBaseGameLayer* bgl, playerData p1Data, playerData p2Data) {
    	macro.push_back({player1, frame, button, holding, false, p1Data, p2Data});
	}

};

recordSystem recorder;

class RecordLayer : public geode::Popup<std::string const&> {
	CCLabelBMFont* fpsLabel = nullptr;
 	CCLabelBMFont* infoMacro = nullptr;
 	CCMenuItemToggler* recording = nullptr;
    CCMenuItemToggler* playing = nullptr;
protected:
    bool setup(std::string const& value) override {
        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
		auto versionLabel = CCLabelBMFont::create("xdBot v1.6.0 - made by Zilko", "chatFont.fnt");
		versionLabel->setOpacity(60);
		versionLabel->setAnchorPoint(ccp(0.0f,0.5f));
		versionLabel->setPosition(winSize/2 + ccp(-winSize.width/2, -winSize.height/2) + ccp(3, 6));
		versionLabel->setScale(0.5f);
		this->addChild(versionLabel);
		this->setTitle("xdBot");
		auto menu = CCMenu::create();
    	menu->setPosition({0, 0});
    	m_mainLayer->addChild(menu);

 		auto checkOffSprite = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
   		auto checkOnSprite = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");

		CCPoint topLeftCorner = winSize/2.f-ccp(m_size.width/2.f,-m_size.height/2.f);

		auto label = CCLabelBMFont::create("Record", "bigFont.fnt"); 
    	label->setAnchorPoint({0, 0.5});
    	label->setScale(0.7f);
    	label->setPosition(topLeftCorner + ccp(168, -60));
    	m_mainLayer->addChild(label);

    	recording = CCMenuItemToggler::create(checkOffSprite,
		checkOnSprite,
		this,
		menu_selector(RecordLayer::toggleRecord));

    	recording->setPosition(label->getPosition() + ccp(105,0));
    	recording->setScale(0.85f);
    	recording->toggle(recorder.state == state::recording); 
    	menu->addChild(recording);

    	auto spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
    	spr->setScale(0.8f);
    	auto btn = CCMenuItemSpriteExtra::create(
        	spr,
        	this,
        	menu_selector(RecordLayer::openSettingsMenu)
    	);
    	btn->setPosition(winSize/2.f-ccp(m_size.width/2.f,m_size.height/2.f) + ccp(355, 20));
    	menu->addChild(btn);

    	btn->setPosition(winSize/2.f-ccp(-m_size.width/2.f,m_size.height/2.f) + ccp(-16, 80));
		btn->setID("right");
    	menu->addChild(btn);

		if (!isAndroid) {
		spr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
    	spr->setScale(0.65f);
    	btn = CCMenuItemSpriteExtra::create(
        	spr,
        	this,
        	menu_selector(RecordLayer::keyInfo)
    	);
    	btn->setPosition(topLeftCorner + ccp(290, -10));
    	menu->addChild(btn);
		}

    	label = CCLabelBMFont::create("Play", "bigFont.fnt");
    	label->setScale(0.7f);
    	label->setPosition(topLeftCorner + ccp(198, -90)); 
    	label->setAnchorPoint({0, 0.5});
    	m_mainLayer->addChild(label);



     	playing = CCMenuItemToggler::create(checkOffSprite, checkOnSprite,
	 	this,
	 	menu_selector(RecordLayer::togglePlay));

    	playing->setPosition(label->getPosition() + ccp(75,0)); 
    	playing->setScale(0.85f);
    	playing->toggle(recorder.state == state::playing); 
    	menu->addChild(playing);

 		auto btnSprite = ButtonSprite::create("Save");
    	btnSprite->setScale(0.72f);

   		btn = CCMenuItemSpriteExtra::create(btnSprite,
   		this,
   		menu_selector(saveMacroPopup::openSaveMacro));

    	btn->setPosition(topLeftCorner + ccp(65, -160)); 
    	menu->addChild(btn);

		btnSprite = ButtonSprite::create("Load");
		btnSprite->setScale(0.72f);

    	btn = CCMenuItemSpriteExtra::create(btnSprite,
		this,
		menu_selector(loadMacroPopup::openLoadMenu));

    	btn->setPosition(topLeftCorner + ccp(144, -160));
    	menu->addChild(btn);

  		btnSprite = ButtonSprite::create("Clear");
		btnSprite->setScale(0.72f);

    	btn = CCMenuItemSpriteExtra::create(btnSprite,
		this,
		menu_selector(RecordLayer::clearMacro));

    	btn->setPosition(topLeftCorner + ccp(228, -160));
    	menu->addChild(btn);

		infoMacro = CCLabelBMFont::create("", "chatFont.fnt");
    	infoMacro->setAnchorPoint({0, 1});
    	infoMacro->setPosition(topLeftCorner + ccp(34, -44));
		updateInfo();
    	m_mainLayer->addChild(infoMacro);

        return true;
	}

    static RecordLayer* create() {
        auto ret = new RecordLayer();
        if (ret && ret->init(300, 200, "", "GJ_square02.png")) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

public:
 
	void openSettingsMenu(CCObject*) {
		geode::openSettingsPopup(mod);
	}

	void updateFps(CCObject* ob) {
		if (static_cast<CCLabelBMFont*>(ob)->getID() == "left")
			fpsIndex--;
		else if (static_cast<CCLabelBMFont*>(ob)->getID() == "right") {
			fpsIndex++;
		}

		if (fpsIndex == -1) fpsIndex = 3;
		else if (fpsIndex == 4) fpsIndex = 0;

		fpsLabel->setString(std::to_string(fpsArr[fpsIndex]).c_str());
		mod->setSavedValue<float>("previous_fps", fpsIndex);
	}


	void keyInfo(CCObject*) {
		FLAlertLayer::create(
    		"Shortcuts",   
    		"<cg>Toggle Speedhack</c> = <cl>C</c>\n<cg>Advance Frame</c> = <cl>V</c>\n<cg>Disable Frame Stepper</c> = <cl>B</c>",  
    		"OK"      
		)->show();
	}

	void updateInfo() {
		int clicksCount = 0;
		if (!recorder.macro.empty()) {
			for (const data& element : recorder.macro) {
        		if (element.holding && !element.posOnly) clicksCount++;
    		}
		}
		
 		std::stringstream infoText;

		infoText << "Current Macro:";
		
		infoText << "\nClicks: " << clicksCount;

		infoText << "\nDuration: " << (!recorder.macro.empty() 
		? recorder.macro.back().frame / fixedFps : 0) << "s";

		infoText << "\nFPS: " << (recorder.fps);

    	infoMacro->setString(infoText.str().c_str());
	}

	void togglePlay(CCObject*) {
		if (recorder.state == state::recording) recording->toggle(false);
    	recorder.state = (recorder.state == state::playing) ? state::off : state::playing;

		if (recorder.state == state::playing) {
			restart = true;
			mod->setSettingValue("speedhack", 1.0);
			if (mod->getSettingValue<bool>("speedhack_audio")) {
				FMOD::ChannelGroup* channel;
    			FMODAudioEngine::sharedEngine()->m_system->getMasterChannelGroup(&channel);
				channel->setPitch(1);
			}
		}
		else if (recorder.state == state::off) restart = false;
		mod->setSettingValue("frame_stepper", false);
	}

	void toggleRecord(CCObject* sender) {
			if (recorder.state == state::playing) this->playing->toggle(false);
    		recorder.state = (recorder.state == state::recording) 
			? state::off : state::recording;
			if (recorder.state == state::recording) {
				if (recorder.macro.empty()) {
					recorder.android = false;
					recorder.fps = 90;
				}
					
				restart = true;
				updateInfo();
			} else if (recorder.state == state::off) {
				restart = false;
				mod->setSettingValue("frame_stepper", false);
			}
	}

	void clearMacro(CCObject*) {
		if (recorder.macro.empty()) return;
		geode::createQuickPopup(
    	"Clear Macro",     
    	"<cr>Clear</c> the current macro?", 
    	"Cancel", "Yes",  
    	[this](auto, bool btn2) {
        	if (btn2) {
				recorder.macro.clear();
				this->updateInfo();
				if (recorder.state == state::playing) this->playing->toggle(false);
				if (recorder.state == state::recording) this->recording->toggle(false);
				recorder.state = state::off;
			}
    	});
	}

    void openMenu(CCObject*) {
		auto layer = create();
		layer->m_noElasticity = (static_cast<float>(mod->getSettingValue<double>("speedhack")) < 1
		 && recorder.state == state::recording) ? true : false;
		layer->show();
	}
};

void saveMacroPopup::openSaveMacro(CCObject*) {
	if (recorder.macro.empty()) {
		FLAlertLayer::create(
    	"Save Macro",   
    	"You can't save an <cl>empty</c> macro.",  
    	"OK"      
		)->show();
		return;
	}
	auto layer = create();
	layer->m_noElasticity = (static_cast<float>(mod->getSettingValue<double>("speedhack")) < 1
	 && recorder.state == state::recording) ? true : false;
	layer->show();
}

void saveMacroPopup::saveMacro(CCObject*) {
  if (std::string(macroNameInput->getString()).length() < 1) {
		FLAlertLayer::create(
    	"Save Macro",   
    	"Macro name can't be <cl>empty</c>.",  
    	"OK"      
		)->show();
		return;
	}

	std::string savePath = mod->getSaveDir().string()
     +slash+std::string(macroNameInput->getString()) + ".xd";

	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring wideString = converter.from_bytes(savePath);
	std::locale utf8_locale(std::locale(), new std::codecvt_utf8<wchar_t>);

	std::wifstream tempFile(wideString);

    if (tempFile.good()) {
        FLAlertLayer::create(
    	"Save Macro",   
    	"A macro with this <cl>name</c> already exists.",  
    	"OK"      
		)->show();
		tempFile.close();
		return;
    }
	tempFile.close();

    std::wofstream file(wideString);
    file.imbue(utf8_locale);

	if (file.is_open()) {

		file << recorder.fps << "\n";

		for (auto &action : recorder.macro) {
			file << action.frame << "|" << action.holding <<
			"|" << action.button << "|" << action.player1 <<
			"|" << action.posOnly << "|" << action.p1.xPos <<
			"|" << action.p1.yPos << "|" << action.p1.upsideDown <<
			"|" << action.p1.rotation << "|" << action.p1.xSpeed <<
			"|" << action.p1.ySpeed << "|" << action.p2.xPos <<
			"|" << action.p2.yPos << "|" << action.p2.upsideDown <<
			"|" << action.p2.rotation << "|" << action.p2.xSpeed <<
			"|" << action.p2.ySpeed  << "\n";
		}
		file.close();
		CCArray* children = CCDirector::sharedDirector()->getRunningScene()->getChildren();
		CCObject* child;
		CCARRAY_FOREACH(children, child) {
    		saveMacroPopup* saveLayer = dynamic_cast<saveMacroPopup*>(child);
    		if (saveLayer) {
        		saveLayer->keyBackClicked();
				break;
   			}
		}
        FLAlertLayer::create(
    	"Save Macro",   
    	"Macro saved <cg>successfully</c>.",  
    	"OK"      
		)->show();
	} else {
        FLAlertLayer::create(
    	"Save Macro",   
    	"There was an <cr>error</c> saving the macro.",  
    	"OK"      
		)->show();
    }
}

void macroCell::handleLoad(CCObject* btn) {
	std::string loadPath = mod->getSaveDir().string()
    +slash+static_cast<CCMenuItemSpriteExtra*>(btn)->getID() + ".xd";
	recorder.macro.clear();

	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring wideString = converter.from_bytes(loadPath);
	std::locale utf8_locale(std::locale(), new std::codecvt_utf8<wchar_t>);


    std::wifstream file(wideString);
    file.imbue(utf8_locale);
	std::wstring line;
	if (!file.is_open()) {
		FLAlertLayer::create(
    	"Load Macro",   
    	"An <cr>error</c> occurred while loading this macro.",  
    	"OK"      
		)->show();
		return;
	}
	bool firstIt = true;
	bool andr = false;
	while (std::getline(file, line)) {
		std::wistringstream isSS(line);

		playerData p1;
		playerData p2;

		int holding, frame, button, player1, posOnly;
		float p1xPos, p1yPos, p1rotation, p1xSpeed, p1ySpeed;
		float p2xPos, p2yPos, p2rotation, p2xSpeed, p2ySpeed;
		int p1upsideDown, p2upsideDown;

		wchar_t s;
		int count = 0;
    	for (char ch : line) {
        	if (ch == '|') {
            	count++;
        	}
    	}

		if (count > 3) {
			if (isSS >> frame >> s >> holding >> s >> button >> 
			s >> player1 >> s >> posOnly >> s >>
			p1xPos >> s >> p1yPos >> s >> p1upsideDown
		 	>> s >> p1rotation >> s >> p1xSpeed >> s >>
		 	p1ySpeed >> s >> p2xPos >> s >> p2yPos >> s >> p2upsideDown
		 	>> s >> p2rotation >> s >> p2xSpeed >> s >>
		 	p2ySpeed && s == L'|') {
				p1 = {
					(float)p1xPos,
					(float)p1yPos,
					(bool)p1upsideDown,
					(float)p1rotation,
					(double)p1xSpeed,
					(double)p1ySpeed,
				};
				p2 = {
					(float)p2xPos,
					(float)p2yPos,
					(bool)p2upsideDown,
					(float)p2rotation,
					(double)p2xSpeed,
					(double)p2ySpeed,
				};
				if ((isAndroid && !(bool)posOnly) || !isAndroid)
					recorder.macro.push_back({(bool)player1, (int)frame, (int)button, (bool)holding, (bool)posOnly, p1, p2});
			}
		} else if (count < 1) {
			std::wstring andStr;
			int fps;
			if (firstIt) {
    			if (isSS >> fps) {
					andr = true;
					recorder.android = false;
					recorder.fps = (int)fps;
    			} else {
        			isSS.clear();
        			if (isSS >> andStr && andStr == L"android") {
            			andr = true;
            			recorder.android = true;
						recorder.fps = 60;
        			}
    			}
			}
		} else {
			if (isSS >> frame >> s >> holding >> s >> button >> 
			s >> player1 && s == L'|') {
				p1.xPos = 0;
				recorder.macro.push_back({(bool)player1, (int)frame, (int)button, (bool)holding, false, p1, p2});
			}
		}
    	firstIt = false;
	}
	if (!andr) {
		recorder.android = false;
		recorder.fps = 90;
	}

	CCArray* children = CCDirector::sharedDirector()->getRunningScene()->getChildren();
	CCObject* child;
	CCARRAY_FOREACH(children, child) {
    	RecordLayer* recordLayer = dynamic_cast<RecordLayer*>(child);
    	loadMacroPopup* loadLayer = dynamic_cast<loadMacroPopup*>(child);
    	if (recordLayer) {
        	recordLayer->updateInfo();
    	} else if (loadLayer) loadLayer->keyBackClicked();
	}
	file.close();
	FLAlertLayer::create(
    "Load Macro",   
    "Macro loaded <cg>successfully</c>.",  
    "OK"      
	)->show();
}

void macroCell::loadMacro(CCObject* button) {
	if (!recorder.macro.empty()) {
		geode::createQuickPopup(
    	"Load Macro",     
    	"<cr>Overwrite</c> the current macro?", 
    	"Cancel", "Ok",  
    	[this, button](auto, bool btn2) {
        	if (btn2) this->handleLoad(button);
    	}); 
	} else handleLoad(button);
}

void clearState() {
	if (mod->getSettingValue<bool>("speedhack_audio")) {
		FMOD::ChannelGroup* channel;
    	FMODAudioEngine::sharedEngine()->m_system->getMasterChannelGroup(&channel);
		channel->setPitch(1);
	}
	
	recorder.state = state::off;

	playingAction = false;

	if (isAndroid && PlayLayer::get()) {
		if (disableFSBtn != nullptr) {
			disableFSBtn->removeFromParent();
			disableFSBtn = nullptr;
		}
		if (advanceFrameBtn != nullptr) {
			advanceFrameBtn->removeFromParent();
			advanceFrameBtn = nullptr;
		}
		if (speedhackBtn != nullptr) {
			speedhackBtn->removeFromParent();
			speedhackBtn = nullptr;
		}
		if (buttonsMenu != nullptr) {
			buttonsMenu->removeFromParent();
			buttonsMenu = nullptr;
		}
	}

	frameLabel = nullptr;
	stateLabel = nullptr;

	androidAction = nullptr;
	leftOver = 0.f;

	if (PlayLayer::get()) {
		CCArray* children = PlayLayer::get()->getChildren();
		CCObject* child;
		CCARRAY_FOREACH(children, child) {
    		CCLabelBMFont* lbl = dynamic_cast<CCLabelBMFont*>(child);
    		if (lbl) {
				if (lbl->getID() == "frameLabel" || lbl->getID() == "stateLabel") lbl->removeFromParentAndCleanup(true);
   			}
		}
	}
	
	mod->setSettingValue("frame_stepper", false);
}

class mobileButtons {
public:
	void frameAdvance(CCObject*) {
	if (!mod->getSettingValue<bool>("disable_frame_stepper")) {
		if (mod->getSettingValue<bool>("frame_stepper")) stepFrame = true;
		else {
			mod->setSettingValue("frame_stepper", true);
			if (disableFSBtn == nullptr)  {
				int y = 35;
				if (PlayLayer::get()->m_levelSettings->m_platformerMode) y = 95;

				auto winSize = CCDirector::sharedDirector()->getWinSize();
				CCSprite* spr = nullptr;
				CCMenuItemSpriteExtra* btn = nullptr;
				spr = CCSprite::createWithSpriteFrameName("GJ_deleteSongBtn_001.png");
				spr->setOpacity(102);
        		spr->setScale(0.8f);
				btn = CCMenuItemSpriteExtra::create(
        		spr,
        		PlayLayer::get(),
				menu_selector(mobileButtons::disableFrameStepper)
    			);
				btn->setPosition(winSize/2 + ccp(-winSize.width/2, -winSize.height/2) + ccp(70, y));
				btn->setZOrder(100);
				btn->setID("disable_fs_btn");
				buttonsMenu->addChild(btn);
				disableFSBtn = btn;
			}
		} 
	}
}

void toggleSpeedhack(CCObject*) {
	if (!mod->getSettingValue<bool>("disable_speedhack")) {
		if (prevSpeed != 1 && mod->getSettingValue<double>("speedhack") == 1)
			mod->setSettingValue("speedhack", prevSpeed);
		else {
			prevSpeed = mod->getSettingValue<double>("speedhack");
			mod->setSavedValue<float>("previous_speed", prevSpeed);
			mod->setSettingValue("speedhack", 1.0);
		}
	}
}

void disableFrameStepper(CCObject*) {
	if (mod->getSettingValue<bool>("frame_stepper")) {
		mod->setSettingValue("frame_stepper", false);
		if (disableFSBtn != nullptr) {
			disableFSBtn->removeFromParent();
			disableFSBtn = nullptr;
		}
	}
}

};

void addButton(const char* id) {
	auto winSize = CCDirector::sharedDirector()->getWinSize();
	CCSprite* spr = nullptr;
	CCMenuItemSpriteExtra* btn = nullptr;

	int y = 35;
	if (PlayLayer::get()->m_levelSettings->m_platformerMode) y = 95;
		
	if (id == "advance_frame_btn") {
		spr = CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
        spr->setScale(0.8f);
		spr->setOpacity(102);
        auto icon = CCSprite::createWithSpriteFrameName("GJ_arrow_02_001.png");
        icon->setPosition(spr->getContentSize() / 2 + ccp(2.5f,0));
        icon->setScaleY(0.7f);
        icon->setScaleX(-0.7f);
		icon->setOpacity(102);
        spr->addChild(icon);
		btn = CCMenuItemSpriteExtra::create(
        	spr,
        	PlayLayer::get(),
			menu_selector(mobileButtons::frameAdvance)
    	);
		btn->setPosition(winSize/2 + ccp(-winSize.width/2, -winSize.height/2) + ccp(20, y));
		btn->setID(id);
		btn->setZOrder(100);
		buttonsMenu->addChild(btn);
		advanceFrameBtn = btn;
	} else if (id == "speedhack_btn") {
		spr = CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
        spr->setScale(0.8f);
		spr->setOpacity(102);
        auto icon = CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png");
        icon->setPosition(spr->getContentSize() / 2);
		icon->setOpacity(102);
        spr->addChild(icon);
		btn = CCMenuItemSpriteExtra::create(
        	spr,
        	PlayLayer::get(),
			menu_selector(mobileButtons::toggleSpeedhack)
    	);
		btn->setPosition(winSize/2 + ccp(winSize.width/2, -winSize.height/2) + ccp(-20, 35));
		btn->setID(id);
		btn->setZ
