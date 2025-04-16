<template>
    <q-card flat>
        <q-card-section>
            <q-input
                v-if="isFarmer"
                ref="farmer_reward_addr_ref"
                v-model="farmer_reward_addr"
                :label="$t('node_settings.farmer_reward_address')"
                :placeholder="$t('common.reward_address_placeholder')"
                :rules="[rules.address]"
                @change="
                    (value) => {
                        if (farmer_reward_addr_ref.validate(value)) {
                            data.farmer_reward_addr = value.length ? value : null;
                        }
                    }
                "
            />

            <q-input
                v-if="isLocalNode"
                ref="timelord_reward_addr_ref"
                v-model="timelord_reward_addr"
                :label="$t('node_settings.timeLord_reward_address')"
                :placeholder="$t('common.reward_address_placeholder')"
                :rules="[rules.address]"
                @change="
                    (value) => {
                        if (timelord_reward_addr_ref.validate(value)) {
                            data.timelord_reward_addr = value.length ? value : null;
                        }
                    }
                "
            />
        </q-card-section>
    </q-card>
</template>

<script setup>
import rules from "@/helpers/rules";

const farmer_reward_addr_ref = ref();
const timelord_reward_addr_ref = ref();

const farmer_reward_addr = ref();
const timelord_reward_addr = ref();

const { data, loading } = useConfigData();

watchEffect(() => (farmer_reward_addr.value = data?.farmer_reward_addr));
watchEffect(() => (timelord_reward_addr.value = data?.timelord_reward_addr));

const { isLocalNode, isFarmer } = useConfigData();
</script>
