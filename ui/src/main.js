// FILE: main.js

import { createApp } from "vue";

// Plugins
import { registerPlugins } from "@/plugins";

// Import icon libraries
//import "@quasar/extras/roboto-font/roboto-font.css";
//import "@quasar/extras/material-icons/material-icons.css";

// Import Quasar css
import "quasar/src/css/index.sass";

import "animate.css";

import "unfonts.css";
import "@/css/app.scss";

// Assumes your root component is App.vue
// and placed in same folder as main.js
import App from "./App.vue";

const app = createApp(App);
registerPlugins(app);

// Assumes you have a <div id="app"></div> in your index.html
app.mount("#app");
