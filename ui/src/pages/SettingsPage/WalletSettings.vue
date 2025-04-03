<template>
    <q-card flat>
        <q-card-section>
            <q-markup-table flat>
                <thead>
                    <tr>
                        <th align="left">{{ $t("wallet_settings.name") }}</th>
                        <th align="left">{{ $t("wallet_settings.symbol") }}</th>
                        <th align="left">{{ $t("wallet_settings.contract") }}</th>
                        <th align="left"></th>
                    </tr>
                </thead>
                <tbody>
                    <template v-for="item in tokens" :key="item.currency">
                        <template v-if="!item.is_native">
                            <tr>
                                <td>{{ item.name }}</td>
                                <td>{{ item.symbol }}</td>
                                <td>
                                    <RouterLink :to="'/explore/address/' + item.currency" class="text-primary mono">
                                        {{ item.currency }}
                                    </RouterLink>
                                </td>
                                <td>
                                    <q-btn outline @click="handleRemoveToken(item.currency)">
                                        {{ $t("common.remove") }}
                                    </q-btn>
                                </td>
                            </tr>
                        </template>
                    </template>
                </tbody>
            </q-markup-table>

            <q-input
                ref="newTokenTextField"
                v-model="newTokenAddress"
                :label="$t('wallet_settings.token_address')"
                placeholder="mmx1..."
                :rules="[rules.address]"
            ></q-input>

            <q-btn
                :label="$t('wallet_settings.add_token')"
                :disable="newTokenAddress.length == 0 || newTokenTextField.hasError"
                outline
                color="primary"
                @click="handleAddPlotDir(newTokenAddress)"
            />
        </q-card-section>
    </q-card>
</template>

<script setup>
import { useWalletTokens } from "@/queries/wapi";
import { useAddTokens, useRemoveTokens } from "@/queries/api";
import rules from "@/helpers/rules";

const newTokenAddress = ref("");
const newTokenTextField = ref("");

const { data: tokens, isPending, isError } = useWalletTokens();
const loading = computed(() => isPending.value);

const addTokens = useAddTokens();
const handleAddPlotDir = (address) => {
    addTokens.mutate(address, {
        onSuccess: () => {
            newTokenAddress.value = "";
        },
    });
};

const removeTokens = useRemoveTokens();
const handleRemoveToken = (address) => {
    removeTokens.mutate(address);
};
</script>
