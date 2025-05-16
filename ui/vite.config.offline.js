import { ConfigBuilder, BuildTargets } from "./vite.ConfigBuilder";

const configBuilder = new ConfigBuilder({
    buildTarget: BuildTargets.OFFLINE,
    usePublicRPC: true,
    useSingleFile: true,
});

export default configBuilder.config;
