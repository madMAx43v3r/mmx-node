export const useWalletForm = (formData, defaultFormData) => {
    const formRef = ref(null);

    const confirmed = ref(false);

    const formDataClone = computed(() => ({ ...formData }));
    watch(formDataClone, (newValue, oldValue) => {
        if (newValue.feeRatio !== oldValue.feeRatio) {
            return;
        }
        confirmed.value = false;
    });

    watch(confirmed, (value) => {
        if (value) {
            validate();
        }
    });

    const validate = () => {
        formRef.value.validate();
    };

    const { isValid, setupValidation } = useFormValidator(formRef);
    const onDialogShow = () => {
        setupValidation();
    };

    const isValidConfirmed = computed(() => {
        return isValid.value && confirmed.value;
    });

    const reset = () => {
        Object.assign(formData, defaultFormData);
        confirmed.value = false;
    };

    return {
        formRef,
        confirmed,
        isValid,
        isValidConfirmed,
        reset,
        validate,
        onDialogShow,
    };
};
