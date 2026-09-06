#include <cassert>
#include <cstdio>
#include "../src/runtime/VirtualFs.h"

int main() {
    port::VirtualFs fs;
    fs.mount("save:/", "/data/save");
    fs.mount("rom:/", "/data/rom");

    auto a = fs.resolve("save:/MarioOdyssey/progress");
    assert(a.has_value() && *a == "/data/save/MarioOdyssey/progress");

    auto b = fs.resolve("rom:/Data/Stage.szs");
    assert(b.has_value() && *b == "/data/rom/Data/Stage.szs");

    // sem barras extras e prefixo sem barra
    auto c = fs.resolve("save:config.ini");
    assert(c.has_value() && *c == "/data/save/config.ini");

    // prefixo desconhecido = nullopt (não quebra, só não resolve)
    assert(!fs.resolve("sdcard:/x").has_value());

    // sem dois-pontos = nullopt
    assert(!fs.resolve("relative/path").has_value());

    fs.unmount("save:/");
    assert(!fs.resolve("save:/a").has_value());
    assert(fs.mounts().size() == 1);

    std::printf("virtual fs tests passed!\n");
    return 0;
}
