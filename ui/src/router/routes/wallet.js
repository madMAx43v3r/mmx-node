export default [
    {
        path: "/wallet",
        component: () => import("@/pages/WalletPage"),
        children: [
            {
                path: "",
                component: () => import("@/pages/Wallet/AccountIndex"),
                meta: {
                    title: "Wallets",
                },
            },
            {
                path: "create",
                component: () => import("@/pages/Wallet/WalletCreate"),
                meta: {
                    title: "Create wallet",
                },
            },
            {
                path: "account/:index",
                component: () => import("@/pages/Wallet/AccountView"),
                props: (route) => ({ index: parseInt(route.params.index) }),
                meta: {
                    title: (route) => `Wallet #${route.params.index}`,
                },
                children: [
                    {
                        path: "",
                        component: () => import("@/pages/Wallet/Account/AccountHome"),
                        meta: {
                            title: "Balance",
                        },
                    },
                    {
                        path: "nfts",
                        component: () => import("@/pages/Wallet/Account/AccountNFT"),
                        meta: {
                            title: "NFTs",
                        },
                    },
                    {
                        path: "contracts",
                        component: () => import("@/pages/Wallet/Account/AccountContracts"),
                        meta: {
                            title: "Contracts",
                        },
                    },
                    {
                        path: "send/:target?",
                        component: () => import("@/pages/Wallet/Account/AccountSend"),
                        props: (route) => ({ target: route.params.target }),
                        meta: {
                            title: "Send",
                        },
                    },
                    {
                        path: "send_from/:source?",
                        component: () => import("@/pages/Wallet/Account/AccountSend"),
                        props: (route) => ({ source: route.params.source }),
                        meta: {
                            title: "Send",
                        },
                    },
                    {
                        path: "history",
                        component: () => import("@/pages/Wallet/Account/AccountHistory"),
                        meta: {
                            title: "History",
                        },
                    },
                    {
                        path: "log",
                        component: () => import("@/pages/Wallet/Account/AccountLog"),
                        meta: {
                            title: "Log",
                        },
                    },
                    {
                        path: "offer",
                        component: () => import("@/pages/Wallet/Account/AccountOffer"),
                        meta: {
                            title: "Offer",
                        },
                    },
                    {
                        path: "liquid",
                        component: () => import("@/pages/Wallet/Account/AccountLiquid"),
                        meta: {
                            title: "Liquidity",
                        },
                    },
                    {
                        path: "plotnfts",
                        component: () => import("@/pages/Wallet/Account/AccountPlotNFTs"),
                        meta: {
                            title: "Plot NFTs",
                        },
                    },
                    {
                        path: "details",
                        component: () => import("@/pages/Wallet/Account/AccountDetails"),
                        meta: {
                            title: "Info",
                        },
                    },
                    {
                        path: "options",
                        component: () => import("@/pages/Wallet/Account/AccountOptions"),
                        meta: {
                            title: "Options",
                        },
                    },
                ],
            },
        ],
    },
];
