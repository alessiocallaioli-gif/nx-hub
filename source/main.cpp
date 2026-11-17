// Switch AIO Manager - updater basato su catalogo JSON (bozza, non ancora compilata/testata)
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <curl/curl.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "json.hpp"   // nlohmann/json single header
#include "miniz.h"    // miniz amalgamato (miniz.c + miniz.h)
using json = nlohmann::json;
using std::string;

#define REGISTRY_URL "https://raw.githubusercontent.com/alessiocallaioli-gif/switch-aio-registry/main/packages.json"
#define CFG "/config/switch-aio/"

struct Pkg {
    string id, name, cat, repo, pattern, type, target, detect, latest, url, installed, status;
    bool upd = false;
};
static std::vector<Pkg> pkgs;
static json installedDb = json::object();
static SDL_Renderer* R;
static TTF_Font* F;
static int sel = 0;

// ---------- utilita' ----------
static size_t wr(void* p, size_t s, size_t n, void* u) { ((string*)u)->append((char*)p, s * n); return s * n; }
static bool httpGet(const string& url, string& out) {
    CURL* c = curl_easy_init(); if (!c) return false;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, wr);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "switch-aio-manager");
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);
    CURLcode rc = curl_easy_perform(c); long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(c);
    return rc == CURLE_OK && code == 200;
}
static void mkdirs(const string& p) { for (size_t i = 1; i < p.size(); i++) if (p[i] == '/') mkdir(p.substr(0, i).c_str(), 0777); }
static string readFile(const string& p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }
static void writeFile(const string& p, const string& d) { mkdirs(p); std::ofstream f(p, std::ios::binary); f.write(d.data(), d.size()); }
static bool exists(const string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }

// ---------- catalogo ----------
static void loadRegistry() {
    pkgs.clear();
    string s;
    if (httpGet(REGISTRY_URL, s)) writeFile(CFG "packages.json", s); else s = readFile(CFG "packages.json");
    for (string src : {s, readFile(CFG "custom.json")}) {   // catalogo ufficiale + personale
        try {
            for (auto& j : json::parse(src)["packages"]) {
                Pkg p;
                p.id = j.value("id", ""); p.name = j.value("name", p.id); p.cat = j.value("category", "homebrew");
                p.repo = j.value("repo", ""); p.pattern = j.value("asset_pattern", "*.zip");
                p.type = j.value("install", "zip"); p.target = j.value("target", "/");
                p.detect = j.value("detect", "");
                p.latest = j.value("latest", ""); p.url = j.value("url", "");   // opzionali: precalcolati dal registro
                pkgs.push_back(p);
            }
        } catch (...) {}
    }
    try { installedDb = json::parse(readFile(CFG "installed.json")); } catch (...) { installedDb = json::object(); }
}

static void refresh(Pkg& p) {
    if (p.latest.empty() && !p.repo.empty()) {
        string r;
        if (httpGet("https://api.github.com/repos/" + p.repo + "/releases/latest", r)) {
            try {
                json j = json::parse(r);
                p.latest = j.value("tag_name", "");
                for (auto& a : j["assets"])
                    if (fnmatch(p.pattern.c_str(), a.value("name", "").c_str(), 0) == 0) { p.url = a["browser_download_url"]; break; }
            } catch (...) {}
        }
    }
    p.installed = installedDb.value(p.id, "");
    bool present = !p.detect.empty() && exists(p.detect);
    if (p.latest.empty() || p.url.empty()) { p.upd = false; p.status = "Errore: release non raggiungibile"; }
    else if (p.installed.empty()) { p.upd = true; p.status = present ? "Installato (versione ignota) - aggiorna per tracciarlo" : "Non installato"; }
    else if (p.installed != p.latest) { p.upd = true; p.status = "Aggiornamento: " + p.installed + " -> " + p.latest; }
    else { p.upd = false; p.status = "Aggiornato (" + p.latest + ")"; }
}

static bool install(Pkg& p) {
    string data;
    if (!httpGet(p.url, data)) return false;
    if (p.type == "file") {
        writeFile(p.target, data);
    } else {
        mz_zip_archive z{};
        if (!mz_zip_reader_init_mem(&z, data.data(), data.size(), 0)) return false;
        string base = p.target; if (base.back() != '/') base += '/';
        for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&z); i++) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&z, i, &st)) continue;
            string n = st.m_filename;
            if (n.find("..") != string::npos) continue;          // protezione zip-slip
            string out = base + n;
            mkdirs(out);
            if (!st.m_is_directory) mz_zip_reader_extract_to_file(&z, i, out.c_str(), 0);
        }
        mz_zip_reader_end(&z);
    }
    installedDb[p.id] = p.latest;
    writeFile(CFG "installed.json", installedDb.dump(2));
    refresh(p);
    return true;
}

