import { mnemonicToSeed } from "./mnemonic";

import { getFingerPrintAsync, getFarmerKeyAsync, getAddressAsync, getKeysAsync, signAsync } from "./common/ECDSAUtils";

import { PubKey } from "./common/PubKey";
import { txin_t } from "./common/txio_t";
import { cost_to_fee } from "./common/utils";

import { getChainParams } from "./utils/getChainParams";

class ECDSA_Wallet {
    #seed_value;
    #passphrase = "";

    constructor(seed, passphrase) {
        let seed_value;
        if (typeof seed === "string") {
            seed_value = mnemonicToSeed(seed);
        } else if (seed instanceof Uint8Array) {
            seed_value = seed;
        } else {
            throw new Error("Invalid seed type");
        }
        this.#seed_value = seed_value;
        this.#passphrase = passphrase ?? "";
    }

    getFingerPrintAsync = async () => getFingerPrintAsync(this.#seed_value, this.#passphrase);

    getFarmerKeyAsync = async () => getFarmerKeyAsync(this.#seed_value);

    getAddressAsync = async (index) => getAddressAsync(this.#seed_value, this.#passphrase, index);

    getKeysAsync = async (index) => getKeysAsync(this.#seed_value, this.#passphrase, index);

    signOf = async (tx, options) => {
        tx.network = options.network;
        tx.finalize();

        const keys = await this.getKeysAsync(0);

        const signature = await signAsync(keys.privKey, tx.id);
        const solution = new PubKey({
            pubkey: keys.pubKey.toHex(),
            signature: signature.toHex(),
        });

        tx.solutions.push(solution);

        const chainParams = await getChainParams(options.network);
        tx.static_cost = tx.calc_cost(chainParams);
        tx.content_hash = tx.calc_hash(true).toHex();
    };

    completeAsync = async (tx, options, deposit = new Map()) => {
        if (!options) {
            throw new Error("options is required");
        }
        if (!options.network) {
            throw new Error("network is required");
        }
        if (!options.expire_at) {
            throw new Error("expire_at is required");
        }

        // set nonce for testing
        if (options.dev) {
            tx.nonce = options.dev.nonce;
        }

        tx.expires = options.expire_at;

        const bigIntMax = (...args) => args.reduce((m, e) => (e > m ? e : m));
        tx.fee_ratio = bigIntMax(BigInt(tx.fee_ratio), BigInt(options.fee_ratio ?? 0n));

        //---
        const missing = [];

        tx.outputs.forEach((output) => {
            missing[output.contract] = (missing[output.contract] ?? 0n) + BigInt(output.amount);
        });

        tx.execute.forEach((deposit) => {
            if (deposit.__type === "mmx.operation.Deposit") {
                missing[deposit.currency] = (missing[deposit.currency] ?? 0n) + BigInt(deposit.amount);
            }
        });

        tx.inputs.forEach((input) => {
            const amount = missing[input.contract];
            if (input.amount && input.amount < amount) {
                missing[input.contract] -= BigInt(input.amount);
            } else {
                missing[input.contract] = 0n;
            }
        });

        deposit.forEach((value, key) => {
            missing[key] = (missing[key] ?? 0n) + BigInt(value);
        });

        Object.keys(missing).forEach((key) => {
            const currency = key;
            const amount = missing[key];
            console.debug("missing", amount, currency);
        });

        const address = await this.getAddressAsync(0);
        tx.outputs.map((out) => {
            const obj = {
                address: address,
                contract: out.contract,
                amount: out.amount,
                memo: out.memo,
                solution: 0,
                flags: 0,
            };

            const tx_in = new txin_t(obj);
            delete tx_in.__type;
            tx.inputs.push(tx_in);
        });
        //---

        const chainParams = await getChainParams(options.network);
        const static_cost = tx.calc_cost(chainParams);
        tx.max_fee_amount = cost_to_fee(BigInt(static_cost) + BigInt(options.gas_limit ?? 5000000), tx.fee_ratio);

        if (!tx.sender) {
            if (options.sender) {
                tx.sender = options.sender;
            } else {
                tx.sender = address;
            }
        }

        await this.signOf(tx, options);

        this.addTxAuxFields(tx, chainParams);
    };

    addTxAuxFields(tx, chainParams) {
        const decimals = chainParams.decimals || 6;
        const feeAmount = Number(cost_to_fee(tx.static_cost, tx.fee_ratio));
        const feeValue = feeAmount / Math.pow(10, decimals);
        tx.aux = { decimals, feeAmount, feeValue };
    }
}
export { ECDSA_Wallet };
