export default [
    {
        path: "/market",
        component: () => import("@/pages/MarketPage"),
        meta: {
            title: "Market",
        },
        children: [
            {
                path: "",
                redirect: "/market/offers",
            },
            {
                path: "offers",
                component: () => import("@/pages/MarketPage/MarketPageOffers"),
                meta: {
                    title: "Offers",
                },
            },
            {
                path: "history",
                component: () => import("@/pages/MarketPage/MarketPageHistory"),
                meta: {
                    title: "History",
                },
            },
        ],
    },
];
