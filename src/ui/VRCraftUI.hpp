#pragma once

namespace vrcraft {

class VRCraftMod;

class VRCraftUI {
public:
    explicit VRCraftUI(VRCraftMod& mod);

    bool registerMenuAndButtons();
    void unregisterMenuAndButtons();
    void refreshHud();

private:
    VRCraftMod& mMod;
    bool        mRegistered = false;
};

} // namespace vrcraft
