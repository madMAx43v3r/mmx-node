<template>
    <div class="fullscreen row justify-center">
        <q-card flat class="self-center col-xl-4 col-lg-6 col-md-8 col-sm-10 col-xs-12">
            <q-toolbar>
                <q-toolbar-title> Login </q-toolbar-title>
            </q-toolbar>
            <q-card-section>
                <q-form class="q-gutter-sm" @submit="handleLogin">
                    <q-input v-model="passwdPlain" type="password" required filled :label="$t('login.password_label')">
                        <template v-slot:prepend>
                            <q-icon :name="mdiLock" />
                        </template>
                    </q-input>
                    <div class="row justify-between">
                        <q-checkbox v-model="autoLogin" label="Save in Browser (until Logout)" />
                        <q-btn label="Login" :icon="mdiLogin" type="submit" color="primary" :disable="!passwdPlain" />
                    </div>
                </q-form>
            </q-card-section>
        </q-card>
    </div>
</template>

<script setup>
import { mdiLock, mdiAlertOctagon, mdiLogin } from "@mdi/js";

const passwdPlain = ref("");
const autoLogin = ref(false);

import { useLogin } from "@/queries/server";
const login = useLogin();
const $q = useQuasar();

const onError = () => {
    $q.notify({
        message: "Login failed!", //TODO i18n
        type: "negative",
    });
};

const sessionStore = useSessionStore();
const { autoLogin: autoLoginStore } = storeToRefs(sessionStore);

const handleLogin = () => {
    const payload = {
        credentials: {
            user: "mmx-admin",
            passwd_plain: passwdPlain.value,
        },
    };
    login.mutate(payload, {
        onSuccess: (data, variables) => {
            autoLoginStore.value = autoLogin.value;
            sessionStore.doLogin(variables.credentials);
        },
        onError,
    });
};
</script>
