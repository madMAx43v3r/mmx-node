export const ONE_SECOND = 1000;

import router from "@/plugins/router";
import i18n from "@/plugins/i18n";
const { t } = i18n.global;
import { Notify } from "quasar";

export const noCacheHeaders = {
    "Cache-Control": "no-cache, no-store, must-revalidate",
    Pragma: "no-cache",
    Expires: "0",
};

export const jsonContentTypeHeaders = {
    "Content-Type": "application/json",
};

export const pushConfigSuccess = (vars) => {
    var value = vars.value != null ? vars.value : "null";
    Notify.create({
        caption: vars.restart ? t("node_settings.restart_needed") : "", //TODO i18n
        message: `Set '${vars.key}' to '${value}'`, //TODO i18n
        type: "positive",
        group: false,
        classes: vars.restart ? "restart-needed" : "",
    });
};

export const pushSuccess = (vars) => {
    Notify.create({
        type: "positive",
        group: false,
        ...vars,
    });
};

export const pushError = (vars) => {
    Notify.create({
        type: "negative",
        group: false,
        ...vars,
    });
};

export const onError = (error) => {
    const message = error.response?.data || error.message;
    Notify.create({
        message: message,
        type: "negative",
    });
};

const sanitize = (string) => {
    const map = {
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#x27;",
        "/": "&#x2F;",
    };
    const reg = /[&<>"'/]/gi;
    return string.replace(reg, (match) => map[match]);
};

const notifyWithLink = (_prefixText, _href, _linkText, vars) => {
    const prefixText = sanitize(_prefixText);
    const href = sanitize(_href);
    const linkText = sanitize(_linkText);

    Notify.create({
        type: "positive",
        group: false,
        ...vars,
        message: `${prefixText} <a class="text-primary" href="${href}">${linkText}</a>`,
        html: true,
    });
};

export const pushSuccessTx = (txId, vars) => {
    const route = router.resolve({ name: "transaction", params: { transactionId: txId } });
    notifyWithLink(t("common.transaction_has_been_sent"), route.href, txId, vars);
};

export const pushSuccessAddr = (address, vars) => {
    const route = router.resolve({ name: "address", params: { address } });
    notifyWithLink(t("common.deployed_as"), route.href, address, vars);
};

import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";
const disableAutoSend = (params) => ({ ...toValue(params), options: { ...toValue(params).options, auto_send: false } });
const formatFee = (fee) => (fee ? parseFloat(fee.toFixed(6)) : null);
const selectEstimatedFee = (data, enabled) => (enabled.value ? formatFee(data?.exec_result?.total_fee_value) : null);
export const getFeeEstimateUseQuery = (key, params, enabled, queryFn) => {
    return useQuery({
        queryKey: ["wallet", "fee", key, params, enabled],
        queryFn: ({ signal }) => queryFn(disableAutoSend(params), signal),
        select: (data) => selectEstimatedFee(data, enabled),
        enabled,
        staleTime: 0,
        gcTime: 0,
    });
};
