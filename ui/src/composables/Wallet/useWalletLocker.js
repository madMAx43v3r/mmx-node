import { useQuasar } from "quasar";
import { useQueryClient } from "@tanstack/vue-query";
import { useWalletIsLocked, fetchWalletIsLocked, useWalletLock, useWalletUnlock } from "@/queries/api";
import { useWalletAccount } from "@/queries/wapi";

export const useWalletLocker = (props) => {
    const queryClient = useQueryClient();

    const { data: account } = useWalletAccount(props);
    const withPassphrase = computed(() => (account.value?.with_passphrase ? account.value.with_passphrase : false));

    const { data: isLockedData } = useWalletIsLocked(props, withPassphrase);

    const isLocked = computed(() => (withPassphrase.value ? (isLockedData.value ?? true) : false));

    const update = async () => {
        await fetchWalletIsLocked(queryClient, props);
    };

    const handleToggleLock = async () => {
        //await update();
        if (isLocked.value) {
            handleUnlock();
        } else {
            await lock(props);
        }
    };

    const walletUnlock = useWalletUnlock(props);
    const unlock = async (passphrase) => {
        await walletUnlock.mutateAsync(passphrase);
    };

    const walletLock = useWalletLock(props);
    const lock = async () => {
        await walletLock.mutateAsync();
    };

    const $q = useQuasar();
    const showPrompt = () =>
        new Promise((resolve, reject) => {
            $q.dialog({
                component: defineAsyncComponent(() => import("@/components/Dialogs/PromptPassphraseDialog")),
            })
                .onOk(async ({ passphrase }) => {
                    resolve(passphrase);
                })
                .onCancel(() => {
                    reject(new Error("Prompt rejected!"));
                });
        });

    const promptPassphrase = async () => {
        if (isLocked.value) {
            return await showPrompt();
        }
    };

    const handleUnlock = async () => {
        if (isLocked.value) {
            const passphrase = await promptPassphrase();
            await unlock(passphrase);
        }
    };

    return {
        isLocked,
        handleToggleLock,
        promptPassphrase,
    };
};
