import axios from "@/queries/axios";
import { useMutation, useQueryClient, keepPreviousData } from "@tanstack/vue-query";
import { useQueryWrapper as useQuery } from "@/composables/useQueryWrapper";
import {
    ONE_SECOND,
    onError,
    pushSuccess,
    pushError,
    pushSuccessTx,
    pushSuccessAddr,
    getFeeEstimateUseQuery,
} from "../../common";
import i18n from "@/plugins/i18n";
const { t } = i18n.global;

const deployContract = (payload) => axios.post("/wapi/wallet/deploy", payload).then((response) => response.data);
export const useDeployContract = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => deployContract(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet", "contracts"] });
            pushSuccessAddr(result);
        },
        onError: (error) => {
            const message = error.response?.data || error.message;
            pushError({
                message: `${t("common.failed_with")}: ${message}`,
            });
        },
    });
};

const passphraseValidate = (payload) =>
    axios.post("/wapi/passphrase/validate", payload).then((response) => response.data);
export const usePassphraseValidate = () => {
    return useMutation({
        mutationFn: (payload) => passphraseValidate(payload),
        onError: (error) => {
            pushError({
                message: `Wrong passphrase!`, // TODO i18n
            });
        },
    });
};

const walletSend = (payload) => axios.post("/wapi/wallet/send", payload).then((response) => response.data);
export const useWalletSend = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletSend(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletExecute = (payload) => axios.post("/wapi/wallet/execute", payload).then((response) => response.data);
export const useWalletExecute = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletExecute(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletMakeOffer = (payload) => axios.post("/wapi/wallet/make_offer", payload).then((response) => response.data);
export const useWalletMakeOffer = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletMakeOffer(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletOfferWithdraw = (payload) =>
    axios.post("/wapi/wallet/offer_withdraw", payload).then((response) => response.data);
export const useWalletOfferWithdraw = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletOfferWithdraw(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletOfferCancel = (payload) =>
    axios.post("/wapi/wallet/cancel_offer", payload).then((response) => response.data);
export const useWalletOfferCancel = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletOfferCancel(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletOfferTrade = (payload) => axios.post("/wapi/wallet/offer_trade", payload).then((response) => response.data);
export const useWalletOfferTrade = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletOfferTrade(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletOfferAccept = (payload) =>
    axios.post("/wapi/wallet/accept_offer", payload).then((response) => response.data);
export const useWalletOfferAccept = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletOfferAccept(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletSwapTrade = (payload) => axios.post("/wapi/wallet/swap/trade", payload).then((response) => response.data);
export const useWalletSwapTrade = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletSwapTrade(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletPlotnftCreate = (payload) =>
    axios.post("/api/wallet/plotnft_create", payload).then((response) => response.data);
export const useWalletPlotnftCreate = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletPlotnftCreate(payload),
        onSuccess: (result, params) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccess({
                message: "Created new PlotNFT: " + params.name,
            });
        },
        onError,
    });
};

export const useWalletSendFeeEstimate = (params, enabled) =>
    getFeeEstimateUseQuery("send", params, enabled, walletSend);

export const useWalletExecuteFeeEstimate = (params, enabled) =>
    getFeeEstimateUseQuery("execute", params, enabled, walletExecute);

export const useWalletMakeOfferFeeEstimate = (params, enabled) =>
    getFeeEstimateUseQuery("offer", params, enabled, walletMakeOffer);

export const useWalletOfferWithdrawFeeEstimate = (params, enabled) =>
    getFeeEstimateUseQuery("offer_withdraw", params, enabled, walletOfferWithdraw);

export const useWalletOfferCancelFeeEstimate = (params, enabled) =>
    getFeeEstimateUseQuery("cancel_offer", params, enabled, walletOfferCancel);

export const useWalletOfferTradeFeeEstimate = (params, enabled) =>
    getFeeEstimateUseQuery("offer_trade", params, enabled, walletOfferTrade);

export const useWalletOfferAcceptFeeEstimate = (params, enabled) =>
    getFeeEstimateUseQuery("accept_offer", params, enabled, walletOfferAccept);

export const useWalletSwapTradeFeeEstimate = (params, enabled) =>
    getFeeEstimateUseQuery("swap/trade", params, enabled, walletSwapTrade);

const walletSwapPayout = (payload) => axios.post("/wapi/wallet/swap/payout", payload).then((response) => response.data);
export const useWalletSwapPayout = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletSwapPayout(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletSwapSwitchPool = (payload) =>
    axios.post("/wapi/wallet/swap/switch_pool", payload).then((response) => response.data);
export const useWalletSwapSwitchPool = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletSwapSwitchPool(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletSwapAddLiquidity = (payload) =>
    axios.post("/wapi/wallet/swap/add_liquid", payload).then((response) => response.data);
export const useWalletSwapAddLiquidity = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletSwapAddLiquidity(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletSwapRemoveLiquidity = (payload) =>
    axios.post("/wapi/wallet/swap/rem_liquid", payload).then((response) => response.data);
export const useWalletSwapRemoveLiquidity = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletSwapRemoveLiquidity(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletSwapRemoveAllLiquidity = (payload) =>
    axios.post("/wapi/wallet/swap/rem_all_liquid", payload).then((response) => response.data);
export const useWalletSwapRemoveAllLiquidity = () => {
    const queryClient = useQueryClient();
    return useMutation({
        mutationFn: (payload) => walletSwapRemoveAllLiquidity(payload),
        onSuccess: (result) => {
            queryClient.invalidateQueries({ queryKey: ["wallet"] });
            pushSuccessTx(result.id);
        },
        onError,
    });
};

const walletSeed = (params, signal) =>
    axios.get("/wapi/wallet/seed", { params, signal }).then((response) => response.data);
export const useWalletSeed = () => {
    return useMutation({
        mutationFn: (payload) => walletSeed(payload),
        onError,
    });
};
