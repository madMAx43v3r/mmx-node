export default [
    {
        path: "/farmer",
        component: () => import("@/pages/FarmerPage"),
        meta: {
            title: "Farmer",
        },
        children: [
            {
                path: "",
                redirect: "/farmer/plots",
            },
            {
                path: "plots",
                component: () => import("@/pages/Farmer/FarmerPlotsIndex"),
                meta: {
                    title: "Plots",
                },
            },
            {
                path: "plotnfts",
                component: () => import("@/pages/Farmer/FarmerPlotNFTsIndex"),
                meta: {
                    title: "PlotNFTs",
                },
            },
            {
                path: "blocks",
                component: () => import("@/pages/Farmer/FarmerBlocksIndex"),
                meta: {
                    title: "Blocks",
                },
            },
            {
                path: "proofs",
                component: () => import("@/pages/Farmer/FarmerProofsIndex"),
                meta: {
                    title: "Proofs",
                },
            },
        ],
    },
];
