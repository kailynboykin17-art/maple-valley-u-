// Maple Valley U — 3DS visual novel engine
// Top screen: background, character portrait, gold-trimmed dialogue box (like the mockup).
// Bottom screen: 2x2 choice buttons with tone tags (Sweet / Bitchy / Good / Bad / Bold),
// relationship meters, and the title menu. Autosaves to sdmc:/3ds/maple-valley-u.sav
#include <3ds.h>
#include <citro2d.h>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>

struct Line { std::string op, a, b; };
struct Choice { std::string tone, text, label; };

static std::vector<Line> script;
static std::map<std::string, size_t> labels;
static std::map<std::string, int> vars;
static std::string curFile, epTitle, bgName, portrait;
static size_t pc = 0;

// ---------- textures ----------
struct Img { C3D_Tex tex; Tex3DS_SubTexture sub; C2D_Image img; bool ok = false; };
static std::map<std::string, Img*> texCache;

static Img* loadTex(const std::string& path, int w, int h, int W, int H) {
    auto it = texCache.find(path);
    if (it != texCache.end()) return it->second;
    Img* im = new Img();
    FILE* f = fopen(path.c_str(), "rb");
    if (f && C3D_TexInit(&im->tex, W, H, GPU_RGBA8)) {
        fread(im->tex.data, 1, W * H * 4, f);
        GSPGPU_FlushDataCache(im->tex.data, W * H * 4);
        C3D_TexSetFilter(&im->tex, GPU_LINEAR, GPU_LINEAR);
        im->sub = { (u16)w, (u16)h, 0.0f, 1.0f, (float)w / W, 1.0f - (float)h / H };
        im->img = { &im->tex, &im->sub };
        im->ok = true;
    }
    if (f) fclose(f);
    texCache[path] = im;
    return im;
}

// ---------- script ----------
static void load(const std::string& file) {
    script.clear(); labels.clear(); pc = 0; curFile = file; portrait.clear();
    std::ifstream f("romfs:/script/" + file);
    std::string s;
    while (std::getline(f, s)) {
        while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back();
        if (s.empty() || s[0] == '#') continue;
        if (s[0] == '*') { labels[s.substr(1)] = script.size(); continue; }
        Line l;
        if (s.rfind("> ", 0) == 0) {
            size_t ar = s.rfind("->");
            l = { "choice", s.substr(2, ar - 3), s.substr(ar + 3) };
        } else if (s[0] == '@') {
            std::istringstream ss(s.substr(1)); ss >> l.op >> l.a; std::getline(ss, l.b);
            if (!l.b.empty() && l.b[0] == ' ') l.b.erase(0, 1);
        } else {
            size_t c = s.find(": ");
            if (c != std::string::npos && c < 24) l = { "say", s.substr(0, c), s.substr(c + 2) };
            else l = { "say", "", s };
        }
        script.push_back(l);
    }
}

static void saveGame() {
    FILE* f = fopen("sdmc:/3ds/maple-valley-u.sav", "w");
    if (!f) return;
    fprintf(f, "%s\n%u\n", curFile.c_str(), (unsigned)pc);
    for (auto& v : vars) fprintf(f, "%s %d\n", v.first.c_str(), v.second);
    fclose(f);
}
static bool loadGame() {
    std::ifstream f("sdmc:/3ds/maple-valley-u.sav");
    std::string file; size_t p;
    if (!(f >> file >> p)) return false;
    load(file); pc = p > 0 ? p - 1 : 0;
    vars.clear(); std::string k; int v;
    while (f >> k >> v) vars[k] = v;
    return true;
}

// ---------- state ----------
enum Mode { TITLE, PLAY, PROFILES };
struct CharProfile { std::map<std::string, std::string> f; };
static std::vector<CharProfile> profiles;
static int profSel = 0, profPage = 0;
static std::string L(CharProfile& P, const char* k, const char* d) { auto it = P.f.find(std::string("l_") + k); return it != P.f.end() ? it->second : d; }
static void loadProfiles() {
    std::ifstream f("romfs:/profiles.txt"); std::string s; CharProfile cur;
    while (std::getline(f, s)) {
        while (!s.empty() && s.back() == '\r') s.pop_back();
        if (s.empty()) { if (!cur.f.empty()) profiles.push_back(cur); cur = CharProfile(); continue; }
        if (s[0] == '#') continue;
        size_t e = s.find('='); if (e != std::string::npos) cur.f[s.substr(0, e)] = s.substr(e + 1);
    }
    if (!cur.f.empty()) profiles.push_back(cur);
}
static Mode mode = TITLE;
static std::string speaker, text;
static std::vector<Choice> choices;
static int sel = 0, titleSel = 0;
static size_t shown = 0; // typewriter

