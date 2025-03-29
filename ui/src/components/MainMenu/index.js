import GuiMainMenu from "@/components/MainMenu/GuiMainMenu.vue";
import ExplorerMainMenu from "@/components/MainMenu/ExplorerMainMenu.vue";
import OfflineMainMenu from "@/components/MainMenu/OfflineMainMenu.vue";

let menu;
// eslint-disable-next-line no-undef
if (__BUILD_TARGET__ === "EXPLORER") {
    menu = ExplorerMainMenu;
    // eslint-disable-next-line no-undef
} else if (__BUILD_TARGET__ === "OFFLINE") {
    menu = OfflineMainMenu;
} else {
    menu = GuiMainMenu;
}

export default menu;
