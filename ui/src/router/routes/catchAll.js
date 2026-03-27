export default [
    {
        path: "/:catchAll(.*)*",
        component: () => import("@/pages/ErrorNotFound"),
        meta: {
            requiresAuth: false,
            title: "Page not found",
        },
    },
];
