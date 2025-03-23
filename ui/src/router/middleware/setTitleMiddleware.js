import { useTitle } from "@vueuse/core";
export const setTitleMiddleware = async (to) => {
    const defaultTitle = "MMX Node";
    var title = defaultTitle;
    to.matched.forEach((element) => {
        const subTitle = element.meta.title;
        if (subTitle) {
            if (subTitle instanceof Function) {
                title += ` — ${subTitle(to)}`;
            } else {
                title += ` — ${subTitle}`;
            }
        }
    });
    useTitle(title);
};
