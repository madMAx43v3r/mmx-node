import { useGithubLatestRelease } from "@/queries/external";

export const useVersionChecker = () => {
    const { data: localConfigData } = useConfigData();
    const { data: latestRelease } = useGithubLatestRelease();

    const version = computed(() => localConfigData.version?.split("-", 1)[0]);
    const latestVersion = computed(() => latestRelease.value?.name);

    const isNewVersionAvailable = computed(() => {
        if (version.value && latestVersion.value && version.value !== latestVersion.value) {
            return true;
        }
        return false;
    });

    return { isNewVersionAvailable, latestVersion, latestRelease, version };
};
