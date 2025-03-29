import { mnemonicToSeed } from "./mnemonic";

import { getFingerPrintAsync, getFarmerKeyAsync, getAddressAsync, getKeysAsync, signAsync } from "./common/ECDSAUtils";

import { PubKey } from "./common/PubKey";
import { txin_t } from "./common/txio_t";
import { cost_to_fee } from "./common/utils";

import { getChainParams } from "./utils/getChainParams";
import { spend_options_t } from "./common/spend_options_t";
import { Operation, Execute, Deposit } from "./common/Operation";

export class ECDSA_Wallet {
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

    signMsgAsync = async (address, msg, options) => {
        if (address == (await this.getAddressAsync(0))) {
            const keys = await this.getKeysAsync(0);

            const signature = await signAsync(keys.privKey, msg);
            const solution = new PubKey({
                pubkey: keys.pubKey.toHex(),
                signature: signature.toHex(),
            });

            return solution;
        }
        return null;
    };

    signOfAsync = async (tx, options) => {
        if (options.nonce) {
            tx.nonce = options.nonce;
        }

        tx.network = options.network;
        tx.finalize();

        const solution_map = new Map();
        const signMsgExAsync = async (owner) => {
            const iter = solution_map.get(owner);

            if (iter !== undefined) {
                return iter;
            }
            const solution = await this.signMsgAsync(owner, tx.id, options);
            if (solution !== null) {
                const index = tx.solutions.length;
                solution_map.set(owner, index);
                tx.solutions.push(solution);
                return index;
            }
            return -1;
        };

        // sign sender
        if (tx.sender && tx.solutions.length === 0) {
            await signMsgExAsync(tx.sender);
        }

        // sign all inputs
        for (const input of tx.inputs) {
            if (input.solution !== txin_t.NO_SOLUTION) {
                continue;
            }
            let owner = input.address;
            const iter = options.owner_map.get(owner);
            if (iter !== undefined) {
                input.flags |= txin_t.IS_EXEC;
                owner = iter;
            }
            input.solution = await signMsgExAsync(owner);
        }

        // sign all operations
        for (const op of tx.execute) {
            if (op.solution !== Operation.NO_SOLUTION) {
                continue;
            }
            let owner = op.address;
            if (new Operation(op) instanceof Execute) {
                if (op.user) {
                    owner = op.user;
                } else {
                    continue;
                }
            } else {
                const iter = options.owner_map.get(op.address);
                if (iter !== undefined) {
                    owner = iter;
                }
            }
            op.solution = await signMsgExAsync(owner);
        }

        // compute final content hash
        const chainParams = await getChainParams(options.network);
        tx.static_cost = tx.calc_cost(chainParams);
        tx.content_hash = tx.calc_hash(true).toHex();
    };

    completeAsync = async (tx, _options, deposit = new Map()) => {
        const options = new spend_options_t(_options);

        tx.expires = options.expire_at;

        const bigIntMax = (...args) => args.reduce((m, e) => (e > m ? e : m));
        tx.fee_ratio = bigIntMax(BigInt(tx.fee_ratio), BigInt(options.fee_ratio));

        //---
        const missing = [];

        tx.outputs.forEach((output) => {
            missing[output.contract] = (missing[output.contract] ?? 0n) + BigInt(output.amount);
        });

        tx.execute.forEach((deposit) => {
            if (new Operation(deposit) instanceof Deposit) {
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

        const address = await this.getAddressAsync(0);
        Object.entries(missing).forEach(([currency, amount]) => {
            //console.debug("missing", amount, currency);
            if (amount) {
                const input = tx.inputs.find((input) => input.address === address && input.contract === currency);
                if (input) {
                    input.amount += amount;
                } else {
                    const obj = {
                        address: address,
                        contract: currency,
                        amount: amount,
                        memo: options.memo,
                    };

                    const tx_in = new txin_t(obj);
                    tx.inputs.push(tx_in);
                }
            }
        });
        //---

        const chainParams = await getChainParams(options.network);
        const static_cost = tx.calc_cost(chainParams);
        tx.max_fee_amount = cost_to_fee(BigInt(static_cost) + BigInt(options.gas_limit), tx.fee_ratio);

        if (!tx.sender) {
            if (options.sender) {
                tx.sender = options.sender;
            } else {
                tx.sender = address;
            }
        }

        await this.signOfAsync(tx, options);

        this.addTxAuxFields(tx, chainParams);
    };

    addTxAuxFields(tx, chainParams) {
        const fee = cost_to_fee(tx.static_cost, tx.fee_ratio);

        const decimals = chainParams.decimals;
        const feeAmount = fee.toString();
        const feeValue = Number(fee) / 10 ** decimals;

        tx.aux = { decimals, feeAmount, feeValue };
    }
}
