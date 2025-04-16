import { mdiLinkEdit } from "@mdi/js";

export const useWapiChanger = () => {
    const appStore = useAppStore();

    const $q = useQuasar();
    const handleWapiChange = () => {
        $q.dialog({
            title: "Enter wapi url",
            // message: "Enter wapi url",
            prompt: {
                model: appStore.wapiBaseUrl,
                type: "text",
                isValid: (value) => {
                    if (isEmpty(value)) return true;

                    try {
                        new URL(value);
                        return true;
                    } catch (error) {
                        return false;
                    }
                },
            },
            cancel: true,
            persistent: true,
        }).onOk((data) => {
            appStore._wapiBaseUrl = data;
        });
    };

    const routeTabItem = computed(() => ({
        icon: mdiLinkEdit,
        click: handleWapiChange,
    }));

    return { routeTabItem, handleWapiChange };
};
