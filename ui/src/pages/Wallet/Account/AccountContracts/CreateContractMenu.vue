<template>
    <div>
        <q-card flat>
            <q-card-section>
                <div class="q-gutter-y-sm">
                    <q-select
                        v-model="type"
                        :options="types"
                        :label="$t('create_contract_menu.contract_type')"
                        emit-value
                        map-options
                    />
                    <div class="row justify-end">
                        <q-btn
                            :label="$t('common.create')"
                            :icon="mdiReceiptTextPlus"
                            text-color="primary"
                            outline
                            :disable="!type"
                            @click="handleCreate"
                        />
                    </div>
                </div>
            </q-card-section>
        </q-card>
    </div>
</template>
<script setup>
import { mdiReceiptTextPlus } from "@mdi/js";
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const type = ref();
const types = computed(() => [
    //{ value: "virtualplot", label: "mmx.contract.VirtualPlot" }
]);

const router = useRouter();
const handleCreate = () => {
    router.push("/wallet/account/" + props.index + "/create/" + type.value);
};
</script>
