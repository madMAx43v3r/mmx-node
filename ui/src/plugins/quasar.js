import { Quasar, Notify, Dialog } from "quasar";
import iconSet from "quasar/icon-set/svg-mdi-v7";

export { Quasar };

export const quasarConfig = {
    config: { dark: true },
    plugins: { Notify, Dialog },
    iconSet: iconSet,
};
