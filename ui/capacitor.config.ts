import type { CapacitorConfig } from "@capacitor/cli";
import * as dotenv from "dotenv";

dotenv.config();

const config: CapacitorConfig = {
    appId: "network.mmx.mobile",
    appName: "MMX Wallet",
    webDir: "dist/explorer",
    android: {
        buildOptions: {
            releaseType: "APK",
            keystorePath: process.env.ANDROID_KEYSTORE_PATH,
            keystorePassword: process.env.ANDROID_KEYSTORE_PASSWORD,
            keystoreAlias: process.env.ANDROID_KEYSTORE_ALIAS,
            keystoreAliasPassword: process.env.ANDROID_KEYSTORE_ALIAS_PASSWORD,
        },
    },
};

export default config;
