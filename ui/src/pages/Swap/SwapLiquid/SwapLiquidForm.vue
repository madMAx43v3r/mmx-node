<template>
    <div>
        <q-card flat>
            <WForm ref="formRef" :data="formData" :default-data="defaultFormData" @reset="handleReset">
                <q-card-section>
                    <div class="row justify-end q-col-gutter-x-md">
                        <TxFeeSelect v-model="formData.feeRatio" class="col-sm-3 col-xs-12" />
                        <q-input
                            :model-value="tokenWalletBalance"
                            :label="$t('swap.wallet_ballance')"
                            :suffix="data?.symbols[0]"
                            input-class="amount-input"
                            readonly
                            class="col-sm-3 col-xs-6"
                        />
                        <q-input
                            :model-value="currencyWalletBalance"
                            :label="$t('swap.wallet_ballance')"
                            :suffix="data?.symbols[1]"
                            input-class="amount-input"
                            readonly
                            class="col-sm-3 col-xs-6"
                        />
                    </div>
                    <div class="row justify-end q-col-gutter-x-md">
                        <q-select
                            v-model="formData.poolIdx"
                            :options="poolFeeRates"
                            emit-value
                            map-options
                            :label="$t('swap.fee_level')"
                            class="col-sm-3 col-xs-12"
                        />
                        <q-input
                            :model-value="price"
                            :label="$t('common.price')"
                            :suffix="`${data?.symbols[1]} / ${data?.symbols[0]}`"
                            input-class="amount-input"
                            readonly
                            class="col-sm-3 col-xs-12"
                        />
                        <q-input
                            v-model.number="formData.tokenAmount"
                            :debounce="200"
                            :label="$t('common.amount')"
                            input-class="amount-input"
                            :suffix="data?.symbols[0]"
                            :rules="[rules.amount, hasTokenFunds]"
                            class="col-sm-3 col-xs-6"
                        />
                        <q-input
                            v-model.number="formData.currencyAmount"
                            :debounce="200"
                            :label="$t('common.amount')"
                            input-class="amount-input"
                            :suffix="data?.symbols[1]"
                            :rules="[rules.amount, hasCurrencyFunds]"
                            class="col-sm-3 col-xs-6"
                        />
                    </div>
                </q-card-section>
                <q-card-actions>
                    <div class="row justify-end col-12 q-gutter-x-xs q-gutter-y-sm">
                        <div class="col">
                            <WBtnReset class="items-start" />
                        </div>
                        <q-btn
                            :label="$t('swap.price_match')"
                            outline
                            :disable="priceMatchDisable"
                            @click="handlePriceMatch"
                        />

                        <q-btn :label="$t('swap.payout')" outline :disable="payoutDisable" @click="handlePayout" />
                        <q-btn
                            :label="$t('swap.switch_fee')"
                            outline
                            :disable="switchFeeDisable"
                            @click="handleSwitchFee"
                        />
                        <q-btn
                            :label="$t('swap.add_liquidity')"
                            outline
                            :disable="addLiquidityDisable"
                            class="text-positive"
                            @click="handleAddLiquidity"
                        />
                        <q-btn
                            :label="$t('swap.remove_liquidity')"
                            outline
                            :disable="removeLiquidityDisable"
                            class="text-negative"
                            @click="handleRemoveLiquidity"
                        />
                        <q-btn
                            :label="$t('swap.remove_all')"
                            outline
                            :disable="removeAllDisable"
                            class="text-negative"
                            @click="handleRemoveAll"
                        />
                    </div>
                </q-card-actions>
            </WForm>
        </q-card>
    </div>
</template>

<script setup>
import rules from "@/helpers/rules";

const props = defineProps({
    address: {
        type: String,
        required: true,
    },
});

const { activeWalletIndex } = useActiveWalletStore();

import { useWalletAddress } from "@/queries/wapi";
const { data: userAddress } = useWalletAddress(reactive({ index: activeWalletIndex }));

import { useSwapUserInfo } from "@/queries/wapi";
const { data: swapUserInfo } = useSwapUserInfo(
    reactive({ id: toRef(() => props.address), user: userAddress }),
    () => !!userAddress.value
);

const defaultFormData = {
    tokenAmount: null,
    currencyAmount: null,
    poolIdx: null,
    feeRatio: null,
};

const formRef = ref(null);
const formData = reactive({ ...defaultFormData });

const { promptPassphrase, isLocked } = useWalletLocker(reactive({ index: activeWalletIndex }));
const { isValid, isValidConfirmed, isValidUnlocked, isValidConfirmedUnlocked } = useWalletFormStatusL(
    formRef,
    isLocked
);

import { useSwapInfo } from "@/queries/wapi";
const { data } = useSwapInfo(reactive({ id: toRef(() => props.address) }));

watchEffect(() => {
    formData.poolIdx = swapUserInfo.value?.pool_idx ?? -1;
});

const handleReset = () => {
    formData.poolIdx = swapUserInfo.value?.pool_idx ?? -1;
};

const poolFeeRates = computed(() => {
    if (data.value) {
        const res = data.value.fee_rates.map((feeRate, i) => ({ label: `${(feeRate * 100).toFixed(2)} %`, value: i }));
        return [{ label: "", value: -1 }, ...res];
    } else {
        return [];
    }
});

import { useWalletBalance } from "@/queries/wapi";
const { data: tokenWalletBalance } = useWalletBalance(
    reactive({
        index: activeWalletIndex,
        currency: toRef(() => data.value?.tokens[0]),
    }),
    (data) => (data ? data.spendable : 0),
    () => !!data.value?.tokens[0]
);
const { data: currencyWalletBalance } = useWalletBalance(
    reactive({
        index: activeWalletIndex,
        currency: toRef(() => data.value?.tokens[1]),
    }),
    (data) => (data ? data.spendable : 0),
    () => !!data.value?.tokens[1]
);

