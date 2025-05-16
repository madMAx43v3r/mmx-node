<template>
    <q-input
        v-model="input"
        :placeholder="$t('explore_menu.placeholder')"
        :error="error"
        hide-bottom-space
        outlined
        @keyup.enter="handleSubmit"
    >
        <template v-slot:append>
            <q-icon :name="mdiMagnify" @click="handleSubmit" />
        </template>
    </q-input>
</template>

<script setup>
import { mdiMagnify } from "@mdi/js";

const router = useRouter();
const route = useRoute();

const searchParams = computed(
    () =>
        route.params.height ||
        route.params.hash ||
        route.params.transactionId ||
        route.params.address ||
        route.params.farmerKey ||
        ""
);

const input = ref(searchParams.value);
const error = ref(false);

watchEffect(() => (input.value = searchParams.value));

import { useCheckHeader } from "@/queries/wapi";
const checkHeader = useCheckHeader();

const handleSubmit = () => {
    const hexPattern = /[0-9A-Fa-f]{64}/g;
    const numberPattern = /^\d+$/;
    error.value = false;
    if (input.value) {
        if (input.value.startsWith("mmx1")) {
            router.push("/explore/address/" + input.value);
        } else if (hexPattern.test(input.value)) {
            if (input.value.length == 64) {
                checkHeader.mutate(
                    { hash: input.value },
                    {
                        onSettled: (data) => {
                            if (data) {
                                router.push("/explore/block/hash/" + input.value);
                            } else {
                                router.push("/explore/transaction/" + input.value);
                            }
                        },
                    }
                );
            } else if (input.value.length == 66) {
                router.push("/explore/farmer/" + input.value);
            } else {
                error.value = true;
            }
        } else if (numberPattern.test(input.value)) {
            router.push("/explore/block/height/" + input.value);
        } else {
            error.value = true;
        }
    }
};
</script>
