export const loadLayoutMiddleware = async (to) => {
    if (to.meta.layout === false) {
        to.meta.layoutComponent = "div";
        return;
    }

    const layout = to.meta.layout ?? "default";
    const layoutComponent = await import(`@/layouts/${layout}.vue`);
    to.meta.layoutComponent = layoutComponent.default;
};