const hasTokenFunds = (value) => value <= tokenWalletBalance.value || "Insufficient funds";
const hasCurrencyFunds = (value) => value <= currencyWalletBalance.value || "Insufficient funds";

const price = computed(() => {
    if (formData.tokenAmount && formData.currencyAmount) {
        return parseFloat((formData.currencyAmount / formData.tokenAmount).toPrecision(6));
    } else {
        return null;
    }
});

const priceMatchDisable = computed(() => {
    return (!!formData.tokenAmount && !!formData.currencyAmount) || (!formData.tokenAmount && !formData.currencyAmount);
});
const handlePriceMatch = () => {
    if (formData.tokenAmount && !formData.currencyAmount) {
        formData.currencyAmount = parseFloat((formData.tokenAmount * data.value.price).toFixed(data.value.decimals[1]));
    } else if (!formData.tokenAmount && formData.currencyAmount) {
        formData.tokenAmount = parseFloat((formData.currencyAmount / data.value.price).toFixed(data.value.decimals[0]));
    }
};

const paid = ref(false);

const payoutDisable = computed(
    () =>
        !swapUserInfo.value ||
        paid.value ||
        !(swapUserInfo.value.fees_earned[0].amount != 0 || swapUserInfo.value.fees_earned[1].amount != 0)
);

const switchFeeDisable = computed(
    () =>
        formData.poolIdx < 0 ||
        (swapUserInfo.value &&
            (swapUserInfo.value.pool_idx < 0 ||
                formData.poolIdx == swapUserInfo.value.pool_idx ||
                (swapUserInfo.value.balance[0].amount == 0 && swapUserInfo.value.balance[1].amount == 0)))
);

const addRemoveLiquidityDisable = computed(
    () => !(!!formData.tokenAmount || !!formData.currencyAmount) || formData.poolIdx < 0
);

const addLiquidityDisable = computed(
    () =>
        addRemoveLiquidityDisable.value ||
        (swapUserInfo.value.pool_idx >= 0 &&
            formData.poolIdx != swapUserInfo.value.pool_idx &&
            (swapUserInfo.value.balance[0].amount != 0 || swapUserInfo.value.balance[1].amount != 0))
);
const removeLiquidityDisable = computed(() => addRemoveLiquidityDisable.value);

const removeAllDisable = computed(
    () =>
        !!formData.tokenAmount ||
        !!formData.currencyAmount ||
        !swapUserInfo.value ||
        !(swapUserInfo.value.balance[0].amount != 0 || swapUserInfo.value.balance[1].amount != 0)
);

import {
    useWalletSwapPayout,
    useWalletSwapSwitchPool,
    useWalletSwapAddLiquidity,
    useWalletSwapRemoveLiquidity,
    useWalletSwapRemoveAllLiquidity,
} from "@/queries/wapi";

const walletSwapPayout = useWalletSwapPayout();
const handlePayout = async () => {
    const payload = {
        index: activeWalletIndex.value,
        address: props.address,
        options: {
            fee_ratio: formData.feeRatio,
        },
    };
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload, options: { ...payload.options, passphrase } };
    // console.log(payloadWithPassphrase);

    await walletSwapPayout.mutateAsync(payloadWithPassphrase, {
        onSuccess: () => {
            paid.value = true;
        },
    });
};
const walletSwapSwitchPool = useWalletSwapSwitchPool();
const handleSwitchFee = async () => {
    const payload = {
        index: activeWalletIndex.value,
        address: props.address,
        pool_idx: formData.poolIdx,
        options: {
            fee_ratio: formData.feeRatio,
        },
    };
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload, options: { ...payload.options, passphrase } };
    // console.log(payloadWithPassphrase);

    await walletSwapSwitchPool.mutateAsync(payloadWithPassphrase);
};

const walletSwapAddLiquidity = useWalletSwapAddLiquidity();
const handleAddLiquidity = async () => {
    const payload = {
        index: activeWalletIndex.value,
        address: props.address,
        pool_idx: formData.poolIdx,
        amount: [formData.tokenAmount, formData.currencyAmount],
        options: {
            fee_ratio: formData.feeRatio,
        },
    };
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload, options: { ...payload.options, passphrase } };
    // console.log(payloadWithPassphrase);

    await walletSwapAddLiquidity.mutateAsync(payloadWithPassphrase, {
        onSuccess: () => {
            formRef.value.reset();
        },
    });
};

const walletSwapRemoveLiquidity = useWalletSwapRemoveLiquidity();
const handleRemoveLiquidity = async () => {
    const payload = {
        index: activeWalletIndex.value,
        address: props.address,
        pool_idx: formData.poolIdx,
        amount: [formData.tokenAmount, formData.currencyAmount],
        options: {
            fee_ratio: formData.feeRatio,
        },
    };
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload, options: { ...payload.options, passphrase } };
    // console.log(payloadWithPassphrase);

    await walletSwapRemoveLiquidity.mutateAsync(payloadWithPassphrase, {
        onSuccess: () => {
            formRef.value.reset();
        },
    });
};

const walletSwapRemoveAllLiquidity = useWalletSwapRemoveAllLiquidity();
const handleRemoveAll = async () => {
    const payload = {
        index: activeWalletIndex.value,
        address: props.address,
        options: {
            fee_ratio: formData.feeRatio,
        },
    };
    const passphrase = await promptPassphrase();
    const payloadWithPassphrase = { ...payload, options: { ...payload.options, passphrase } };
    // console.log(payloadWithPassphrase);

    await walletSwapRemoveAllLiquidity.mutateAsync(payloadWithPassphrase);
};
</script>
