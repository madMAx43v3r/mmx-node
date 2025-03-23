import axios from "@/queries/axios";
import { useMutation, useQueryClient, keepPreviousData } from "@tanstack/vue-query";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";
import { onError, jsonContentTypeHeaders } from "../common";

import i18n from "@/plugins/i18n";
import { Notify } from "quasar";
const { t } = i18n.global;

const transactionValidate = (payload) =>
    axios
        .post("/wapi/transaction/validate", payload, { headers: jsonContentTypeHeaders })
        .then((response) => response.data);
export const useTransactionValidate = () => {
    return useMutation({
        mutationFn: (payload) => transactionValidate(payload),
        onSuccess: (response) => {
            if (response.error) {
                Notify.create({
                    message: response.error.message,
                    type: "negative",
                });
            } else {
                Notify.create({
                    message: "Transaction validated successfully", // TODO i18n
                    type: "positive",
                });
            }
        },
        onError,
    });
};

const transactionBroadcast = (payload) =>
    axios
        .post("/wapi/transaction/broadcast", payload, { headers: jsonContentTypeHeaders })
        .then((response) => response.data);
export const useTransactionBroadcast = () => {
    return useMutation({
        mutationFn: (payload) => transactionBroadcast(payload),
        onSuccess: (response) => {
            if (response.error) {
                Notify.create({
                    message: response.error.message,
                    type: "negative",
                });
            } else {
                Notify.create({
                    message: "Transaction broadcasted successfully", // TODO i18n
                    type: "positive",
                });
            }
        },
        onError,
    });
};
