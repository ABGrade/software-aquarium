#include "app.hpp"
#include <iostream>
int main(int argc, char** argv) {
    const char* path=argc>1?argv[1]:"aquarium-preview.bmp";
    App app;
    if (!app.exportPreview(path)) { std::cerr<<"Could not write preview\n"; return 1; }
    std::cout<<"Preview saved to "<<path<<'\n';
    return 0;
}
