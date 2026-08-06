#include "client/ui/OVSettingsPanel.h"

#include <iostream>

int main()
{
    ov::client::OVSettingsPanel panel;
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        panel.SetOpen(true);
        if (!panel.IsOpen()) return 1;
        panel.SetOpen(false);
        if (panel.IsOpen()) return 2;
    }
    std::cout << "Settings open/close state test passed\n";
    return 0;
}