static void advance() {
    choices.clear(); shown = 0;
    while (pc < script.size()) {
        Line l = script[pc++];
        if (l.op == "say") {
            speaker = l.a; text = l.b;
            if (!speaker.empty()) {
                std::string id; for (char ch : speaker) if (isalnum((unsigned char)ch)) id += tolower(ch);
                FILE* t = fopen(("romfs:/gfx/char/" + id + ".tex").c_str(), "rb");
                if (t) { fclose(t); portrait = id; }
            }
            return;
        }
        if (l.op == "choice") {
            Choice c; c.label = l.b; c.text = l.a;
            if (!l.a.empty() && l.a[0] == '[') { size_t e = l.a.find(']'); c.tone = l.a.substr(1, e - 1); c.text = l.a.substr(e + 2); }
            choices.push_back(c);
            if (pc >= script.size() || script[pc].op != "choice") { sel = 0; saveGame(); return; }
            continue;
        }
        if (l.op == "title") epTitle = l.a + (l.b.empty() ? "" : " " + l.b);
        else if (l.op == "bg") bgName = l.a;
        else if (l.op == "show") portrait = l.a;
        else if (l.op == "hide") portrait.clear();
        else if (l.op == "add") vars[l.a] += atoi(l.b.c_str());
        else if (l.op == "jump") pc = labels[l.a];
        else if (l.op == "if") { std::istringstream ss(l.b); int v; std::string t; ss >> v >> t; if (vars[l.a] >= v) pc = labels[t]; }
        else if (l.op == "ifnot") { std::istringstream ss(l.b); int v; std::string t; ss >> v >> t; if (vars[l.a] < v) pc = labels[t]; }
        else if (l.op == "load") { load(l.a); saveGame(); }
    }
    speaker = ""; text = "To be continued...";
}

static void startFile(const std::string& f) { vars.clear(); load(f); advance(); mode = PLAY; }

// ---------- drawing ----------
static C2D_TextBuf buf;
static u32 C(u8 r, u8 g, u8 b, u8 a = 255) { return C2D_Color32(r, g, b, a); }

static void txt(const std::string& s, float x, float y, float sc, u32 col, float wrap = 0, bool center = false) {
    C2D_Text t; C2D_TextParse(&t, buf, s.c_str()); C2D_TextOptimize(&t);
    u32 fl = C2D_WithColor | (wrap > 0 ? C2D_WordWrap : 0) | (center ? C2D_AlignCenter : 0);
    C2D_DrawText(&t, fl, x, y, 0.0f, sc, sc, col, wrap);
}
static void frame(float x, float y, float w, float h, u32 fill, u32 line) {
    C2D_DrawRectSolid(x, y, 0, w, h, line);
    C2D_DrawRectSolid(x + 1, y + 1, 0, w - 2, h - 2, fill);
}
static u32 toneColor(const std::string& t) {
    if (t == "Sweet") return C(0xF2, 0x9C, 0xC0);
    if (t == "Bitchy") return C(0xD9, 0x4A, 0x5A);
    if (t == "Good") return C(0x8F, 0xD1, 0x9E);
    if (t == "Bad") return C(0x9A, 0x6B, 0xD8);
    if (t == "Bold") return C(0xE8, 0xC2, 0x7A);
    return C(0xCC, 0xCC, 0xCC);
}
// placeholder backgrounds: warm gradient per location until real bg art is added
static void drawBg() {
    u32 a = C(0x3A, 0x2A, 0x24), b = C(0x12, 0x10, 0x14);
    if (bgName.find("track") != std::string::npos || bgName.find("field") != std::string::npos) { a = C(0x5C, 0x3A, 0x2E); b = C(0x1E, 0x2A, 0x22); }
    else if (bgName.find("night") != std::string::npos || bgName.find("party") != std::string::npos) { a = C(0x2A, 0x1C, 0x3A); b = C(0x0A, 0x08, 0x12); }
    else if (bgName.find("penthouse") != std::string::npos) { a = C(0x6A, 0x50, 0x3C); b = C(0x24, 0x1A, 0x16); }
    else if (bgName.find("soccer") != std::string::npos || bgName.find("pitch") != std::string::npos) { a = C(0x1C, 0x3A, 0x2A); b = C(0x08, 0x10, 0x0C); }
    C2D_DrawRectangle(0, 0, 0, 400, 240, a, a, b, b);
    std::string loc = bgName; for (auto& ch : loc) { if (ch == '_') ch = ' '; else ch = toupper(ch); }
    txt(loc, 12, 128, 0.38f, C(255, 255, 255, 70));
}

