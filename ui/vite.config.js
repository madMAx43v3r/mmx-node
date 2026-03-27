import { ConfigBuilder, BuildTargets } from "./vite.ConfigBuilder";

const configBuilder = new ConfigBuilder({
    buildTarget: BuildTargets.GUI,
    writeBuildInfo: true,
});

export default configBuilder.config;
