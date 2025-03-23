import { createRouter, createWebHashHistory } from "vue-router";
import routes from "@/router/routes";

const router = createRouter({
    history: createWebHashHistory(import.meta.env.BASE_URL),
    routes: routes,
});

import { loadLayoutMiddleware } from "@/router/middleware/loadLayoutMiddleware";
import { setTitleMiddleware } from "@/router/middleware/setTitleMiddleware";
import { authMiddleware } from "@/router/middleware/authMiddleware";

router.beforeEach(loadLayoutMiddleware);
router.afterEach(setTitleMiddleware);

// eslint-disable-next-line no-undef
if (__BUILD_TARGET__ == "GUI") {
    router.beforeEach(authMiddleware);
}

export default router;