// ---------- grafica ----------
static void text(const string& s, int x, int y, SDL_Color c) {
    if (s.empty()) return;
    SDL_Surface* sf = TTF_RenderUTF8_Blended(F, s.c_str(), c); if (!sf) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(R, sf);
    SDL_Rect d{x, y, sf->w, sf->h}; SDL_RenderCopy(R, t, nullptr, &d);
    SDL_FreeSurface(sf); SDL_DestroyTexture(t);
}
static void rect(int x, int y, int w, int h, SDL_Color c, bool fill = true) {
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a); SDL_Rect r{x, y, w, h};
    if (fill) SDL_RenderFillRect(R, &r); else SDL_RenderDrawRect(R, &r);
}
static void draw() {
    SDL_SetRenderDrawColor(R, 16, 18, 26, 255); SDL_RenderClear(R);
    rect(0, 0, 1280, 70, {24, 28, 42, 255});
    text("Switch AIO Manager", 40, 18, {0, 220, 190, 255});
    int upd = 0; for (auto& p : pkgs) upd += p.upd;
    text(std::to_string(upd) + " aggiornamenti disponibili", 900, 18, {255, 190, 60, 255});
    const int rowH = 76, visible = 7;
    int first = sel >= visible ? sel - visible + 1 : 0;
    for (int i = first; i < (int)pkgs.size() && i < first + visible; i++) {
        Pkg& p = pkgs[i]; int y = 90 + (i - first) * rowH;
        SDL_Color acc = p.upd ? SDL_Color{255, 170, 40, 255} : (p.latest.empty() ? SDL_Color{200, 70, 70, 255} : SDL_Color{60, 200, 110, 255});
        rect(40, y, 1200, rowH - 8, {26, 30, 44, 255});
        rect(40, y, 8, rowH - 8, acc);
        text(p.name + "   [" + p.cat + "]", 70, y + 4, {240, 240, 245, 255});
        text(p.status, 70, y + 34, {150, 160, 180, 255});
        if (i == sel) rect(40, y, 1200, rowH - 8, {0, 220, 190, 255}, false);
    }
    rect(0, 660, 1280, 60, {24, 28, 42, 255});
    text("A Installa/Aggiorna     Y Aggiorna tutto     X Ricarica catalogo     + Esci", 40, 675, {180, 190, 210, 255});
}
static void present() { draw(); SDL_RenderPresent(R); }
static void banner(const string& m) {
    draw();
    rect(300, 290, 680, 120, {30, 34, 48, 255}); rect(300, 290, 680, 120, {0, 220, 190, 255}, false);
    text(m, 330, 335, {255, 255, 255, 255});
    SDL_RenderPresent(R);
}
static void loadAll() {
    banner("Carico il catalogo...");
    loadRegistry();
    for (auto& p : pkgs) { banner("Controllo " + p.name + "..."); refresh(p); }
    if (sel >= (int)pkgs.size()) sel = 0;
}

int main() {
    socketInitializeDefault(); plInitialize(PlServiceType_User); curl_global_init(CURL_GLOBAL_DEFAULT);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK); TTF_Init();
    SDL_Window* w = SDL_CreateWindow("aio", 0, 0, 1280, 720, SDL_WINDOW_FULLSCREEN);
    R = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_JoystickOpen(0);
    PlFontData fd; plGetSharedFontByType(&fd, PlSharedFontType_Standard);
    F = TTF_OpenFontRW(SDL_RWFromMem(fd.address, fd.size), 1, 26);

    loadAll();
    bool run = true;
    while (run && appletMainLoop()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type != SDL_JOYBUTTONDOWN) continue;
            int b = e.jbutton.button, n = (int)pkgs.size();
            if (b == 13 && sel > 0) sel--;                       // su
            else if (b == 15 && sel + 1 < n) sel++;              // giu
            else if (b == 0 && n) {                              // A
                banner("Installo " + pkgs[sel].name + "...");
                if (!install(pkgs[sel])) pkgs[sel].status = "Installazione fallita";
            } else if (b == 3) {                                 // Y
                for (auto& p : pkgs) {
                    if (!p.upd) continue;
                    banner("Aggiorno " + p.name + "...");
                    if (!install(p)) p.status = "Installazione fallita";
                }
            } else if (b == 2) loadAll();                        // X
            else if (b == 10) run = false;                       // +
        }
        present();
    }
    TTF_CloseFont(F); TTF_Quit(); SDL_Quit();
    curl_global_cleanup(); plExit(); socketExit();
    return 0;
}
