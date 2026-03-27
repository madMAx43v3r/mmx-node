import { prefetchFarmers, prefetchBlock } from "@/queries/wapi";

export default [
    {
        path: "/explore",
        component: () => import("@/pages/ExplorePage"),
        meta: {
            title: "Explore",
        },
        children: [
            {
                path: "",
                redirect: "/explore/blocks",
            },
            {
                path: "blocks",
                component: () => import("@/pages/Explore/BlocksIndex"),
                meta: {
                    title: "Blocks",
                    indexPage: true,
                },
            },
            {
                path: "block/hash/:hash",
                component: () => import("@/pages/Explore/BlockView"),
                props: (route) => ({ hash: route.params.hash }),
                meta: {
                    title: (route) => `Block #${route.params.hash}`,
                    prefetcher: (queryClient, route) => {
                        const params = { hash: route.params.hash };
                        prefetchBlock(queryClient, params);
                    },
                },
            },
            {
                path: "block/height/:height",
                component: () => import("@/pages/Explore/BlockView"),
                props: (route) => ({ height: parseInt(route.params.height) }),
                meta: {
                    title: (route) => `Block #${route.params.height}`,
                    prefetcher: (queryClient, route) => {
                        const params = { height: parseInt(route.params.height) };
                        prefetchBlock(queryClient, params);
                    },
                },
            },
            {
                path: "transactions",
                component: () => import("@/pages/Explore/TransactionsIndex"),
                meta: {
                    title: "Transactions",
                    indexPage: true,
                },
            },
            {
                name: "transaction",
                path: "transaction/:transactionId",
                component: () => import("@/pages/Explore/TransactionView"),
                props: (route) => ({ transactionId: route.params.transactionId }),
                meta: {
                    title: (route) => `Transaction #${route.params.transactionId}`,
                },
            },
            {
                path: "farmers",
                component: () => import("@/pages/Explore/FarmersIndex"),
                meta: {
                    title: "Farmers",
                    indexPage: true,
                    prefetcher: (queryClient, route) => {
                        const params = { limit: 100 };
                        prefetchFarmers(queryClient, params);
                    },
                },
            },
            {
                path: "farmer/:farmerKey",
                component: () => import("@/pages/Explore/FarmerView"),
                props: (route) => ({ farmerKey: route.params.farmerKey }),
                meta: {
                    title: (route) => `Farmer #${route.params.farmerKey}`,
                },
            },
            {
                name: "address",
                path: "address/:address",
                component: () => import("@/pages/Explore/AddressView"),
                props: (route) => ({ address: route.params.address }),
                meta: {
                    title: (route) => `Address #${route.params.address}`,
                },
            },
        ],
    },
];
