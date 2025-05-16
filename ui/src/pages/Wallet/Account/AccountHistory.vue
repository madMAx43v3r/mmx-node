<template>
    <div class="q-gutter-y-sm q-mt-sm">
        <q-card flat>
            <q-card-section>
                <div class="row">
                    <q-select
                        v-model="type"
                        :options="typeOptions"
                        emit-value
                        map-options
                        :label="$t('account_history.type')"
                        :disable="memo?.length > 0"
                        :clearable="type !== null"
                        class="col-3"
                    >
                        <template v-slot:selected-item="scope">
                            <div :class="getTypeFieldCssClasses(scope.opt.value)">
                                {{ scope.opt.label }}
                            </div>
                        </template>

                        <template v-slot:option="scope">
                            <q-item v-bind="scope.itemProps">
                                <q-item-section>
                                    <q-item-label :class="getTypeFieldCssClasses(scope.opt.value)">
                                        {{ scope.opt.label }}
                                    </q-item-label>
                                </q-item-section>
                            </q-item>
                        </template>
                    </q-select>
                    <TokenSelect
                        v-model="currency"
                        :label="$t('account_history.token')"
                        null-item
                        :null-label="$t('account_history_form.any')"
                        :clearable="currency !== null"
                        class="q-ml-md col"
                    />
                </div>
                <div class="row q-mt-xs">
                    <q-input v-model="memo" label="Memo" class="col" clearable />
                </div>
            </q-card-section>
        </q-card>

        <AccountHistoryTable
            :index="index"
            :limit="limit"
            :type="type"
            :currency="currency"
            :memo="memo"
            export-enabled
            class="q-mt-sm"
        />
    </div>
</template>

<script setup>
const props = defineProps({
    index: {
        type: Number,
        required: true,
    },
});

const limit = ref(200);
const type = ref(null);
const currency = ref(null);
const memo = ref();

watchEffect(() => {
    if (memo.value?.length > 0) {
        type.value = "RECEIVE";
    } else {
        type.value = null;
    }
});

const { t } = useI18n();
const typeOptions = computed(() => [
    { label: t("account_history_form.any"), value: null },
    { label: t("account_history_form.spend"), value: "SPEND" },
    { label: t("account_history_form.receive"), value: "RECEIVE" },
    { label: t("account_history_form.reward"), value: "REWARD" },
    { label: "VDF " + t("account_history_form.reward"), value: "VDF_REWARD" },
    { label: t("account_history_form.tx_fee"), value: "TXFEE" },
]);
</script>
