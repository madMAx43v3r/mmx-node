//
// https://github.com/quasarframework/quasar/discussions/16989#discussioncomment-10430556

export const useFormValidator = (formRef) => {
    const rulesValidityStatuses = ref([]);
    const rulesWatchers = ref([]);

    const isSetupValidationDone = ref(false);

    const setupValidation = () => {
        if (formRef.value) {
            //console.log("setupValidation");
            const components = formRef.value.getValidationComponents().filter((item) => item.rules);
            rulesValidityStatuses.value = new Array(components.length).fill(false);

            components.forEach((component, index) => {
                const ruleWatcher = watch(
                    () => component.modelValue,
                    (value) => {
                        const status = component.rules?.every((rule) => rule(value) === true) ?? false;
                        rulesValidityStatuses.value[index] = status;
                    },
                    { deep: true, immediate: true }
                );
                rulesWatchers.value.push(ruleWatcher);
            });
            isSetupValidationDone.value = true;
        }
    };

    const isNotValid = computed(() => rulesValidityStatuses.value.some((status) => status === false));
    const isValid = computed(() => (isSetupValidationDone.value ? isNotValid.value === false : false));

    onMounted(() => {
        setupValidation();
    });

    const resetRulesWatchers = () => {
        rulesWatchers.value.forEach((cleanup) => cleanup());
        rulesWatchers.value = [];
    };

    if (import.meta.hot) {
        import.meta.hot.on("vite:beforeUpdate", () => {
            resetRulesWatchers();

            formRef.value?.resetValidation();
            rulesValidityStatuses.value = [];

            setTimeout(() => setupValidation(), 1);
        });
    }

    return { isValid, setupValidation };
};
