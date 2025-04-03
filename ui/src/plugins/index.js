/**
 * plugins/index.js
 *
 * Automatically included in `./src/main.js`
 */

// Plugins
import { Quasar, quasarConfig } from "./quasar";
import pinia from "./pinia";
import router from "./router";
import i18n from "./i18n";
import VueQueryPlugin, { vueQueryPluginOptions } from "./query";
import highlight from "./highlight";

export function registerPlugins(app) {
    app.use(Quasar, quasarConfig);
    app.use(router);
    app.use(pinia);
    app.use(i18n);
    app.use(VueQueryPlugin, vueQueryPluginOptions);
    app.use(highlight);
}
