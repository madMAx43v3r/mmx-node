import { ConfigBuilder, BuildTargets } from "./vite.ConfigBuilder";

const configBuilder = new ConfigBuilder({
    buildTarget: BuildTargets.EXPLORER,
    writeBuildInfo: true,
    usePublicRPC: true,
});

export default configBuilder.config;
