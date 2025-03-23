import hljs from "highlight.js/lib/core";
import javascript from "highlight.js/lib/languages/javascript";
import json from "highlight.js/lib/languages/json";

hljs.registerLanguage("mmx", javascript);
hljs.registerLanguage("json", json);

import hljsVuePlugin from "@highlightjs/vue-plugin";
export default hljsVuePlugin;