int main() {
    romfsInit(); gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE); C2D_Init(C2D_DEFAULT_MAX_OBJECTS); C2D_Prepare();
    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    buf = C2D_TextBufNew(8192);
    const u32 gold = C(0xD8, 0xB8, 0x7A), cream = C(0xF6, 0xEC, 0xE1), pink = C(0xF0, 0x7E, 0xB0),
              panel = C(0x14, 0x0E, 0x0C, 0xF0), green = C(0x0E, 0x3B, 0x26);
    Img* logo = loadTex("romfs:/gfx/logo.tex", 64, 64, 64, 64);
    Img* poster = loadTex("romfs:/gfx/title.tex", 400, 225, 512, 256);
    std::ifstream probe("sdmc:/3ds/maple-valley-u.sav"); bool hasSave = probe.good(); probe.close();
    const char* menu[] = { "Continue", "Season 1: One Pace at a Time", "Season 2: MVU Women's Soccer", "Character Profiles" };
    loadProfiles();

    while (aptMainLoop()) {
        hidScanInput(); u32 k = hidKeysDown(); touchPosition tp; hidTouchRead(&tp);
        if (k & KEY_START) { if (mode == PLAY) saveGame(); break; }

        if (mode == TITLE) {
            if (k & KEY_DOWN) titleSel = (titleSel + 1) % 4;
            if (k & KEY_UP) titleSel = (titleSel + 3) % 4;
            if (k & KEY_TOUCH) for (int i = 0; i < 4; i++) if (tp.py >= 52 + i * 44 && tp.py < 90 + i * 44) { titleSel = i; k |= KEY_A; }
            if (k & KEY_A) {
                if (titleSel == 0 && hasSave && loadGame()) { advance(); mode = PLAY; }
                else if (titleSel == 1) startFile("s1/ep01.mvs");
                else if (titleSel == 2) startFile("s2/ep01.mvs");
                else if (titleSel == 3 && !profiles.empty()) { mode = PROFILES; profPage = 0; }
            }
        } else if (mode == PROFILES) {
            int n = profiles.size();
            if (k & (KEY_RIGHT | KEY_R)) { profSel = (profSel + 1) % n; profPage = 0; }
            if (k & (KEY_LEFT | KEY_L)) { profSel = (profSel + n - 1) % n; profPage = 0; }
            if (k & (KEY_A | KEY_DOWN | KEY_UP)) profPage ^= 1;
            if (k & KEY_TOUCH) { if (tp.px < 60 && tp.py > 200) { profSel = (profSel + n - 1) % n; profPage = 0; } else if (tp.px > 260 && tp.py > 200) { profSel = (profSel + 1) % n; profPage = 0; } else profPage ^= 1; }
            if (k & KEY_B) mode = TITLE;
        } else {
            if (shown < text.size()) shown += 2;
            if (!choices.empty()) {
                int n = choices.size();
                if (k & KEY_RIGHT) sel = (sel + 1) % n;
                if (k & KEY_LEFT) sel = (sel + n - 1) % n;
                if (k & KEY_DOWN) sel = (sel + 2) % n;
                if (k & KEY_UP) sel = (sel + n - 2 + n) % n;
                if (k & KEY_TOUCH) for (int i = 0; i < n; i++) {
                    int x = 8 + (i % 2) * 156, y = 56 + (i / 2) * 76;
                    if (tp.px >= x && tp.px < x + 148 && tp.py >= y && tp.py < y + 68) { sel = i; k |= KEY_A; }
                }
                if ((k & KEY_A) && shown >= text.size()) {
                    if (!choices[sel].tone.empty()) vars["Tone_" + choices[sel].tone]++;
                    pc = labels[choices[sel].label]; advance();
                } else if (k & KEY_A) shown = text.size();
            } else if (k & (KEY_A | KEY_TOUCH)) {
                if (shown < text.size()) shown = text.size(); else advance();
            }
            if (k & KEY_B) { saveGame(); mode = TITLE; hasSave = true; }
        }

        C2D_TextBufClear(buf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        // ---------- TOP ----------
        C2D_TargetClear(top, C(0, 0, 0)); C2D_SceneBegin(top);
        if (mode == TITLE) {
            C2D_DrawRectangle(0, 0, 0, 400, 240, green, green, C(0x06, 0x14, 0x0C), C(0x06, 0x14, 0x0C));
            if (poster->ok) C2D_DrawImageAt(poster->img, 0, 8, 0.0f, nullptr, 1.0f, 1.0f);
            else if (logo->ok) C2D_DrawImageAt(logo->img, 168, 34, 0.0f, nullptr, 1.0f, 1.0f);
            txt("Touch or press A", 200, 205, 0.4f, C(255, 255, 255, 140), 0, true);
        } else if (mode == PROFILES) {
            CharProfile& P = profiles[profSel];
            u32 paper = C(0xFB, 0xE3, 0xEC), ink = C(0x5A, 0x1E, 0x3A), hot = C(0xE0, 0x5A, 0x96);
            C2D_DrawRectangle(0, 0, 0, 400, 240, C(0xF7, 0xC6, 0xD8), C(0xF7, 0xC6, 0xD8), C(0xE9, 0xA8, 0xC4), C(0xE9, 0xA8, 0xC4));
            for (int y = 8; y < 240; y += 16) C2D_DrawRectSolid(0, y, 0, 400, 1, C(255, 255, 255, 90));
            Img* p = loadTex("romfs:/gfx/char/" + P.f["id"] + ".tex", 192, 256, 256, 256);
            frame(236, 10, 156, 206, C(255, 255, 255), C(255, 255, 255));
            if (p->ok) C2D_DrawImageAt(p->img, 240, 14, 0.0f, nullptr, 0.77f, 0.77f);
            else if (logo->ok) C2D_DrawImageAt(logo->img, 270, 70, 0.0f, nullptr, 1.4f, 1.4f);
            frame(10, 10, 218, 46, paper, hot);
            txt(P.f["name"], 18, 14, 0.62f, ink, 206);
            txt("(" + P.f["pronouns"] + ")", 18, 36, 0.42f, hot);
            frame(10, 62, 218, 96, C(0xFF, 0xF4, 0xF8, 0xE8), hot);
            txt(L(P, "age", "Age") + ": " + P.f["age"], 18, 68, 0.48f, ink);
            txt(L(P, "height", "Height") + ": " + P.f["height"], 120, 68, 0.48f, ink);
            txt(L(P, "sign", "Zodiac") + ": " + P.f["sign"], 18, 90, 0.48f, ink);
            txt(L(P, "hometown", "From") + ": " + P.f["hometown"], 18, 112, 0.44f, ink, 204);
            txt(P.f["team"], 18, 134, 0.44f, hot, 204);
            frame(10, 164, 218, 52, C(0xFF, 0xF4, 0xF8, 0xE8), hot);
            txt(L(P, "food", "Fav food") + ": " + P.f["food"], 18, 170, 0.42f, ink, 204);
            txt(L(P, "music", "Fav music") + ": " + P.f["music"], 18, 192, 0.42f, ink, 204);
            char cnt[32]; snprintf(cnt, sizeof cnt, "%d / %d", profSel + 1, (int)profiles.size());
            txt(cnt, 314, 222, 0.4f, ink, 0, true);
        } else {
            drawBg();
            if (!portrait.empty()) {
                Img* p = loadTex("romfs:/gfx/char/" + portrait + ".tex", 192, 256, 256, 256);
                if (p->ok) C2D_DrawImageAt(p->img, 244, 24, 0.0f, nullptr, 0.72f, 0.72f);
            }
            // header bar
            C2D_DrawRectSolid(0, 0, 0, 400, 20, C(0x0C, 0x08, 0x08, 0xC8));
            C2D_DrawRectSolid(0, 20, 0, 400, 1, gold);
            txt("MAPLE VALLEY UNIVERSITY", 8, 4, 0.40f, cream);
            txt(epTitle, 392 - epTitle.size() * 5.0f, 4, 0.40f, gold);
            // dialogue box
            frame(8, 150, 384, 84, panel, gold);
            if (!speaker.empty()) { std::string up = speaker; for (auto& ch : up) ch = toupper(ch); txt(up, 18, 154, 0.55f, pink); }
            txt(text.substr(0, shown), 18, speaker.empty() ? 160 : 174, 0.48f, cream, 364);
            if (shown >= text.size() && choices.empty()) txt("v", 378, 220, 0.4f, gold);
        }
        // ---------- BOTTOM ----------
        C2D_TargetClear(bot, C(0x14, 0x0E, 0x0C)); C2D_SceneBegin(bot);
        if (mode == TITLE) {
            txt("MAPLE VALLEY U", 160, 22, 0.7f, gold, 0, true);
            for (int i = 0; i < 4; i++) {
                bool off = (i == 0 && !hasSave);
                frame(20, 52 + i * 44, 280, 38, i == titleSel ? C(0x3A, 0x2A, 0x20) : C(0x1E, 0x16, 0x12), gold);
                txt(menu[i], 160, 63 + i * 44, 0.48f, off ? C(120, 110, 100) : cream, 0, true);
            }
        } else if (mode == PROFILES) {
            CharProfile& P = profiles[profSel];
            u32 ink = C(0x5A, 0x1E, 0x3A), hot = C(0xE0, 0x5A, 0x96);
            C2D_DrawRectSolid(0, 0, 0, 320, 240, C(0xFB, 0xE3, 0xEC));
            for (int y = 26; y < 200; y += 15) C2D_DrawRectSolid(10, y, 0, 300, 1, C(0xA8, 0xC8, 0xF0, 120));
            C2D_DrawRectSolid(30, 0, 0, 1, 200, C(0xF0, 0x80, 0x90, 160));
            if (profPage == 0) {
                txt(L(P, "bio", "About"), 38, 4, 0.5f, hot);
                txt(P.f["bio"], 38, 26, 0.42f, ink, 272);
            } else {
                txt(L(P, "likes", "Likes") + ":", 38, 4, 0.5f, hot);
                txt(P.f["likes"], 38, 24, 0.42f, ink, 272);
                txt(L(P, "dislikes", "Dislikes") + ":", 38, 104, 0.5f, hot);
                txt(P.f["dislikes"], 38, 124, 0.42f, ink, 272);
            }
            frame(6, 204, 52, 30, C(0xF7, 0xC6, 0xD8), hot); txt("<", 32, 208, 0.6f, ink, 0, true);
            frame(262, 204, 52, 30, C(0xF7, 0xC6, 0xD8), hot); txt(">", 288, 208, 0.6f, ink, 0, true);
            txt(profPage == 0 ? "Tap/A: likes & dislikes   B: menu" : "Tap/A: about   B: menu", 160, 214, 0.36f, ink, 0, true);
        } else if (!choices.empty()) {
            txt("What does she do?", 160, 20, 0.5f, gold, 0, true);
            for (size_t i = 0; i < choices.size() && i < 4; i++) {
                int x = 8 + (i % 2) * 156, y = 56 + (i / 2) * 76;
                frame(x, y, 148, 68, (int)i == sel ? C(0x3A, 0x2A, 0x20) : C(0x1E, 0x16, 0x12), (int)i == sel ? gold : C(0x6A, 0x58, 0x40));
                if (!choices[i].tone.empty()) txt(choices[i].tone, x + 6, y + 4, 0.38f, toneColor(choices[i].tone));
                txt(choices[i].text, x + 6, y + 20, 0.42f, cream, 136);
            }
        } else {
            txt("RELATIONSHIPS", 160, 10, 0.5f, gold, 0, true);
            int row = 0;
            for (auto& v : vars) {
                if (v.first.rfind("Tone_", 0) == 0 || row > 8) continue;
                txt(v.first, 14, 34 + row * 21, 0.4f, cream);
                int w = v.second * 8; if (w > 140) w = 140; if (w < -140) w = -140;
                C2D_DrawRectSolid(170, 38 + row * 21, 0, 140, 8, C(0x30, 0x24, 0x1E));
                if (w > 0) C2D_DrawRectSolid(170, 38 + row * 21, 0, w, 8, pink);
                else if (w < 0) C2D_DrawRectSolid(170, 38 + row * 21, 0, -w, 8, C(0x90, 0x40, 0x48));
                row++;
            }
            txt("A: next   B: save & menu   START: quit", 160, 224, 0.38f, C(200, 190, 170), 0, true);
        }
        C3D_FrameEnd(0);
    }
    C2D_TextBufDelete(buf); C2D_Fini(); C3D_Fini(); gfxExit(); romfsExit();
    return 0;
}
