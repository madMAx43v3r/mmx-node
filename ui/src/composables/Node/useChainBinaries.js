import { useChainInfo } from "@/queries/wapi";

export const useChainBinaries = () => {
    const { rows: chainInfo } = useChainInfo();

    const { t } = useI18n();
    const optionsList = {
        token_binary: {
            name: "Token", // TODO i18n
        },
        offer_binary: {
            name: "Offer",
        },
        swap_binary: {
            name: "Swap",
        },
        nft_binary: {
            name: "NFT",
        },
        plot_nft_binary: {
            name: "Plot NFT",
        },

        // TODO
        // escrow_binary: {},
        // relay_binary: {},
        // time_lock_binary: {},
    };

    const chainBinaries = computed(() => {
        const binaries = Object.entries(chainInfo.value).filter(
            ([key, value]) => key.endsWith("_binary") && typeof value === "string" && value.startsWith("mmx1")
        );

        const result = Object.fromEntries(
            binaries.map(([key, value]) => [
                key,
                {
                    address: value,
                    name: optionsList[key]?.name || key,
                },
            ])
        );

        return result;
    });

    const chainBinariesSwapped = computed(() => {
        return Object.fromEntries(
            Object.entries(chainBinaries.value).map(([key, value]) => [value.address, { ...value, key }])
        );
    });

    const filterInit = computed(() =>
        Object.fromEntries(Object.entries(chainBinariesSwapped.value).map(([key, value]) => [key, true]))
    );

    return { chainBinaries, chainBinariesSwapped, filterInit };
};
