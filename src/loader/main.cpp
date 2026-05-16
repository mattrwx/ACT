#include "gui/gui.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    gui::init();

    while (gui::alive())
        gui::render();

    gui::shutdown();
    return 0;
}