export default [
    {
        path: "/node",
        component: () => import("@/pages/NodePage"),
        meta: {
            title: "Node",
        },
        children: [
            {
                path: "",
                redirect: "/node/log",
            },
            {
                path: "log",
                component: () => import("@/pages/Node/LogIndex"),
            },
            {
                path: "peers",
                component: () => import("@/pages/Node/PeersIndex"),
                meta: {
                    title: "Peers",
                },
            },
            {
                path: "blocks",
                component: () => import("@/pages/Explore/BlocksIndex"),
                props: { limit: 20 },
                meta: {
                    title: "Blocks",
                },
            },
            {
                path: "netspace",
                component: () => import("@/pages/Node/NetspaceChart"),
                meta: {
                    title: "Netspace",
                },
            },
            {
                path: "vdf_speed",
                component: () => import("@/pages/Node/VdfSpeedChart"),
                meta: {
                    title: "VDF Speed",
                },
                alias: [],
            },
            {
                path: "reward",
                component: () => import("@/pages/Node/RewardChart"),
                meta: {
                    title: "Block Reward",
                },
                alias: [],
            },
        ],
    },
];
