import { useQueryClient } from "@tanstack/vue-query";
import { useSession, useLogin } from "@/queries/server";

export const useNodeSession = () => {
    const sessionStore = useSessionStore();

    const { data } = useSession();

    const login = useLogin();
    const queryClient = useQueryClient();
    const handleLogin = (credentials) => {
        login.mutate(
            { credentials },
            {
                onSuccess: () => queryClient.invalidateQueries({ queryKey: ["config"] }),
                onError: () => sessionStore.doLogout(false),
            }
        );
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
