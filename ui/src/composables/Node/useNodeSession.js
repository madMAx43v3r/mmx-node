import { useSession, useLogin } from "@/queries/server";

export const useNodeSession = () => {
    const sessionStore = useSessionStore();
    const { data } = useSession();

    // const $q = useQuasar();
    const onError = () => {
        sessionStore.doLogout(false);
        // $q.notify({
        //     message: "Login failed!", //TODO i18n
        //     type: "negative",
        // });
    };

    const login = useLogin();
    const handleLogin = (credentials) => {
        const payload = {
            credentials,
        };
        login.mutate(payload, {
            onError,
        });
    };

    watch(data, (data) => {
        if (!(data && data.user)) {
            if (sessionStore.autoLogin) {
                handleLogin(sessionStore.credentials);
            } else {
                sessionStore.doLogout(false);
            }
        }
    });

    // eslint-disable-next-line no-undef
    // if (process.env.NODE_ENV === "production") {
    //     watch(isError, (isError) => {
    //         if (isError) {
    //             sessionStore.doLogout(false);
    //         }
    //     });
    // }
};
