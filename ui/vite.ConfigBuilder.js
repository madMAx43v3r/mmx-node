import { defineConfig } from "vite";

import AutoImport from "unplugin-auto-import/vite";
import Fonts from "unplugin-fonts/vite";
import Components from "unplugin-vue-components/vite";

import GenerateFile from "vite-plugin-generate-file";
import { viteSingleFile } from "vite-plugin-singlefile";
import VueDevTools from "vite-plugin-vue-devtools";

import { quasar, transformAssetUrls } from "@quasar/vite-plugin";
import vue from "@vitejs/plugin-vue";
import { nodePolyfills } from "vite-plugin-node-polyfills";

import path from "node:path";
import { fileURLToPath, URL } from "node:url";

export const BuildTargets = {
    GUI: "GUI",
    EXPLORER: "EXPLORER",
    OFFLINE: "OFFLINE",
    PLAYGROUND: "PLAYGROUND",
};

export class ConfigBuilder {
    #date = new Date();

    txQRSendBaseUrl = "https://goldfish-app-wmlwj.ondigitalocean.app";
    publicRPCUrl = "https://rpc.mmx.network";

    buildTarget = BuildTargets.GUI;
    writeBuildInfo = false;
    usePublicRPC = false;
    allowCustomRPC = false;
    useDefaultRollupOptions = false;
    useSingleFile = false;

    constructor(options) {
        Object.assign(this, options);
    }

    get config() {
        const config = defineConfig(this.initConfig);

        (config.build ??= {}).outDir = `dist/${this.buildTarget.toLowerCase()}`;

        (config.define ??= {}).__BUILD_TARGET__ = JSON.stringify(this.buildTarget);
        (config.define ??= {}).__TX_QR_SEND_BASE_URL__ = JSON.stringify(
            // eslint-disable-next-line no-undef
            process.env.NODE_ENV === "production" ? this.txQRSendBaseUrl : undefined
        );

        (config.define ??= {}).__PUBLIC_RPC_URL__ = JSON.stringify(this.publicRPCUrl);

        if (this.usePublicRPC) {
            (config.define ??= {}).__WAPI_URL__ = JSON.stringify(
                // eslint-disable-next-line no-undef
                process.env.NODE_ENV === "production" ? this.publicRPCUrl : undefined
            );
        }

        (config.define ??= {}).__ALLOW_CUSTOM_RPC__ = JSON.stringify(this.allowCustomRPC);

        if (this.writeBuildInfo) {
            const buildId = Math.floor(Math.random() * Number.MAX_SAFE_INTEGER)
                .toString(16)
                .toUpperCase();
            console.log("Build Id:", buildId);

            (config.define ??= {}).__BUILD_ID__ = JSON.stringify(buildId);

            (config.plugins ??= []).push(
                GenerateFile([
                    {
                        type: "json",
                        output: "./guiBuildInfo.json",
                        data: {
                            id: buildId,
                            timestamp: this.#date.getTime(),
                            datetime: this.#date.toISOString(),
                        },
                    },
                ])
            );
        }

        if (this.singleFile) {
            this.useDefaultRollupOptions = true;
            (config.plugins ??= []).push(viteSingleFile());
        }

        if (this.useDefaultRollupOptions) {
            (config.build ??= {}).rollupOptions = {};
        }

        //console.log("config :", config);
        return config;
    }

    // https://vitejs.dev/config/
    initConfig = {
        base: "./",
        plugins: [
            nodePolyfills(),
            VueDevTools(),
            vue({
                template: { transformAssetUrls },
            }),
            // @quasar/plugin-vite options list:
            // https://github.com/quasarframework/quasar/blob/dev/vite-plugin/index.d.ts
            quasar({
                sassVariables: "/src/css/quasar.variables.scss",
            }),
            Components({ dts: true }),
            Fonts({
                fontsource: {
                    families: [
                        {
                            name: "Roboto Flex Variable",
                            variable: {
                                wght: true,
                            },
                        },
                        {
                            name: "Roboto Mono Variable",
                            variable: {
                                wght: true,
                            },
                        },
                    ],
                },
            }),
            AutoImport({
                imports: [
                    "vue",
                    {
                        "vue-i18n": ["useI18n"],
                        "vue-router": ["useRoute", "useRouter"],
                        "@vueuse/core": ["computedAsync"],
                        quasar: ["useQuasar"],
                    },
                ],
                dirs: ["src/utils/**/*", "src/composables/**/*", "src/stores/**/*"],
                eslintrc: {
                    enabled: true,
                },
                vueTemplate: true,
            }),
        ],
        resolve: {
            alias: {
                "@": fileURLToPath(new URL("./src", import.meta.url)),
                "@mmxConfig": fileURLToPath(new URL("../config", import.meta.url)),
                //"@bex": fileURLToPath(new URL("./src-bex", import.meta.url)),
            },
            extensions: [".js", ".json", ".jsx", ".mjs", ".ts", ".tsx", ".vue"],
        },
        optimizeDeps: {
            // exclude: ["echarts"],
            entries: ["./src/**/*.{vue,js,jsx,ts,tsx}"],
        },
        css: {
            preprocessorOptions: {
                // Use sass-embedded for better stability and performance
                sass: {
                    api: "modern-compiler",
                },
                scss: {
                    api: "modern-compiler",
                },
            },
        },
        test: {
            globals: true,
            environment: "node",
            // setupFiles: "@testing-library/jest-dom",
            mockReset: true,
        },
        build: {
            target: "es2020",
            chunkSizeWarningLimit: 1000,
            rollupOptions: {
                output: {
                    manualChunks: (id) => {
                        //console.log(id);

                        if (id.includes("/node_modules/")) {
                            if (id.includes("@scure")) {
                                return "@scure";
                            }

                            if (id.includes("zrender") || id.includes("echarts")) {
                                return "echarts";
                            }

                            if (id.includes("/quasar/lang/")) {
                                const { base } = path.parse(id);
                                return "locales/quasar/" + base;
                            }

                            if (id.includes("vue") || id.includes("quasar")) {
                                return "quasar";
                            }

                            return "modules";
                        }

                        if (id.includes("/src/locales/")) {
                            const { base } = path.parse(id);
                            return "locales/" + base;
                        }

                        if (id.includes("/src/mmx/wallet/")) {
                            return "mmx-wallet";
                        }

                        if (id.includes("/src/")) {
                            return "ui";
                        }

                        if (id.includes("/config/")) {
                            return "mmx-config";
                        }

                        //console.log(id);
                        return;
                    },
                },
            },
        },
        server: {
            port: 3000,
            hmr: {
                path: "/__hmr",
                clientPort: 3000,
            },
            // warmup: {
            //     clientFiles: ["./src/components/**/*.vue", "./src/pages/**/*.vue"],
            // },
            proxy: {
                "/api": {
                    target: "http://127.0.0.1:11380",
                    changeOrigin: true,
                },
                "/wapi": {
                    target: "http://127.0.0.1:11380",
                    changeOrigin: true,
                },
                "/server": {
                    target: "http://127.0.0.1:11380",
                    changeOrigin: true,
                },
            },
        },
    };
}
