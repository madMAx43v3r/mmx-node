export default [
    {
        path: "/swap",
        component: () => import("@/pages/SwapPage"),
        meta: {
            title: "Swap",
        },
        children: [
            {
                path: "",
                component: () => import("@/pages/Swap/SwapList"),
            },
            {
                path: ":address",
                component: () => import("@/pages/Swap/SwapView"),
                props: (route) => ({
                    address: route.params.address,
                }),
                children: [
                    {
                        path: "",
                        redirect: { name: "trade" },
                    },
                    {
                        name: "trade",
                        path: "trade",
                        component: () => import("@/pages/Swap/SwapTrade"),
                        meta: {
                            title: "Trade",
                        },
                    },
                    {
                        path: "history",
                        component: () => import("@/pages/Swap/SwapHistory"),
                        meta: {
                            title: "History",
                        },
                    },
                    {
                        path: "liquid",
                        component: () => import("@/pages/Swap/SwapLiquid"),
                        meta: {
                            title: "Liquid",
                        },
                    },
                    {
                        path: "pool",
                        component: () => import("@/pages/Swap/SwapPool"),
                        meta: {
                            title: "Pool",
                        },
                    },
                ],
            },
        ],
    },
];
