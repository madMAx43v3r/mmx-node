export const useWalletFormStatus = (formRef) => {
    const isValid = computed(() => formRef.value?.isValid ?? false);
    const isValidConfirmed = computed(() => formRef.value?.isValidConfirmed ?? false);

    return { isValid, isValidConfirmed };
};

export const useWalletFormStatusL = (formRef, isLocked) => {
    const { isValid, isValidConfirmed } = useWalletFormStatus(formRef);

    const isValidUnlocked = computed(() => isValid.value && !isLocked.value);
    const isValidConfirmedUnlocked = computed(() => isValidConfirmed.value && !isLocked.value);

    return { isValid, isValidConfirmed, isValidUnlocked, isValidConfirmedUnlocked };
};
