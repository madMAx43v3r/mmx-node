import { useConfig, useSetConfig } from "@/queries/wapi";

const configMapping = {
    timelord: {
        name: "timelord",
        restart: true,
        default: false,
    },
    open_port: {
        name: "Router.open_port",
        restart: true,
        default: false,
    },
    allow_remote: {
        name: "allow_remote",
        restart: true,
        default: false,
    },

    opencl_platform: {
        name: "opencl.platform",
        restart: true,
        default: null,
        suppressNotification: true,
    },
    opencl_device: {
        name: "Node.opencl_device",
        restart: true,
        suppressNotification: true,
    },
    opencl_device_name: {
        name: "Node.opencl_device_name",
        restart: true,
    },
    opencl_device_select: {
        name: "Node.opencl_device_select",
        restart: true,
        default: -1,
        tmp_only: true,
        suppressNotification: true,
    },
    opencl_device_list: {
        // locally calculated field
        default: [],
    },
    opencl_device_list_relidx: {
        // locally calculated field
        default: [],
    },

    farmer_reward_addr: {
        name: "Farmer.reward_addr",
        restart: true,
    },
    timelord_reward_addr: {
        name: "TimeLord.reward_addr",
        restart: true,
    },

    harv_num_threads: {
        name: "Harvester.num_threads",
        restart: true,
    },
    reload_interval: {
        name: "Harvester.reload_interval",
        restart: true,
    },
    recursive_search: {
        name: "Harvester.recursive_search",
        restart: true,
        default: false,
    },

    plot_dirs: {
        name: "Harvester.plot_dirs",
        restart: false,
    },

    version: {
        name: "build.version",
    },
    commit: {
        name: "build.commit",
    },
};

const getInitData = () => {
    const data = {};
    Object.keys(configMapping).forEach((key) => {
        const { default: defaultValue } = configMapping[key];
        data[key] = defaultValue ?? null;
    });
    return data;
};

const initData = getInitData();

const assign = (target, source) => {
    if (!source) return;

    Object.keys(configMapping).forEach((key) => {
        const { name, default: defaultValue } = configMapping[key];
        if (name === undefined) return;
        target[key] = source?.[name] ?? defaultValue;
    });

    {
        target.opencl_device_list = [];
        target.opencl_device_list_relidx = [];

        target.opencl_device_list.push({ label: "None", value: -1 });
        let list = source["Node.opencl_device_list"];
        if (list) {
            for (const [i, device] of list.entries()) {
                target.opencl_device_list.push({ label: device[0], value: i });
                target.opencl_device_list_relidx.push({ name: device[0], index: device[1] });
            }
        }
    }
};
export function useConfigData() {
    const setConfig = useSetConfig();

    const { data: queryData, isPending, isError } = useConfig();
    const loading = computed(() => isPending.value || isError.value);

    const data = reactive({ ...initData });

    const unwatchArr = [];
    watchEffect(() => {
        Object.keys(unwatchArr).forEach((key) => unwatchArr[key]());
        assign(data, queryData.value);
        Object.keys(data).forEach((key) => {
            unwatchArr[key] = watch(
                () => data[key],
                (value, prev) => {
                    const configKey = configMapping[key].name;
                    setConfig.mutate({ ...configMapping[key], key: configKey, value });
                }
            );
        });
    });

    const isWallet = computed(() => {
        if (!queryData.value) return false;
        const data = queryData.value?.wallet;
        return data || data == null ? true : false;
    });

    const isFarmer = computed(() => {
        if (!queryData.value) return false;
        const data = queryData.value?.farmer;
        return data || data == null ? true : false;
    });

    const isLocalNode = computed(() => {
        if (!queryData.value) return false;
        const data = queryData.value?.local_node;
        return data || data == null ? true : false;
    });

    return { data, loading, isWallet, isFarmer, isLocalNode };
}
